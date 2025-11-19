#include "ffstopevent.h"

FFStopEvent::FFStopEvent(FFCaptureContext* captureCtx)
    :FFEvent (captureCtx)
{

}

void FFStopEvent::work()
{
    close();
    clearQueue();
}

void FFStopEvent::close()
{
    //设置编码标志
    vFilterThread->stopEncoder();
    aFilterThread->stopEncoder();

    //关闭编码线程
    vEncoderThread->stop();
    vEncoderThread->wakeAllThread();
    vEncoderThread->wait();
    vEncoderThread->close();

    aEncoderThread->stop();
    aEncoderThread->wakeAllThread();
    aEncoderThread->wait();
    aEncoderThread->close();

    //关闭复用线程
    muxerThread->stop();
    muxerThread->wakeAllThread();
    muxerThread->wait();
    muxerThread->close();


}

void FFStopEvent::clearQueue()
{
    aEncoderPktQueue->close();
    vEncoderPktQueue->close();

    aFilterEncoderFrmQueue->close();
    vFilterEncoderFrmQueue->close();
}
