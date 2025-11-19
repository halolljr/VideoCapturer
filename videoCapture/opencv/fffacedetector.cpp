#include "fffacedetector.h"

FFFaceDetector::FFFaceDetector()
{
    smoothValue = 0;
    whiteValue = 0;
}

FFFaceDetector::~FFFaceDetector()
{
    if(faceDetector){
        delete faceDetector;
        faceDetector = nullptr;
    }
    if(adapter){
        delete adapter;
        adapter = nullptr;
    }
}

void FFFaceDetector::init()
{
    faceDetector = new cv::CascadeClassifier();
    profileDetector = new cv::CascadeClassifier();
    int ret = faceDetector->load("../videoCapture/3rdparty/data/lbpcascades/lbpcascade_frontalface.xml");
    if(!ret){
        std::cerr << "Load lbpcascade_frontalface.xml Fail!" << std::endl;
        return;
    }

    ret = profileDetector->load("../videoCapture/3rdparty/data/haarcascades/haarcascade_profileface.xml");

    if(!ret){
        std::cerr << "Load lbpcascade_profileface.xml Fail!" << std::endl;
        return;
    }

    adapter = new FFVideoAdapter();

    // 创建一个小的测试图像用于预初始化
    cv::Mat testImage = cv::Mat::zeros(100, 100, CV_8UC3);
    cv::Mat testGray;
    cv::cvtColor(testImage, testGray, cv::COLOR_BGR2GRAY);

    // 预检测，触发分类器初始化
    std::vector<cv::Rect> testFaces;
    faceDetector->detectMultiScale(testGray, testFaces, 1.3, 3, 0, cv::Size(30, 30));
    profileDetector->detectMultiScale(testGray, testFaces, 1.3, 3, 0, cv::Size(30, 30));
}


AVFrame* FFFaceDetector::detectFace(AVFrame *frame)
{

    if(whiteValue.load() == 0 && smoothValue.load() == 0){
        return frame;
    }

    cv::Mat bgrMat = adapter->convertFrameToMat(frame);
    cv::Mat scaledBgrMat;
    cv::resize(bgrMat, scaledBgrMat, cv::Size(), 0.25, 0.25);  // 缩小75%

    cv::Mat grayMat;
    cv::cvtColor(scaledBgrMat,grayMat,cv::COLOR_BGR2GRAY);
    cv::equalizeHist(grayMat,grayMat);
    std::vector<cv::Rect>faces;
    //正脸
    faceDetector->detectMultiScale(grayMat,faces,
                                   1.3, //缩放因子
                                   3,
                                   0,
                                   cv::Size(50,50)); //最小人脸大小

    //侧脸
    std::string text = "Face";
    if(faces.empty()){
        profileDetector->detectMultiScale(grayMat,faces,
                                          1.3, //缩放因子
                                          3,
                                          0,
                                          cv::Size(50,50)); //最小人脸大小
        text = "Profile";
    }

    for (auto& face : faces) {
        face.x *= 4; face.y *= 4;
        face.width *= 4; face.height *= 4;
    }


    //保证人脸识别平滑

    const int noFaceThreshold = 60;
    if(!faces.empty()){
        lastFaces = std::move(faces);
    }
    else{
        noFaceCount ++;
        if(noFaceCount == noFaceThreshold){
            noFaceCount = 0;
            lastFaces.clear();
        }
    }

    for (const auto& face : lastFaces) {
        // 提取人脸ROI
        cv::Rect roi = face & cv::Rect(0, 0, bgrMat.cols, bgrMat.rows);
        if (roi.width <= 0 || roi.height <= 0) continue;

        cv::Mat faceROI = bgrMat(roi);

        //计算ROI的平均RGB值
        cv::Scalar meanRGB = cv::mean(faceROI);
        int avgB = static_cast<int>(meanRGB[0]);
        int avgG = static_cast<int>(meanRGB[1]);
        int avgR = static_cast<int>(meanRGB[2]);

        const int threshold = 40;

        //对接近平均值的像素增加亮度
        cv::Mat resultROI = faceROI.clone();
        for (int y = 0; y < faceROI.rows; ++y) {
            cv::Vec3b* ptr = resultROI.ptr<cv::Vec3b>(y);
            for (int x = 0; x < faceROI.cols; ++x) {
                int b = ptr[x][0];
                int g = ptr[x][1];
                int r = ptr[x][2];

                // 计算与平均值的RGB距离
                int dist = std::abs(b - avgB) + std::abs(g - avgG) + std::abs(r - avgR);

                // 如果距离小于阈值，则增加亮度
                if (dist < threshold) {
                    ptr[x][0] = cv::saturate_cast<uchar>(b + whiteValue);
                    ptr[x][1] = cv::saturate_cast<uchar>(g + whiteValue);
                    ptr[x][2] = cv::saturate_cast<uchar>(r + whiteValue);
                }
            }
        }


        // 应用双边滤波
        int d = smoothValue * 2 + 1;
        int sigmaSpace = (smoothValue + 1) * 10;
        int sigmaColor = sigmaSpace;

        cv::Mat finalROI;
        cv::bilateralFilter(resultROI, finalROI,d, sigmaColor, sigmaSpace);

        // 将结果复制回原图
        finalROI.copyTo(bgrMat(roi));
    }

    AVFrame* faceFrame = adapter->convertMatToFrame(bgrMat);
    faceFrame->pts = frame->pts;

    av_frame_unref(frame);
    av_frame_free(&frame);


    return faceFrame;
}

void FFFaceDetector::setSmoothValue(int value)
{
    smoothValue.store(value);
}

void FFFaceDetector::setWhiteValue(int value)
{
    whiteValue.store(value);
}

