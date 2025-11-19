#include "ffoverlayprocessor.h"

FFOverlayProcessor::FFOverlayProcessor()
{
    if(adapter){
        delete adapter;
        adapter = nullptr;
    }
}

void FFOverlayProcessor::init()
{
    adapter = new FFVideoAdapter();
    background = cv::Mat::zeros(1080,1920,CV_8UC3);
}

void FFOverlayProcessor::sendOverlayImage(AVFrame *frame, int x, int y, int w, int h)
{
    std::lock_guard<std::mutex>lock(mutex);
    if(frame == nullptr){
        std::cout << "nullptr" << std::endl;
        return;
    }
    //将AVFrame转换为cv::Mat
    cv::Mat mat = adapter->convertFrameToMat(frame);
    
    overlayImage(mat,x,y,w,h);
    av_frame_unref(frame);
    av_frame_free(&frame);
}

void FFOverlayProcessor::sendOverlayImage(cv::Mat &mat, int x, int y, int w, int h)
{
    overlayImage(mat,x,y,w,h);
}

void FFOverlayProcessor::resetBackground()
{
    background.release();
    background = cv::Mat::zeros(1080,1920,CV_8UC3);
}

AVFrame *FFOverlayProcessor::getOverlayFrame()
{
     std::lock_guard<std::mutex>lock(mutex);
    AVFrame* frame = adapter->convertMatToFrame(background.clone());
    return frame;
}

//将转化后的帧叠加到画布上（这个画布很大，会有很多帧叠加在上面）
void FFOverlayProcessor::overlayImage(cv::Mat &foreground, int x, int y, int w, int h)
{

    // 确定叠加区域的边界（基于调整后的大小），这是针对显示区域而言的
    int x1 = std::max(x, 0);
    int y1 = std::max(y, 0);
    int x2 = std::min(x + w, background.cols);
    int y2 = std::min(y + h, background.rows);

    // 检查叠加区域是否有效
    if (x1 >= x2 || y1 >= y2) return;

    // 首先将前景图像调整到指定大小
    cv::Mat resized_foreground;
    cv::resize(foreground, resized_foreground, cv::Size(w, h));

    // 计算叠加区域在调整后的前景图上的对应区域
    //因为有可能我们故意把图像拖拽超出画布范围，所以需要调整,这是针对图像帧本身而言的
    //原始叠加区域: (-50, 50) 到 (150, 200)  // 左边界超出
    //裁剪后区域:   (0, 50) 到 (150, 200)    // 被裁剪了
    int fg_x1 = x1 - x;
    int fg_y1 = y1 - y;
    int fg_x2 = fg_x1 + (x2 - x1);
    int fg_y2 = fg_y1 + (y2 - y1);

    // 验证并调整前景ROI坐标
    if (fg_x1 < 0 || fg_y1 < 0 || fg_x2 > resized_foreground.cols || fg_y2 > resized_foreground.rows) {
        fg_x1 = std::max(fg_x1, 0);
        fg_y1 = std::max(fg_y1, 0);
        fg_x2 = std::min(fg_x2, resized_foreground.cols);
        fg_y2 = std::min(fg_y2, resized_foreground.rows);

        if (fg_x1 >= fg_x2 || fg_y1 >= fg_y2) {
            std::cerr << "Adjusted foreground ROI still invalid" << std::endl;
            return;
        }
    }

    // 创建前景图的裁剪区域
    cv::Rect roi_fg(fg_x1, fg_y1, fg_x2 - fg_x1, fg_y2 - fg_y1);
    cv::Mat foreground_roi = resized_foreground(roi_fg);

    // 创建背景图的目标区域
    cv::Rect roi_bg(x1, y1, x2 - x1, y2 - y1);
    cv::Mat background_roi = background(roi_bg);

    // 将前景图叠加到背景图上
    foreground_roi.copyTo(background_roi);
}
