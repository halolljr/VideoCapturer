#include "ffvfilterthread.h"
#include"queue/ffvframequeue.h"
#include"filter/ffvfilter.h"
#include"opencv/fffacedetector.h"
#include"ui/ffcapwindow.h"
#include"capture/ffcapturecontext.h"
#include"clock/ffglobalclock.h"
using namespace FFCaptureContextType;
FFVFilterThread::FFVFilterThread()
{
    overlayX = overlayY = 0;
    overlayWidth = overlayHeght = 400;
    overlayPts = 0;
    overlayDts = 0;
    encoderFlag.store(false);

    //for test
    cameraFlag.store(false);
    videoFlag.store(false);
    screenFlag.store(false);
    pauseFlag.store(false);

    pauseTime = 0;
    lastPauseTime= 0;
}

FFVFilterThread::~FFVFilterThread()
{
    if(faceDetector){
        delete faceDetector;
        faceDetector = nullptr;
    }
    if(overlayProcessor){
        delete overlayProcessor;
        overlayProcessor = nullptr;
    }
    if(cameraFrame){
        av_frame_unref(cameraFrame);
        av_frame_free(&cameraFrame);
    }
    if(cameraFrame2){
        av_frame_unref(cameraFrame2);
        av_frame_free(&cameraFrame2);
    }
    if(lastVideoFrame){
        av_frame_unref(lastVideoFrame);
        av_frame_free(&lastVideoFrame);
    }
    if(videoFrame){
        av_frame_unref(videoFrame);
        av_frame_free(&videoFrame);
    }
    if(screenFrame){
        av_frame_unref(screenFrame);
        av_frame_free(&screenFrame);
    }

}

void FFVFilterThread::init(FFVFrameQueue *frmQueue1_, FFVFrameQueue* frmQueue2_,FFVFrameQueue* frmQueue3_,FFVFrameQueue* renderFrmQueue_,
                           FFVFilter *filter_,FFCapWindow* capWindow_)
{
    frmQueue1 = frmQueue1_;
    frmQueue2 = frmQueue2_;
    frmQueue3 = frmQueue3_;
    renderFrmQueue = renderFrmQueue_;

    filter = filter_;
    capWindow = capWindow_;

    faceDetector = new FFFaceDetector();
    faceDetector->init();

    overlayProcessor = new FFOverlayProcessor();
    overlayProcessor->init();
}

void FFVFilterThread::startEncoder()
{
    std::lock_guard<std::mutex> lock(mutex); // 确保与 run() 中的锁一致
    encoderFlag.store(true);
    cond.notify_one();
}

void FFVFilterThread::stopEncoder()
{
    std::lock_guard<std::mutex> lock(mutex); // 确保与 run() 中的锁一致
    encoderFlag.store(false);
    pauseFlag.store(false);
    pauseTime = 0;
    lastPauseTime = 0;
    cond.notify_one();
}

void FFVFilterThread::openVideoSource(int sourceType)
{
    std::lock_guard<std::mutex>lock(mutex);

    //    std::cerr << "open Video Source" << std::endl;
    enum demuxerType type = static_cast<demuxerType>(sourceType);
    switch (type) {
    case SCREEN:
        screenFlag.store(true);
        break;
    case CAMERA:
        cameraFlag.store(true);
        break;
    case VIDEO:
        videoFlag.store(true);
        eofFlag = false;
        lastVideoTime = 0;
        break;
    default:
        break;
    }

    cond.notify_one();
}

void FFVFilterThread::closeVideoSource(int sourceType)
{
    std::lock_guard<std::mutex>lock(mutex);
    enum demuxerType type = static_cast<demuxerType>(sourceType);

    switch (type) {
    case SCREEN:
        screenFlag.store(false);
        break;
    case CAMERA:
        cameraFlag.store(false);
        break;
    case VIDEO:
        videoFlag.store(false);
        eofFlag = false;
        lastVideoTime = 0;
        break;

    default:
        break;
    }

    cond.notify_one();
}

void FFVFilterThread::setWhiteValue(int value)
{
    if(faceDetector){
        faceDetector->setWhiteValue(value);
    }
}

void FFVFilterThread::setSmoothValue(int value)
{
    if(faceDetector){
        faceDetector->setSmoothValue(value);
    }
}

void FFVFilterThread::pauseEncoder()
{
    std::lock_guard<std::mutex>lock(mutex);
    if(pauseFlag.load()){
        pauseTime +=  av_gettime_relative() - lastPauseTime;
        pauseFlag.store(false);
    }
    else{
        pauseFlag.store(true);
        lastPauseTime = av_gettime_relative();
    }
}

bool FFVFilterThread::peekStart()
{
    return encoderFlag.load();
}

void FFVFilterThread::wakeAllThread()
{
    if(frmQueue1){
        frmQueue1->wakeAllThread();
    }

    if(frmQueue2){
        frmQueue2->wakeAllThread();
    }

    if(frmQueue3){
        frmQueue3->wakeAllThread();
    }

    if(renderFrmQueue){
        renderFrmQueue->wakeAllThread();
    }
}

void FFVFilterThread::run()
{
    while(!m_stop){
        bool hasVideo = videoFlag.load();
        bool hasScreen = screenFlag.load();
        bool hasCamera = cameraFlag.load();
        bool hasEncoder = encoderFlag.load();
        bool hasPause = pauseFlag.load();

        if(!hasVideo && !hasScreen && !hasCamera && !hasEncoder){
            std::unique_lock<std::mutex> lock(mutex);
            cond.wait_for(lock,std::chrono::milliseconds(100));
            continue;
        }

        //        std::cout << "video pause: "<<hasPause << std::endl;
        auto start = av_gettime_relative();

        // 渲染帧
        if(hasVideo){
            if(!eofFlag){
                if(lastVideoTime == 0){
                    lastVideoTime = av_gettime_relative();
                    videoFrame = frmQueue3->dequeue();
                    if(videoFrame== nullptr || videoFrame->data[0] == nullptr){
                        eofFlag = true;
                        continue;
                    }

                    QMetaObject::invokeMethod(capWindow, "sendVideoFrame",
                                              Qt::QueuedConnection,
                                              Q_ARG(AVFrame*, av_frame_clone(videoFrame)));
                    if(lastVideoFrame){
                        av_frame_unref(lastVideoFrame);
                        av_frame_free(&lastVideoFrame);
                    }
                    lastVideoFrame = videoFrame;
                }
                else{
                    int64_t currentVideoTime = av_gettime_relative();
                    if(currentVideoTime - lastVideoTime >= 33000){ //33ms
                        videoFrame = frmQueue3->dequeue();
                        if(videoFrame== nullptr || videoFrame->data[0] == nullptr){
                            eofFlag = true;
                            continue;
                        }

                        QMetaObject::invokeMethod(capWindow, "sendVideoFrame",
                                                  Qt::QueuedConnection,
                                                  Q_ARG(AVFrame*, av_frame_clone(videoFrame)));
                        if(lastVideoFrame){
                            av_frame_unref(lastVideoFrame);
                            av_frame_free(&lastVideoFrame);
                        }
                        lastVideoFrame = videoFrame;


                        lastVideoTime = currentVideoTime;
                    }
                }

            }
        }

        if (hasScreen) {
            screenFrame = frmQueue1->dequeue();
            if(screenFrame != nullptr){
                QMetaObject::invokeMethod(capWindow, "sendScreenFrame",
                                          Qt::QueuedConnection,
                                          Q_ARG(AVFrame*, av_frame_clone(screenFrame)));
            }
        }

        if (hasCamera) {
            cameraFrame = frmQueue2->dequeue();
            if(cameraFrame != nullptr){
                cameraFrame2 = faceDetector->detectFace(cameraFrame);
                QMetaObject::invokeMethod(capWindow, "sendCameraFrame",
                                          Qt::QueuedConnection,
                                          Q_ARG(AVFrame*, av_frame_clone(cameraFrame2)));
            }
        }

        // 叠加顺序:倒序
        overlayNumbers = capWindow->getOverlayNumbers();
        for(auto iter = overlayNumbers.rbegin(); iter != overlayNumbers.rend(); ++iter){
            int type = *iter;
            if(type == -1) continue;

            if(type == CAMERA && cameraFrame2 && hasCamera){
                overlayFrame(av_frame_clone(cameraFrame2), type);
                av_frame_free(&cameraFrame2);
            }
            else if(type == SCREEN && screenFrame && hasScreen){
                overlayFrame(av_frame_clone(screenFrame), type);
                av_frame_free(&screenFrame);
            }
            else if(type == VIDEO && lastVideoFrame && hasVideo){
                videoFrame2 = av_frame_clone(lastVideoFrame);
                overlayFrame(av_frame_clone(videoFrame2), type);
                av_frame_free(&videoFrame2);
            }
        }

        // 编码帧
        if(hasEncoder && !hasPause){
            std::lock_guard<std::mutex>lock(mutex);
            AVFrame* encodeFrame = overlayProcessor->getOverlayFrame();
            auto end = av_gettime_relative();
            //记录处理耗时
            int64_t duration = (end - start) * 10;

            //我们将解码后编码前的视频帧的pts改为现实时间的纳秒级别，然后进行编码，确保音视频同步
            overlayPts = av_gettime_relative() * 10 + duration - pauseTime * 10;
            encodeFrame->pkt_dts = overlayDts;
            encodeFrame->pts = overlayPts;

            filter->sendEncodeFrame(encodeFrame);
            overlayProcessor->resetBackground();
            //            std::cout << "has encoder" << std::endl;
            //            std::cout << "pauseTime:" << pauseTime <<std::endl;
        }
    }

}

void FFVFilterThread::overlayFrame(AVFrame *frame,int type){
    if (frame) {
        capWindow->getOverlayPos(&overlayX, &overlayY, &overlayWidth, &overlayHeght, type);
        overlayProcessor->sendOverlayImage(av_frame_clone(frame), overlayX, overlayY, overlayWidth, overlayHeght);
        av_frame_free(&frame);
    }
}

