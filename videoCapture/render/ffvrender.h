#ifndef FFVRENDER_H
#define FFVRENDER_H

#include<chrono>
#include<atomic>
extern "C"{
#include"libavformat/avformat.h"
}

class FFTimer;
class FFVFrameQueue;
class FFPlayerContext;
class FFCapWindow;

class FFVRender
{
public:
    FFVRender();
    ~FFVRender();

    void init(FFVFrameQueue* frmQueue_,FFCapWindow* capWindow_);
    void start();
    void stop();
    void pause();
    void wait();
    void close();
    void setSpeed(float speed);

    void wakeAllThread();
    bool peekReadyFlag();
private:
    void initTimer();

private:
    FFVFrameQueue* frmQueue = nullptr;

    FFTimer* timer = nullptr;
    FFCapWindow* capWindow = nullptr;
};

#endif // FFVRENDER_H
