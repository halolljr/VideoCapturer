#include "ffarender.h"
#include"queue/ffaframequeue.h"
#include"globalclock.h"
#include"queue/ffeventqueue.h"
#include"event/ffreadyevent.h"
#include"ui/ffplayerwindow.h"
#include"player/ffplayercontext.h"
#include"event/ffendevent.h"
#include"event/ffprocessevent.h"
#include"sonic/sonic.h"

FFARender::FFARender()
    :readyFlag(false),seekFlag(false),pauseFlag(false),speedFlag(false),speed(1)
{

}

FFARender::~FFARender()
{
    stop();
    close();

    if(playerCtx){
        delete playerCtx;
        playerCtx = nullptr;
    }
    if(abufOut){
        av_freep(&abufOut);
        abufOut = nullptr;
    }
}

void FFARender::close(){
    maxBufSize = -1;
    if(aOutput){
        aOutput->deleteLater();
        aOutput = nullptr;
    }
    if(aPars){
        delete  aPars;
        aPars = nullptr;
    }
    if(abuf){
        av_freep(&abuf);
        abuf = nullptr;
    }
    if(sonicCtx){
        sonicDestroyStream(sonicCtx);
        sonicCtx = nullptr;
    }


    readyFlag = false;
    seekFlag = false;
    pauseFlag = false;
    speedFlag = false;
    speed = 1;
}

void FFARender::setSpeed(float speed_)
{
    speed = speed_;
    speedFlag.store(true,std::memory_order_release);
}

void FFARender::init(FFAFrameQueue *frmQueue_, FFADecoder *aDecoder_,FFPlayerWindow* playerWindow_)
{
    frmQueue = frmQueue_;
    aDecoder = aDecoder_;

    playerCtx = new FFPlayerContext();
    playerCtx->playerWindow = playerWindow_;
    playerCtx->aRender = this;
    playerCtx->aFrmQueue = frmQueue_;
}

void FFARender::stop()
{
    m_stop = true;
    pauseFlag = false;
    pauseCond.notify_all();
}

void FFARender::pause()
{
    bool flag = pauseFlag.load(std::memory_order_acquire);
    pauseFlag.store(!flag,std::memory_order_release);
    if(flag) {
        pauseCond.notify_one();
    }
}

void FFARender::setVolume(double volume)
{
    if(aOutput){
        aOutput->setVolume(volume);
    }

}

bool FFARender::peekReady()
{
    return readyFlag.load(std::memory_order_acquire);
}

void FFARender::wakeAllThread()
{
    pauseCond.notify_all();
}


void FFARender::run()
{
    while(!m_stop){
        AVFrame* frame = frmQueue->dequeue();
        if(frame == nullptr){
            m_stop = true;
            break;
        }
        if(aPars == nullptr){
            initAudio();
        }
        if(!frame->data[0] && !frame->data[1] && !frame->data[2]){
            av_frame_free(&frame);
            //            m_stop = true;
            //            sendEndEvent();
            //            break;
            continue;
        }

        if(pauseFlag.load(std::memory_order_acquire)){
            std::unique_lock<std::mutex>lock(mutex);
            pauseCond.wait(lock);
            pauseFlag.store(false,std::memory_order_release);
        }

        playAudio(frame);

        av_frame_unref(frame);
        av_frame_free(&frame);
    }

}


void FFARender::initAudio()
{
    initAudioPars();
    clockBase = aPars->sampleSize * aPars->nbChannels * aPars->sampleRate;
    aFormat.setCodec("audio/pcm");
    aFormat.setSampleRate(aPars->sampleRate);
    aFormat.setChannelCount(aPars->nbChannels);
    aFormat.setSampleSize(16);
    aFormat.setSampleType(QAudioFormat::SignedInt);
    aFormat.setByteOrder(QAudioFormat::LittleEndian);

    //查看设备是否支持
    deviceInfo = QAudioDeviceInfo::defaultOutputDevice();
    if(!deviceInfo.isFormatSupported(aFormat)){
        aFormat = deviceInfo.nearestFormat(aFormat);
    }

    aOutput = new QAudioOutput(aFormat);
    aOutput->setVolume(1);

    aDevice = aOutput->start();

    //准备事件
    if(!readyFlag.load(std::memory_order_relaxed)){
        totalSec = aDecoder->getToalSec();
        FFEvent* event = new FFReadyEvent(playerCtx,totalSec,-1);
        FFEventQueue::getInstance().enqueue(event);
        //        std::cout<<"audio ready!"<<std::endl;
        readyFlag.store(true,std::memory_order_release);
    }
}

void FFARender::initAudioPars()
{
    FFAudioPars* tmpPars = aDecoder->getAudioPars();
    if(tmpPars == nullptr){
        return;
    }
    aPars = new FFAudioPars();
    memcpy(aPars,tmpPars,sizeof(FFAudioPars));
}

void FFARender::playAudio(AVFrame *frame)
{

    uint8_t* playBuffer = nullptr;

    int64_t bufSize = av_samples_get_buffer_size(
                nullptr,
                aPars->nbChannels,
                frame->nb_samples,
                aPars->aFormatEnum,
                1);

    if(bufSize > maxBufSize){
        maxBufSize = bufSize;
        if(abuf){
            av_freep(&abuf);
        }
        abuf = static_cast<uint8_t*>(av_mallocz(maxBufSize));
        if(!abuf){
            std::cerr << "malloc abuf!" <<std::endl;
            return;
        }
    }

    memcpy(abuf,frame->data[0],bufSize);
    playBuffer = abuf;

    if(speedFlag.load(std::memory_order_acquire)){ //速率改变需要重新创建
        if(sonicCtx != nullptr){
            sonicDestroyStream(sonicCtx);
        }
        sonicCtx = sonicCreateStream(aPars->sampleRate,aPars->nbChannels);
        //设置速率
        sonicSetSpeed(sonicCtx,speed);
        sonicSetPitch(sonicCtx,1.0f);
        sonicSetRate(sonicCtx,1.0f);
        speedFlag.store(false,std::memory_order_acquire);
    }

    if(fabs(speed - 1) >= 0.1){ //变速
//        std::cerr<<"changeSpeed:"<<speed<<std::endl;
        int actual_out_samples = bufSize /
                (aPars->nbChannels * av_get_bytes_per_sample(aPars->aFormatEnum));

        int out_ret = sonicWriteShortToStream(sonicCtx,(int16_t*)abuf,actual_out_samples);
        int num_samples = sonicSamplesAvailable(sonicCtx);

        int out_size = num_samples * av_get_bytes_per_sample(aPars->aFormatEnum) * aPars->nbChannels;
        av_fast_malloc(&abufOut,&abufOutSize,out_size); //复用已经有的内存

        int sonic_samples;
        if(out_ret){
            sonic_samples = sonicReadShortFromStream(sonicCtx,(int16_t*)abufOut,num_samples);
            playBuffer = abufOut;
            bufSize = sonic_samples * av_get_bytes_per_sample(aPars->aFormatEnum) * aPars->nbChannels;
        }
    }

    int64_t globalTime = frame->pts * av_q2d(aPars->timeBase) * 1e3;
//    std::cerr<<"audio frame pts :"<<frame->pts<<std::endl;
    globalClock* gClock = globalClock::getInstance();
    gClock->setClock(globalTime);
    sendProcessEvent((globalTime+500) / 1000);

//    std::cerr<<"audio pts:" << globalTime <<std::endl;
    int bytesWrite = 0;
    while(bytesWrite < bufSize){
        int64_t bytes = aDevice->write((const char*)playBuffer + bytesWrite, bufSize - bytesWrite);

        if(bytes <= 0){
            std::this_thread::sleep_for(std::chrono::microseconds(1000));
            continue;
        }
        bytesWrite += bytes;
    }
}

void FFARender::sendEndEvent()
{
    //    std::cerr<<"arender: sendStopEvent"<<std::endl;
    FFEvent* stopEv = new FFEndEvent(playerCtx);
    FFEventQueue::getInstance().enqueue(stopEv);
}

void FFARender::sendProcessEvent(int curSeconds)
{
    if(curSeconds == lastSec){
        return;
    }
    lastSec = curSeconds;
    FFEvent* processEv = new FFProcessEvent(playerCtx,curSeconds);
    FFEventQueue::getInstance().enqueue(processEv);
}

