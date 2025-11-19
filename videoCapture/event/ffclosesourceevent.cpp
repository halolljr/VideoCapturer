#include "ffclosesourceevent.h"

using namespace FFCaptureContextType;

FFCloseSourceEvent::FFCloseSourceEvent(FFCaptureContext *captureCtx, FFCaptureContextType::demuxerType sourceType_)
    :FFEvent (captureCtx)
{
    index = demuxerIndex[sourceType_];
    sourceType = sourceType_;
}

void FFCloseSourceEvent::work()
{

    close();
    std::cout << "Source Type:" << sourceType<<std::endl;
}

void FFCloseSourceEvent::close()
{
    //关闭解复用线程
    //    std::cout << "demuxer type" << demuxer->getType()<< std::endl;

    //关闭解码线程
    if(sourceType == AUDIO || sourceType == MICROPHONE){
        aDemuxerThread[index]->stop();
        aDemuxerThread[index]->wakeAllThread();

        aDemuxerThread[index]->wait();
        aDemuxerThread[index]->close();

        aDecoderThread[index]->stop();
        aDecoderThread[index]->wakeAllThread();
        aDecoderThread[index]->wait();
        aDecoderThread[index]->close();
    }
    else{
       vDemuxerThread[index]->stop();
       vDemuxerThread[index]->wakeAllThread();

       vDemuxerThread[index]->wait();
       vDemuxerThread[index]->close();

       vDecoderThread[index]->stop();
       vDecoderThread[index]->wakeAllThread();
       vDecoderThread[index]->wait();
       vDecoderThread[index]->close();
    }


    //清空对应的队列
    if(sourceType == AUDIO || sourceType == MICROPHONE){
        aPktQueue[index]->close();
        aFrmQueue[index]->close();
    }
    else{
        vPktQueue[index]->close();
        vFrmQueue[index]->close();
    }

    //设置关闭标志
    if(sourceType == AUDIO || sourceType == MICROPHONE){
        aFilterThread->closeAudioSource(sourceType);
    }
    else{
        vFilterThread->closeVideoSource(sourceType);
    }

}


