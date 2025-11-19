#ifndef FFEVENTLOOP_H
#define FFEVENTLOOP_H

class FFEventQueue;
class FFThreadPool;

#include<thread>
#include<atomic>

class FFEventLoop {
public:
    explicit FFEventLoop();
    ~FFEventLoop();
    void init(FFEventQueue* evQueue_, FFThreadPool* threPool_);
    void start();
    void stop();
    void wait();

    void wakeAllThread();
private:
    void work();

private:
    FFEventQueue* evQueue;
    FFThreadPool* threPool;

    std::thread loopThread;
    std::atomic<bool> m_stop;

};


#endif // EVENT_LOOP_HPP
