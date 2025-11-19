#include "ffvencoder.h"
#include"queue/ffvframequeue.h"
#include"queue/ffvpacketqueue.h"

FFVEncoder::FFVEncoder()
{

}

FFVEncoder::~FFVEncoder()
{
    close();

}

void FFVEncoder::init(FFVPacketQueue *pktQueue_){
    pktQueue = pktQueue_;
}

void FFVEncoder::close()
{
    std::lock_guard<std::mutex>lock(mutex);
    if(codecCtx){
        avcodec_free_context(&codecCtx);
        codecCtx = nullptr;
    }
    if(vPars){
        delete vPars;
        vPars = nullptr;
    }

    lastPts = -1;

}

void FFVEncoder::wakeAllThread()
{
    if(pktQueue){
        pktQueue->wakeAllThread();
    }
}

void FFVEncoder::initVideo(AVFrame *frame,AVRational fps)
{
    std::lock_guard<std::mutex>lock(mutex);
    vPars = new FFVEncoderPars();
    vPars->biteRate = 2 * 1024 * 1024; //2M
    vPars->width = frame->width;    //一帧的宽
    vPars->height = frame->height;  //一帧的高
    vPars->videoFmt = AV_PIX_FMT_YUV420P;   //像素格式,因为我们
    vPars->frameRate = fps; //帧率

#if 0
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
#else
    const AVCodec* codec = avcodec_find_encoder_by_name("h264_nvenc"); //只有英伟达显卡
#endif
    if(codec == nullptr){
        std::cerr << "Find H264 Codec Fail !" << std::endl;
        return;
    }

    codecCtx = avcodec_alloc_context3(codec);
    if(codecCtx == nullptr){
        std::cerr << "Alloc CodecCtx Fail !" << std::endl;
        return;
    }

    codecCtx->width = vPars->width; //宽
    codecCtx->height = vPars->height;   //高
    codecCtx->framerate = vPars->frameRate; //帧率
    codecCtx->time_base = AVRational{vPars->frameRate.den,vPars->frameRate.num};    //时间基，num：分子,den：分母
    codecCtx->pix_fmt = vPars->videoFmt;    //像素格式
/*  1. 在 H.264、AAC 等编码格式中，编码输出的码流通常分两部分：
    全局头（Global Header）：
        包含编解码器需要的解码参数，比如 H.264 里的 SPS/PPS、AAC 里的 AudioSpecificConfig。
    通常在码流开头，且所有帧都会共用同一个全局头。
    每帧数据（Frame Data）：
        真正的音视频帧数据，比如 H.264 的 IDR/P/B 帧，AAC 的 ADTS 帧。
    2. AV_CODEC_FLAG_GLOBAL_HEADER 的作用:
    编码器生成的全局头不会写在每个编码帧里，而是单独提供。
    编码后的输出数据里只有原始帧数据，解码器需要在别处获取全局头。
    常用于 MP4、FLV、MKV 这种容器格式，因为容器本身有存放全局头的位置，比如MP4 封装时，codecContext->extradata 会写进 moov → avcC box，播放器解码时就知道全局头信息。
    如果不设置这个标志，编码器可能会在每个关键帧前都加上 SPS/PPS，导致码流略大
*/
    codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; //和ffaencoder一样，可以查看这个的意思

    codecCtx->max_b_frames = 0;  // 禁用B帧
    codecCtx->gop_size = 12;     // 合理关键帧间隔
    codecCtx->keyint_min = 12;   // 最小关键帧间隔
    codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;  // 低延迟模式
#if 0
/*
FF_THREAD_FRAME：表示按帧多线程编码，每个线程处理不同的帧。

优点：可以显著提升编码速度。

缺点：可能会稍微增加延迟，因为一帧要等所有线程完成。

thread_count：设置线程数量，通常和 CPU 核心数相当，比如 8 核 CPU 就设置 8。

实时推流场景一般会用 FF_THREAD_SLICE（按片编码）来降低延迟，或者减少 thread_count。
*/
    codecCtx->thread_type = FF_THREAD_FRAME;
    codecCtx->thread_count = 8;
#endif

    // 配置编码器参数
    AVDictionary* codec_options = nullptr;
#if 1   //开启硬编码的设置
/*
usage = 3
这是硬件编码器的预设，数值越高通常表示更快或更低延迟。

比如在 AMD AMF 中，usage=3 可能表示低延迟模式。
max_b_frames = 0

禁用 B 帧，只使用 I/P 帧，降低延迟。
B 帧会导致解码端需要缓存更多帧，增加延迟。

latency = true
开启低延迟模式，某些硬件编码器提供这个参数
*/
    av_dict_set(&codec_options, "usage", "3", 0);
    av_dict_set(&codec_options,"max_b_frames","0",0);
    av_dict_set(&codec_options,"latency","true",0);
#else
/*
tune=zerolatency
专门针对实时推流，关闭编码器内部缓冲，让每帧尽快输出。
会减少码率压缩效率，但降低延迟。

preset=ultrafast
x264/x265 提供的速度/压缩比平衡预设，从 ultrafast 到 veryslow 共 10 多个档位。
越快，压缩效率越低，但编码速度越快，延迟越低
*/
    av_dict_set(&codec_options, "tune", "zerolatency", 0);
    av_dict_set(&codec_options, "preset", "ultrafast", 0);
#endif
    int ret = avcodec_open2(codecCtx,codec,&codec_options);
    if(ret < 0){

        printError(ret);
        return;
    }
    av_dict_free(&codec_options);
    // 添加验证代码
    std::cout << "编码器名称: " << codec->name << std::endl;
    std::cout << "编码器长名称: " << codec->long_name << std::endl;
    std::cout << "硬件编码器: " << (strstr(codec->name, "nvenc") ? "是" : "否") << std::endl;

    // 检查编码器能力
    if (codec->capabilities & AV_CODEC_CAP_HARDWARE) {
        std::cout << "支持硬件加速: 是" << std::endl;
}
}

int FFVEncoder::encode(AVFrame *frame, int streamIndex, int64_t pts, AVRational timeBase)
{
    std::lock_guard<std::mutex>lock(mutex);

    if(frame == nullptr || codecCtx == nullptr) {
        std::cout << "nullptr" << std::endl;
        return 0;
    }
    //帧数据已经经过了过滤器处理，所以采用滤镜的原时间基
    //进行pts转化的目的是：将帧数据的时间基转化为编码器的时间基，保持实际时间不变，确保时间戳的连续性，使其正常音视频同步
    pts = av_rescale_q(pts,timeBase,codecCtx->time_base);
    if(pts <= lastPts){ //修正
        pts = lastPts + 1;
    }
    lastPts = pts;

//    std::cout<<"rescale pts:"<<pts<<std::endl;
    frame->pts = pts;


    int ret = avcodec_send_frame(codecCtx,frame);
    if(ret < 0){
        printError(ret);
        return -1;
    }


    while(1){
        AVPacket* pkt = av_packet_alloc();
        // GPU编码器的工作流程：
        //- 接收CPU内存中的原始帧数据
        //- 在GPU上进行H.264编码计算
        //- 自动将编码结果拷贝回CPU内存
        //- 返回标准的AVPacket结构
        ret = avcodec_receive_packet(codecCtx,pkt);
        if(ret == AVERROR(EAGAIN)){
            av_packet_free(&pkt);

            //            std::cerr<<"Encode Video AVERROR(EAGAIN)" << std::endl;
            break;
        }
        else if(ret == AVERROR_EOF){
            std::cout << "Encode Video EOF !" << std::endl;
            av_packet_free(&pkt);
            break;
        }
        else if(ret < 0){
            //            std::cerr << "Encode Video Frame Fail !" << std::endl;
            printError(ret);
            av_packet_free(&pkt);
            av_frame_unref(frame);
            av_frame_free(&frame);
            return -1;
        }
        else{
            pkt->stream_index = streamIndex;
//            std::cerr<<"enqueue pkt !"<<std::endl;
            //            std::cout<<"video packet pts:"<<pkt->pts<<std::endl;
            //            std::cout<<"video packet dts:"<<pkt->dts<<std::endl;
            //            std::cout<<"video packet duration:"<<pkt->duration<<std::endl;
            pktQueue->enqueue(pkt);
            av_packet_free(&pkt);
        }
    }


    return 0;
}

AVCodecContext *FFVEncoder::getCodecCtx()
{
//    std::lock_guard<std::mutex>lock(mutex);
    return codecCtx;
}

FFVEncoderPars *FFVEncoder::getEncoderPars()
{
//    std::lock_guard<std::mutex>lock(mutex);
    return vPars;
}

void FFVEncoder::printError(int ret)
{
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE];
    int res = av_strerror(ret,errorBuffer,sizeof errorBuffer);
    if(res < 0){
        std::cerr << "Unknow Error!" << std::endl;
    }
    else{
        std::cerr << "Error:" << errorBuffer << std::endl;
    }
}
