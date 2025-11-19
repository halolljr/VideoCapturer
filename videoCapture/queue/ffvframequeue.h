#ifndef FFVFRAMEQUEUE_H
#define FFVFRAMEQUEUE_H

#include <iostream>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <atomic>

extern "C" {
#include <libavformat/avformat.h>
}

class FFVFrameQueue {
public:
    explicit FFVFrameQueue();
    ~FFVFrameQueue();

    void enqueue(AVFrame* srcFrame);
    AVFrame* dequeue();

    void wakeAllThread();
    void clearQueue();
    void enqueueNull();
    void flushQueue();
    void close();
    void start();
    bool peekEmpty();

private:
    AVFrame* peekQueue();
private:
    std::queue<AVFrame*> frmQueue;
    std::mutex mutex;
    std::condition_variable cond;
    std::atomic<bool> m_stop;

};

#endif // FFVFRAMEQUEUE_H
