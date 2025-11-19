#include "ffvideoadapter.h"

FFVideoAdapter::FFVideoAdapter()
{

}

FFVideoAdapter::~FFVideoAdapter()
{
    if(sws_ctx){
        sws_free_context(&sws_ctx);
    }
}

void FFVideoAdapter::initSws(int width, int height)
{
    if(sws_ctx){
        sws_free_context(&sws_ctx);
    }
    //AV_PIX_FMT_BGR24（BGR24格式，OpenCV默认格式）
    //AV_PIX_FMT_YUV420P（YUV420P格式，我们默认的指定的格式）
    sws_ctx = sws_getContext(
                width, height, AV_PIX_FMT_BGR24,
                width, height, AV_PIX_FMT_YUV420P,
                SWS_FAST_BILINEAR, nullptr, nullptr, nullptr
                );

    lastW = width;
    lastH = height;

    if (!sws_ctx) {
        std::cerr << "Error: Could not initialize SwsContext!" << std::endl;
        return;
    }
}


AVFrame *FFVideoAdapter::convertMatToFrame(const cv::Mat& bgrMat)
{
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        std::cerr << "Error: Could not allocate video frame!" << std::endl;
        return nullptr;
    }
    //宽高以及像素格式
    int width = bgrMat.cols;
    int height = bgrMat.rows;
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = width;
    frame->height = height;

    //分配内存
    int ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        std::cerr << "Error: Could not allocate the video frame data!" << std::endl;
        av_frame_free(&frame);
        return nullptr;
    }

    //对于 BGR 格式，所有通道（B、G、R）都存储在一个连续的内存块中，所以只有一个元素
    int bgrLinesize[1] = { static_cast<int>(bgrMat.step) }; //行大小
    const uint8_t* bgrData[1] = { bgrMat.data };

    if(sws_ctx == nullptr || lastW != width ||lastH != height){
        initSws(width,height);
    }
    sws_scale(
                sws_ctx, bgrData, bgrLinesize, 0, height,
                frame->data, frame->linesize
                );


    return frame;
}

cv::Mat FFVideoAdapter::convertFrameToMat(AVFrame *frame)
{
    int width = frame->width;
    int height = frame->height;
    //原始
    // frame->data[0] (Y分量):
    // ┌─────────────────┐
    // │ Y Y Y Y Y Y Y Y │ ← 第1行
    // │ Y Y Y Y Y Y Y Y │ ← 第2行  
    // │ Y Y Y Y Y Y Y Y │ ← 第3行
    // │ Y Y Y Y Y Y Y Y │ ← 第4行
    // │ ...             │
    // └─────────────────┘
    // 尺寸: width × height
    
    // frame->data[1] (U分量):
    // ┌─────────┐
    // │ U U U U │ ← 第1行
    // │ U U U U │ ← 第2行
    // │ ...     │
    // └─────────┘
    // 尺寸: width/2 × height/2
    
    // frame->data[2] (V分量):
    // ┌─────────┐
    // │ V V V V │ ← 第1行
    // │ V V V V │ ← 第2行
    // │ ...     │
    // └─────────┘
    // 尺寸: width/2 × height/2
    
    //分配一块连续内存，用于存储YUV420P数据
    //一种“打包视图”。它并不是真正意义上的“矩阵行列”，而是把一整帧 YUV 数据（Y + U + V）当作一个连续的二维单通道图像。
    //总高度：height * 3 / 2（Y分量高度 + U/V分量高度）
    //宽度：width,这里对于 U/V 平面部分，虽然每个分量实际宽度只有 width/2，
    //但是 OpenCV 这里为了让整个数据块是“规则矩形”，仍然把整帧视为 width 宽度。
    //数据类型：CV_8UC1（8位无符号单通道）
    cv::Mat yuvMat(height * 3 / 2, width, CV_8UC1);

    //复制 Y 分量（亮度）
    memcpy(yuvMat.data, frame->data[0], width * height);
    //复制 U 分量（色度）
    memcpy(yuvMat.data + width * height, frame->data[1], width * height / 4);
    //复制 V 分量（色度）
    memcpy(yuvMat.data + width * height * 5 / 4, frame->data[2], width * height / 4);

    //颜色空间转换，将YUV420P转换为BGR（AVFrame->Mat）
    cv::Mat bgrMat;
    cv::cvtColor(yuvMat, bgrMat, cv::COLOR_YUV2BGR_I420);

    return bgrMat;
}


