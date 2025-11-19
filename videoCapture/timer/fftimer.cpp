#include"fftimer.h"
#include"queue/ffvframequeue.h"
#include"decoder/ffvdecoder.h"
#include"queue/ffeventqueue.h"
#include"player/ffplayercontext.h"
#include"clock/ffglobalclock.h"
#include"ui/ffcapwindow.h"
#include"render/ffvrender.h"

#define FRMAE_RATE 30

FFTimer::FFTimer()
    :m_stop(false),seekFlag(false),pauseFlag(false),speedFlag(false),speed(1.0f),speedFactor(1.0f)
{
    readyFlag = true;
}
void FFTimer::init(FFVFrameQueue *frmQueue_, FFVRender *vRender_, FFCapWindow *capWindow_)
{
    frmQueue = frmQueue_;
    vRender = vRender_;
    capWindow = capWindow_;
}


void FFTimer::start() {
    m_stop = false;
    timerTread = std::thread(&FFTimer::work,this);
}

void FFTimer::wait()
{
    if (timerTread.joinable()){
        timerTread.join();
        std::cerr<<"timer thread has joined!"<<std::endl;
    }

}

void FFTimer::stop() {
    m_stop = true;
    pauseFlag = false;
    cond.notify_all();
    pauseCond.notify_all();
}


void FFTimer::pause()
{
    bool flag = pauseFlag.load(std::memory_order_acquire);
    pauseFlag.store(!flag,std::memory_order_release);
    if(flag) {
        pauseCond.notify_one();
    }
}


void FFTimer::close()
{
    seekFlag = false;
    pauseFlag = false;
    speedFlag = false;
    speed = 1.0f;
    speedFactor = 1.0f;
}

void FFTimer::wakeAllThread()
{
    pauseCond.notify_all();
    cond.notify_all();
    readyFlag.store(true,std::memory_order_release);
}

void FFTimer::setSpeed(float speed_)
{
    speedFlag.store(true,std::memory_order_release);
    speed = speed_;
}

bool FFTimer::peekReadyFlag()
{
    return readyFlag.load(std::memory_order_acquire);
}


void FFTimer::setTimerInterval(std::chrono::milliseconds interval_)
{
    interval = interval_;
}

void FFTimer::setTimerInterval(double interval_)
{
    interval = std::chrono::microseconds(static_cast<int64_t>(interval_));
}

void FFTimer::setTimerInterval(int64_t interval_)
{
    this->interval = std::chrono::microseconds(interval_);
}

void FFTimer::playVideo()
{

    AVFrame* frame = frmQueue->dequeue();
    if (frame == nullptr || frame->data[0] == nullptr){
        stop();
        return;
    }

    QMetaObject::invokeMethod(
                capWindow, "sendVideoFrame",
                Qt::QueuedConnection,
                Q_ARG(AVFrame*,frame)
                );

}


FFTimer::~FFTimer() {
    stop();
    close();
}

void FFTimer::work()
{
    while (!m_stop) {
        playVideo();
        readyFlag.store(false,std::memory_order_release);
        //假设为30fps
        double duration = 1e6 / FRMAE_RATE;
        av_usleep(static_cast<uint>(duration));
        readyFlag.store(true,std::memory_order_release);

    }
}

void FFTimer::copyYUV(AVFrame *frame)
{
    yBuf = new uint8_t[frame->width * frame->height];
    uBuf = new uint8_t[frame->width * frame->height /4];
    vBuf = new uint8_t[frame->width * frame->height /4];
    for (int i = 0; i < frame->height; i++) {
        memcpy(yBuf + i * frame->width ,
               frame->data[0] + i * frame->linesize[0],
                frame->width);
    }
    for (int i = 0; i < frame->height / 2; i++) {
        memcpy(uBuf + i * frame->width / 2,
               frame->data[1] + i * frame->linesize[1],
                frame->width / 2);
        memcpy(vBuf + i *  frame->width / 2,
               frame->data[2] + i * frame->linesize[2],
                frame->width / 2);
    }
}


