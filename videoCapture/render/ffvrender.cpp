
#include "ffvrender.h"
#include"timer/fftimer.h"
#include"queue/ffvframequeue.h"
#include<QMetaObject>
#include<iomanip>
#include"event/ffreadyevent.h"
#include"queue/ffeventqueue.h"
#include"player/ffplayercontext.h"
#include"event/ffendevent.h"
#include"ui/ffcapwindow.h"

#define NOSYNC_THRESHOLD 10000

FFVRender::FFVRender()
{

}

FFVRender::~FFVRender()
{
    if(timer){
        delete timer;
        timer = nullptr;
    }
}

void FFVRender::init(FFVFrameQueue *frmQueue_,FFCapWindow* capWindow_)
{
    frmQueue = frmQueue_;
    capWindow = capWindow_;
    initTimer();
}


void FFVRender::start()
{
    if(timer)
        timer->start();
}

void FFVRender::stop()
{
    if(timer)
        timer->stop();
}

void FFVRender::pause()
{
    if(timer)
        timer->pause();
}

void FFVRender::wait()
{
    if(timer)
        timer->wait();
}

void FFVRender::close()
{
    if(timer){
        timer->close();
    }
}

void FFVRender::setSpeed(float speed)
{
    if(timer)
        timer->setSpeed(speed);
}

void FFVRender::wakeAllThread()
{
    if(timer){
        timer->wakeAllThread();
    }
}

bool FFVRender::peekReadyFlag()
{
    if(timer){
        return timer->peekReadyFlag();
    }

    return false;
}

void FFVRender::initTimer()
{
    timer = new FFTimer();
    timer->init(frmQueue,this,capWindow);
}




