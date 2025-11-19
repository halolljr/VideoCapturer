#include "ffeventqueue.h"
#include "event/ffevent.h"

FFEventQueue::~FFEventQueue() {
    std::lock_guard<std::mutex> lock(mutex);
    m_stop.store(true);
    cond.notify_all();
    while (!evQueue.empty()) {
        FFEvent* event = evQueue.front();
        evQueue.pop();
        delete event;
    }
}

FFEventQueue &FFEventQueue::getInstance() {
    static FFEventQueue instance;
    return instance;
}

void FFEventQueue::enqueue(FFEvent* event) {
    std::lock_guard<std::mutex> lock(mutex);
    evQueue.emplace(event);
//    std::cout<<"enqueue event!"<<std::endl;
    cond.notify_one();
}

FFEvent* FFEventQueue::dequeue() {
    std::unique_lock<std::mutex> lock(mutex);
    cond.wait(lock, [this]() { return !evQueue.empty() || m_stop.load(); });
    if (m_stop.load()) {
        return nullptr;
    }

    FFEvent* event = evQueue.front();
//    std::cout<<"dequeue event!"<<std::endl;
    evQueue.pop();
    return event;
}

void FFEventQueue::clearQueue()
{
    std::lock_guard<std::mutex> lock(mutex);
    while (!evQueue.empty()) {
        FFEvent* event = evQueue.front();
        evQueue.pop();
        delete event;
    }
}

void FFEventQueue::wakeAllThread() {
    std::lock_guard<std::mutex> lock(mutex);
    m_stop.store(true);
    cond.notify_all();
}
