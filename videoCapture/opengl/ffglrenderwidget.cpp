#include "ffglrenderwidget.h"
#include<QMouseEvent>

// 顶点数据（位置(x,y) + 纹理坐标(u,v)）
const QVector<float> FFGLRenderWidget::vertices = {
    -1.0f,  1.0f,  0.0f, 0.0f,  // 顶点0：左上
    -1.0f, -1.0f,  0.0f, 1.0f,  // 顶点1：左下
    1.0f, -1.0f,  1.0f, 1.0f,  // 顶点2：右下
    1.0f,  1.0f,  1.0f, 0.0f   // 顶点3：右上
};

const QVector<unsigned int> FFGLRenderWidget::indices = {0, 1, 2, 3};

FFGLRenderWidget::FFGLRenderWidget(QWidget *parent) : QOpenGLWidget(parent) {
    setMinimumSize(100,100);
    // 确保每个实例使用独立的着色器程序
    shaderProgram = new QOpenGLShaderProgram(this);
}

FFGLRenderWidget::~FFGLRenderWidget() {
    makeCurrent();
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vboVertice);
    glDeleteBuffers(1, &ebo);
    glDeleteTextures(1, &yTexture);
    glDeleteTextures(1, &uTexture);
    glDeleteTextures(1, &vTexture);

    // 清理着色器程序
    delete shaderProgram;

    doneCurrent();
}

void FFGLRenderWidget::setAspect(float aspect_)
{
    aspect = aspect_;
}

void FFGLRenderWidget::setBlackScreen()
{
    makeCurrent();

    // 配置纹理参数并初始化为黑色YUV数据
    uint8_t yData = 0;    // Y分量全0
    uint8_t uData = 128;  // U分量全128
    uint8_t vData = 128;  // V分量全128

    // 初始化Y纹理
    glBindTexture(GL_TEXTURE_2D, yTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &yData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 初始化U纹理
    glBindTexture(GL_TEXTURE_2D, uTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &uData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 初始化V纹理
    glBindTexture(GL_TEXTURE_2D, vTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &vData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    update();
    doneCurrent();
}

void FFGLRenderWidget::setKeepRatio(bool flag)
{
    keepRatio = flag;
}

void FFGLRenderWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // 初始化缓冲对象
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vboVertice);
    glGenBuffers(1, &ebo);

    // 配置顶点数据
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vboVertice);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // 设置顶点属性指针
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 解绑VAO
    glBindVertexArray(0);

    // 初始化纹理
    glGenTextures(1, &yTexture);
    glGenTextures(1, &uTexture);
    glGenTextures(1, &vTexture);

    //初始绑定为黑屏
    // 配置纹理参数并初始化为黑色YUV数据
    uint8_t yData = 0;    // Y分量全0
    uint8_t uData = 128;  // U分量全128
    uint8_t vData = 128;  // V分量全128

    // 初始化Y纹理
    glBindTexture(GL_TEXTURE_2D, yTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &yData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 初始化U纹理
    glBindTexture(GL_TEXTURE_2D, uTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &uData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 初始化V纹理
    glBindTexture(GL_TEXTURE_2D, vTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &vData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 加载着色器
    if (!shaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaderSource/source.vert") ||
            !shaderProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaderSource/source.frag") ||
            !shaderProgram->link()) {
        qCritical() << "Shader error:" << shaderProgram->log();
    }
}

void FFGLRenderWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);

    // 确保使用当前实例的着色器程序
    shaderProgram->bind();

    // 设置视口
    if(keepRatio){
        glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
    }
    // 绑定纹理
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, yTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, uTexture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, vTexture);

    shaderProgram->setUniformValue("yTexture", 0);
    shaderProgram->setUniformValue("uTexture", 1);
    shaderProgram->setUniformValue("vTexture", 2);

    // 绘制四边形
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    shaderProgram->release();
}

void FFGLRenderWidget::resizeGL(int w, int h) {
    if(keepRatio){
        const float targetAspect = aspect;
        const float currentAspect = static_cast<float>(w) / h;

        if (currentAspect > targetAspect) {
            // 窗口过宽，左右加黑边
            viewportWidth = static_cast<int>(h * targetAspect);
            viewportHeight = h;
            viewportX = (w - viewportWidth) / 2;
            viewportY = 0;
        } else {
            // 窗口过高，上下加黑边
            viewportWidth = w;
            viewportHeight = static_cast<int>(w / targetAspect);
            viewportX = 0;
            viewportY = (h - viewportHeight) / 2;
        }
    }
}

void FFGLRenderWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton){
        emit mouseDoubleClick();
    }

    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void FFGLRenderWidget::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton){
        emit mouseClick();
    }

    QOpenGLWidget::mousePressEvent(event);
}

void FFGLRenderWidget::setYUVData(uint8_t *yData, uint8_t *uData, uint8_t *vData, int width, int height) {
    makeCurrent();

    // 上传Y分量
    glBindTexture(GL_TEXTURE_2D, yTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, yData);

    // 上传UV分量（宽高各减半）
    int uvWidth = width / 2;
    int uvHeight = height / 2;
    glBindTexture(GL_TEXTURE_2D, uTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, uvWidth, uvHeight, 0, GL_RED, GL_UNSIGNED_BYTE, uData);
    glBindTexture(GL_TEXTURE_2D, vTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, uvWidth, uvHeight, 0, GL_RED, GL_UNSIGNED_BYTE, vData);

    update();
    doneCurrent();

    delete[] yData;
    delete[] uData;
    delete[] vData;
}

void FFGLRenderWidget::setYUVData(AVFrame *frame)
{
    makeCurrent();

    int width = frame->width;
    int height = frame->height;

    // 上传Y分量
    glBindTexture(GL_TEXTURE_2D, yTexture);
    // 设置行对齐
    glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[0] / sizeof(uint8_t));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, frame->data[0]);
    // 恢复默认行对齐
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    // 上传UV分量（宽高各减半）
    int uvWidth = width / 2;
    int uvHeight = height / 2;

    // 上传U分量
    glBindTexture(GL_TEXTURE_2D, uTexture);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[1] / sizeof(uint8_t));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, uvWidth, uvHeight, 0, GL_RED, GL_UNSIGNED_BYTE, frame->data[1]);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    // 上传V分量
    glBindTexture(GL_TEXTURE_2D, vTexture);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize[2] / sizeof(uint8_t));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, uvWidth, uvHeight, 0, GL_RED, GL_UNSIGNED_BYTE, frame->data[2]);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    update();
    doneCurrent();

    av_frame_unref(frame);
    av_frame_free(&frame);
}
