#include "ffaencoder.h"
#include"queue/ffaframequeue.h"
#include"queue/ffapacketqueue.h"

FFAEncoder::FFAEncoder()
{

}

FFAEncoder::~FFAEncoder()
{
    close();
}

void FFAEncoder::init(FFAPacketQueue *pktQueue_){
    pktQueue = pktQueue_;
}

void FFAEncoder::close()
{
    std::lock_guard<std::mutex>lock(mutex);
    if(codecCtx){
        avcodec_free_context(&codecCtx);
        codecCtx = nullptr;
    }
    if(aPars){
        delete aPars;
        aPars = nullptr;
    }

    clearPendingFrame();
}

void FFAEncoder::wakeAllThread()
{
    if(pktQueue){
        pktQueue->wakeAllThread();
    }
}

void FFAEncoder::initAudio(AVFrame *frame)
{
    std::lock_guard<std::mutex>lock(mutex);

    aPars = new FFAEncoderPars();
    aPars->biteRate = 64 * 1024; //64k，比特率
    aPars->nbChannel = frame->ch_layout.nb_channels;    //通道个数
    aPars->sampleRate = frame->sample_rate; //采样率
    aPars->audioFmt = AV_SAMPLE_FMT_FLTP;   //采样格式，AAC编码器的标准输入格式

    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);   //AAC
    if(codec == nullptr){
        std::cerr << "Find AAC Codec Fail !" << std::endl;
        return;
    }

    codecCtx = avcodec_alloc_context3(codec);
    if(codecCtx == nullptr){
        std::cerr << "Alloc CodecCtx Fail !" << std::endl;
        return;
    }

    codecCtx->bit_rate = aPars->biteRate;   //目标比特率
    codecCtx->ch_layout.nb_channels = aPars->nbChannel; //目标通道个数
    av_channel_layout_default(&codecCtx->ch_layout,aPars->nbChannel);   //目标通道格式
    codecCtx->sample_rate = aPars->sampleRate;  //目标采样率
    codecCtx->sample_fmt = aPars->audioFmt; //目标采样格式
    codecCtx->time_base = AVRational{1 ,aPars->sampleRate}; //时间基准
    //- 将编码器特定信息放在文件头部而非每个包中
    //- 对于 MP4 等容器格式是必需的
    //- 提高兼容性和播放效率
    codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; //全局头标志

    int ret = avcodec_open2(codecCtx,codec,nullptr);
    if(ret < 0){
        printError(ret);
        return;
    }
}
//此函数将内部缓存的音频数据 (pendingFrame) 转换为标准的 FFmpeg AVFrame 格式，用于：
//- 当缓存中积累了足够的音频数据时
//- 需要将缓存数据送入编码器进行处理
//- 在程序结束时处理剩余的不完整音频帧
AVFrame* FFAEncoder::createFrameFromPending() {
    AVFrame* frame = av_frame_alloc();
    frame->format = codecCtx->sample_fmt;   //编码器的采样格式
    frame->ch_layout =  codecCtx->ch_layout;    //编码器的通道布局
    frame->ch_layout.nb_channels = codecCtx->ch_layout.nb_channels; //编码器的通道个数
    frame->sample_rate = codecCtx->sample_rate; //编码器的采样率
    frame->nb_samples = pendingFrame.samples;   //缓存中的样本数量
    frame->pts = pendingFrame.next_pts; //显示时间戳pts

    av_frame_get_buffer(frame, 0);

    //- ch: 当前处理的通道索引(0=左声道, 1=右声道)
    //- frame->data[ch]: 目标 AVFrame 中第 ch 个通道的数据指针
    //- pendingFrame.data[ch].data(): 源缓存中第 ch 个通道的数据
    //- 复制长度：样本数量 × 每样本字节数(对于 float 是 4 字节)
    for (int ch = 0; ch < frame->ch_layout.nb_channels; ++ch) {
        memcpy(frame->data[ch], pendingFrame.data[ch].data(), pendingFrame.samples * av_get_bytes_per_sample(codecCtx->sample_fmt));
    }
    return frame;
}

void FFAEncoder::clearPendingFrame()
{
    pendingFrame.next_pts = 0;
    pendingFrame.samples = 0;
    for (auto& channel : pendingFrame.data) {
        channel.clear();
    }
}

int FFAEncoder::encode(AVFrame *frame, int streamIndex, int64_t pts, AVRational timeBase)
{
    //参考可以使用AVFifo队列去实现，这样子就不用计算缓存帧和多次的内存拷贝了。
    std::lock_guard<std::mutex>lock(mutex);

    if(frame == nullptr || codecCtx == nullptr) {
        std::cout << "nullptr" << std::endl;
        return 0;
    }

    int frame_size = codecCtx->frame_size;  //一般是编码器固定的，比如AAC是固定1024个采样点为一帧
    int input_samples = frame->nb_samples;  //输入的音频帧大小可能不一致，得到输入的帧的采样点个数
    int bytes_per_sample = av_get_bytes_per_sample(codecCtx->sample_fmt);
    //将基于timebase(bq)的pts(a)转化为基于codecCtx->time_base(cq)的pts(d)，保持实际时间不变
    //计算过程:a*bq=d*cq,原理是保证实际时间不变
    //int64_t initial_pts = av_rescale_q(pts, timeBase, codecCtx->time_base);

    //将之前缓存的不完整数据与新数据合并
    std::vector<uint8_t> merged_data[8];
    int total_samples = pendingFrame.samples + input_samples;

    // 1. 将缓存数据复制到合并缓冲区
    for (int ch = 0; ch < codecCtx->ch_layout.nb_channels; ++ch) {
        merged_data[ch].resize(total_samples * bytes_per_sample);
        //先把缓存的数据复制
        if (pendingFrame.samples > 0) {
            memcpy(merged_data[ch].data(),
                   pendingFrame.data[ch].data(),
                   pendingFrame.samples * bytes_per_sample);
        }
        // 2. 追加新数据
        memcpy(merged_data[ch].data() + pendingFrame.samples * bytes_per_sample,
               frame->data[ch],
               input_samples * bytes_per_sample);
    }

    // 计算能生成多少完整帧
    int total_full_frames = total_samples / frame_size;
    int remaining_samples = total_samples % frame_size;

    // 3. 处理完整帧
    for (int i = 0; i < total_full_frames; ++i) {
        AVFrame* sub_frame = av_frame_alloc();
        sub_frame->format = codecCtx->sample_fmt;   //采样格式
        sub_frame->ch_layout = codecCtx->ch_layout; //通道布局
        sub_frame->ch_layout.nb_channels = codecCtx->ch_layout.nb_channels; //通道个数
        sub_frame->sample_rate = codecCtx->sample_rate; //采样率
        sub_frame->nb_samples = frame_size; //编码器要求的采样点个数
        sub_frame->pts = pendingFrame.next_pts + i * frame_size;    //pts，要注意未编码前的帧的pts和编码得到的包的pts

        av_frame_get_buffer(sub_frame, 0);

        // 复制数据到子帧
        //将一帧的通道数据都复制
        for (int ch = 0; ch < sub_frame->ch_layout.nb_channels; ++ch) {
            uint8_t* src = merged_data[ch].data() + i * frame_size * bytes_per_sample;
            memcpy(sub_frame->data[ch], src, frame_size * bytes_per_sample);
        }

        // 发送到编码器
        avcodec_send_frame(codecCtx, sub_frame);
        av_frame_free(&sub_frame);

        // 接收编码后的包
        while (1) {
            AVPacket* pkt = av_packet_alloc();
            int ret = avcodec_receive_packet(codecCtx, pkt);

            if (ret == AVERROR(EAGAIN)) {
                av_packet_free(&pkt);
                break;
            } else if (ret < 0) {
                av_packet_free(&pkt);
                return -1;
            }

            pkt->stream_index = streamIndex;
            pktQueue->enqueue(pkt);
            av_packet_free(&pkt);
        }
    }

    // 4. 更新缓存
    //再从合并的数据取回剩余的
    pendingFrame.next_pts = pendingFrame.next_pts + total_full_frames * frame_size;
    pendingFrame.samples = remaining_samples;
    for (int ch = 0; ch < codecCtx->ch_layout.nb_channels; ++ch) {
        pendingFrame.data[ch].resize(remaining_samples * bytes_per_sample);
        if (remaining_samples > 0) {
            uint8_t* src = merged_data[ch].data() + total_full_frames * frame_size * bytes_per_sample;
            memcpy(pendingFrame.data[ch].data(), src, remaining_samples * bytes_per_sample);
        }
    }


    return 0;
}

FFAEncoderPars* FFAEncoder::getEncoderPars()
{
    std::lock_guard<std::mutex>lock(mutex);
    return aPars;
}

AVCodecContext *FFAEncoder::getCodecCtx()
{
    //这里加锁会死锁
//    std::lock_guard<std::mutex>lock(mutex);
    return codecCtx;
}


void FFAEncoder::printError(int ret)
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
