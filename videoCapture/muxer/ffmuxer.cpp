#include "ffmuxer.h"
#
FFMuxer::FFMuxer()
: headerFlag(false), trailerFlag(false)
{
}

FFMuxer::~FFMuxer() {
    std::lock_guard<std::mutex> lock(mutex);  // 析构函数加锁保护
    if (!headerFlag) {
        writeTrailer();
    }
    if (fmtCtx) {
        avformat_close_input(&fmtCtx);
    }
    url.clear();
}

void FFMuxer::init(const std::string& url_,std::string const& format_) {
    std::lock_guard<std::mutex> lock(mutex);
    url = url_;
    format = format_;
    initMuxer();


}

void FFMuxer::initMuxer() {
//    std::cout << "url = " << url << std::endl;
//    std::cout << "format = "<< format << std::endl;

    //核心api
    int ret = avformat_alloc_output_context2(&fmtCtx, nullptr,format.c_str(), url.c_str());
    if (ret < 0) {
        std::cerr << "Alloc Output Context Fail !" << std::endl;
        printError(ret);
        return;
    }

    if(format == "rtsp"){
        AVDictionary *opts = nullptr;
        //强制走TCP通道
        ret = av_opt_set(&opts,"rtsp_transport","tcp",0);
        ret = av_dict_set(&opts, "stimeout", "5000000", 0);
        if(ret < 0){
            std::cerr << "av_dict_set: stimeout fail" << std::endl;
        }


        
        ret = av_opt_set_dict(fmtCtx, &opts);
        if(ret < 0){
            std::cerr <<"av_opt_set_dict fail" << std::endl;
        }

        av_dict_free(&opts);

    }
    else{
        ret = avio_open(&fmtCtx->pb, url.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            std::cerr << "Open File Fail !" << std::endl;
            printError(ret);
            return;
        }
    }

//    std::cout << "init muxer finish!" << std::endl;
}

void FFMuxer::printError(int ret) {
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE];
    int res = av_strerror(ret, errorBuffer, sizeof(errorBuffer));
    if (res < 0) {
        std::cerr << "Unknow Error!" << std::endl;
    } else {
        std::cerr << "Error:" << errorBuffer << std::endl;
    }
}

void FFMuxer::addStream(AVCodecContext* codecCtx) {
    std::lock_guard<std::mutex> lock(mutex);  // 保护流添加和状态变量

    //核心api
    AVStream* stream = avformat_new_stream(fmtCtx, nullptr);
    if (!stream) {
        std::cerr << "New Stream Fail !" << std::endl;
        return;
    }
    //核心api
    int ret = avcodec_parameters_from_context(stream->codecpar, codecCtx);
    if (ret < 0) {
        std::cerr << "Copy Parameters From Context Fail !" << std::endl;
        printError(ret);
        return;
    }

    //这里设置好时间基，但是当你调用avformat_write_header时，会根据格式要求[可以参考常见的容器时间基]，覆盖掉你设置的时间基。
    stream->time_base = codecCtx->time_base;
    if (codecCtx->codec_type == AVMEDIA_TYPE_AUDIO) {
        aCodecCtx = codecCtx;
        aStream = stream;
        aStreamIndex = stream->index;
    }
    else if (codecCtx->codec_type == AVMEDIA_TYPE_VIDEO) {
        vCodecCtx = codecCtx;
        vStream = stream;
        vStreamIndex = stream->index;
    }

    streamCount++;
//    std::cout << "streamCout = "<< streamCount <<std::endl;
    if (streamCount == 2) {
        readyFlag = true;  // 直接赋值，由锁保证可见性
    }
}

int FFMuxer::mux(AVPacket* packet) {
//    std::cout << "mux!!!!" << std::endl;
    std::lock_guard<std::mutex> lock(mutex);  // 保护整个mux过程
    if (trailerFlag) {
        return 1;
    }

    int streamIndex = packet->stream_index;
    AVRational srcTimeBase, dstTimeBase;

    if (streamIndex == aStreamIndex) {
        srcTimeBase = aCodecCtx->time_base;
        dstTimeBase = aStream->time_base;
    }
    else if (streamIndex == vStreamIndex) {
        srcTimeBase = vCodecCtx->time_base;
        dstTimeBase = vStream->time_base;
    }
    else {
        return -1;  // 无效的流索引
    }

    //要从编码器的时间基转化为容器流的时间基
    packet->pts = av_rescale_q(packet->pts, srcTimeBase, dstTimeBase);
    packet->dts = av_rescale_q(packet->dts, srcTimeBase, dstTimeBase);
    packet->duration = av_rescale_q(packet->duration, srcTimeBase, dstTimeBase);

    if (packet->pts == AV_NOPTS_VALUE || packet->dts == AV_NOPTS_VALUE || packet->pts < 0) {
        av_packet_unref(packet);
        av_packet_free(&packet);
        return 0;
    }

    if(fmtCtx == nullptr){
        return 0;
    }

    int ret = av_write_frame(fmtCtx, packet);

//    int ret = av_interleaved_write_frame(fmtCtx, packet);
    if (ret < 0) {
        std::cerr << "Mux Fail !" << std::endl;
        printError(ret);
        av_packet_unref(packet);
        av_packet_free(&packet);
        return -1;
    }

    av_packet_unref(packet);
    av_packet_free(&packet);
    return 0;
}

void FFMuxer::writeHeader() {
    while (!readyFlag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
//        std::cout << "wait ready flag" << std::endl;
    }
//    std::cout << "write header ready" << std::endl;

    std::lock_guard<std::mutex> lock(mutex);  // 保护头文件写入状态
//    std::cout << "write header ready2" << std::endl;
    //    std::cout << "writeHeader" << std::endl;
    if (headerFlag) {  // 直接判断bool变量
        return;
    }
    headerFlag = true;  // 设置状态后再执行写入

    if(fmtCtx == nullptr){
        std::cerr << "fmtCtx is nullptr" << std::endl;
        return;
    }

    int ret = avformat_write_header(fmtCtx, nullptr);
    if (ret < 0) {
        std::cerr << "Write Header Fail !" << std::endl;
        printError(ret);
        headerFlag = false;  // 恢复状态以防后续错误
    }

//    std::cout << "headerFlag :" << headerFlag << std::endl;
}

void FFMuxer::writeTrailer() {
    std::lock_guard<std::mutex> lock(mutex);  // 保护尾文件写入状态
    if (trailerFlag) {  // 直接判断bool变量
        return;
    }
    trailerFlag = true;  // 设置状态后再执行写入

    if(fmtCtx == nullptr){
        return;
    }
    int ret = av_write_trailer(fmtCtx);
    if (ret < 0) {
        std::cerr << "Write Trailer Fail !" << std::endl;
        printError(ret);
        trailerFlag = false;  // 恢复状态以防后续错误
    }
}

int FFMuxer::getAStreamIndex() {
    std::lock_guard<std::mutex>lock(mutex);
    return aStreamIndex;
}

int FFMuxer::getVStreamIndex() {
    std::lock_guard<std::mutex>lock(mutex);
    return vStreamIndex;
}

void FFMuxer::close()
{
    std::lock_guard<std::mutex>lock(mutex);
    if(fmtCtx){
        if (fmtCtx->pb) avio_closep(&fmtCtx->pb);
        avformat_free_context(fmtCtx); // 释放输出上下文
        fmtCtx = nullptr;
    }
    if(!headerFlag){
        writeTrailer();
    }
    url.clear();
    format.clear();

    headerFlag = false;
    trailerFlag = false;
    readyFlag = false;

    aStreamIndex = vStreamIndex = -1;
    streamCount = 0;

    aStream = nullptr;
    vStream = nullptr;

    aCodecCtx = nullptr;
    vCodecCtx = nullptr;

}


