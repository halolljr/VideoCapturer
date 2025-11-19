#include "ffrenderwidget.h"
#include"filter/ffvfilter.h"
#include"ffcapwindow.h"

#ifndef RESIZE_DIR
#define RESIZE_DIR
enum ResizeDir {
    NODIR,
    TOP,
    BOTTOM,
    LEFT,
    RIGHT,
    BOTTOM_LEFT,
    BOTTOM_RIGHT,
    TOP_LEFT,
    TOP_RIGHT
};
#endif

#define BORDER_WIDTH 5
#define WIDTH 1920
#define HEIGHT 1080


FFRenderWidget::FFRenderWidget(FFVFilter* vFilter_, int type_, QWidget *parent) :
    QWidget(parent),type(type_),vFilter(vFilter_)
{
    glRenderWidget = new FFGLRenderWidget(this);
    layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);
    layout->addWidget(glRenderWidget);

    this->setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setMouseTracking(true);
    glRenderWidget->setMouseTracking(true);

    resize(400,300);
    setMinimumSize(100,100);
    moveFlag = true;
    stackNumber = 1;

    calcPos();
    setFilterPos(overlayX,overlayY,overlayW,overlayH);

    if(parentWidget()->parentWidget()->parentWidget() != nullptr){
        capWindow = static_cast<FFCapWindow*>(parentWidget()->parentWidget()->parentWidget());
    }
}

FFRenderWidget::~FFRenderWidget()
{
}

bool FFRenderWidget::getMoveFlag(){
    return moveFlag;
}

void FFRenderWidget::setMoveFlag(bool flag){
    moveFlag = flag;
}


void FFRenderWidget::setYUVData(AVFrame *frame)
{
    glRenderWidget->setYUVData(frame);
}

void FFRenderWidget::setYUVData(uint8_t *yData, uint8_t *uData, uint8_t *vData, int width, int height)
{
    glRenderWidget->setYUVData(yData,uData,vData,width,height);
}

void FFRenderWidget::mousePressEvent(QMouseEvent *event)
{

    if (event->button() == Qt::LeftButton) {
        this->raise();
        if(capWindow){
            capWindow->adjustStatckNumber(type);
        }
        dragPos = event->globalPos() - frameGeometry().topLeft(); // 记录鼠标按下时相对窗口左上角的偏移
        initialGeometry = this->geometry(); // 记录窗口初始几何信息
        resizeDir = getResizeDirction(event->pos());
        event->accept();
    }
}

void FFRenderWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        if(capWindow){
            capWindow->adjustStatckNumber(type);
        }
        moveFlag = true;
        if (resizeDir == NODIR) {
            this->move(event->globalPos() - dragPos); // 移动窗口
        }
        else{
            QPoint delta = event->globalPos() - (dragPos + initialGeometry.topLeft());
            // 计算鼠标相对于窗口初始位置的偏移
            QRect newGeometry = initialGeometry;
            switch (resizeDir) {
            case TOP:
                newGeometry.setTop(newGeometry.top() + delta.y());
                break;
            case BOTTOM:
                newGeometry.setBottom(newGeometry.bottom() + delta.y());
                break;
            case LEFT:
                newGeometry.setLeft(newGeometry.left() + delta.x());
                break;
            case RIGHT:
                newGeometry.setRight(newGeometry.right() + delta.x());
                break;
            case BOTTOM_LEFT:
                newGeometry.setLeft(newGeometry.left() + delta.x());
                newGeometry.setBottom(newGeometry.bottom() + delta.y());
                break;
            case BOTTOM_RIGHT:
                newGeometry.setRight(newGeometry.right() + delta.x());
                newGeometry.setBottom(newGeometry.bottom() + delta.y());
                break;
            case TOP_LEFT:
                newGeometry.setLeft(newGeometry.left() + delta.x());
                newGeometry.setTop(newGeometry.top() + delta.y());
                break;
            case TOP_RIGHT:
                newGeometry.setRight(newGeometry.right() + delta.x());
                newGeometry.setTop(newGeometry.top() + delta.y());
                break;
            default:
                break;
            }

            bool canResize = true;
            if ((resizeDir == TOP || resizeDir == BOTTOM)&& newGeometry.height() - 2 < this->minimumHeight()) {
                newGeometry.setTop(initialGeometry.top() + initialGeometry.height() - this->minimumHeight());
                canResize = false;
            }
            else if ((resizeDir == LEFT || resizeDir == RIGHT) && newGeometry.width() - 2 < this->minimumWidth()) {
                if (resizeDir == LEFT) {
                    newGeometry.setLeft(initialGeometry.right() - this->minimumWidth());
                }
                else {
                    newGeometry.setRight(initialGeometry.left() + this->minimumWidth());
                }
                canResize = false;
            }
            // 添加左下角缩放最小尺寸判断
            else if (resizeDir == BOTTOM_LEFT) {
                if (newGeometry.width() - 2 < this->minimumWidth()) {
                    newGeometry.setLeft(initialGeometry.right() - this->minimumWidth());
                    canResize = false;
                }
                if (newGeometry.height() - 2 < this->minimumHeight()) {
                    newGeometry.setBottom(initialGeometry.top() + this->minimumHeight());
                    canResize = false;
                }
            }
            // 添加右下角缩放最小尺寸判断
            else if (resizeDir == BOTTOM_RIGHT) {
                if (newGeometry.width() - 2 < this->minimumWidth()) {
                    newGeometry.setRight(initialGeometry.left() + this->minimumWidth());
                    canResize = false;
                }
                if (newGeometry.height() - 2< this->minimumHeight()) {
                    newGeometry.setBottom(initialGeometry.top() + this->minimumHeight());
                    canResize = false;
                }
            }
            // 添加左上角缩放最小尺寸判断
            else if (resizeDir == TOP_LEFT) {
                if (newGeometry.width() - 2 < this->minimumWidth()) {
                    newGeometry.setLeft(initialGeometry.right() - this->minimumWidth());
                    canResize = false;
                }
                if (newGeometry.height() - 2 < this->minimumHeight()) {
                    newGeometry.setTop(initialGeometry.bottom() - this->minimumHeight());
                    canResize = false;
                }
            }
            // 添加右上角缩放最小尺寸判断
            else if (resizeDir == TOP_RIGHT) {
                if (newGeometry.width() - 2 < this->minimumWidth()) {
                    newGeometry.setRight(initialGeometry.left() + this->minimumWidth());
                    canResize = false;
                }
                if (newGeometry.height() - 2< this->minimumHeight()) {
                    newGeometry.setTop(initialGeometry.bottom() - this->minimumHeight());
                    canResize = false;
                }
            }

            if (canResize) {
                this->setGeometry(newGeometry);
                this->update();
            }
        }

        //更新overlay位置
        calcPos();
        setFilterPos(overlayX,overlayY,overlayW,overlayH);
    }
    else {
        int dir = getResizeDirction(event->pos());
        switch (dir) {
        case TOP:
        case BOTTOM:
            setCursor(Qt::SizeVerCursor);
            break;
        case LEFT:
        case RIGHT:
            setCursor(Qt::SizeHorCursor);
            break;
        case BOTTOM_LEFT:
            setCursor(Qt::SizeBDiagCursor);
            break;
        case BOTTOM_RIGHT:
            setCursor(Qt::SizeFDiagCursor);
            break;
        case TOP_LEFT:
            setCursor(Qt::SizeFDiagCursor);
            break;
        case TOP_RIGHT:
            setCursor(Qt::SizeBDiagCursor);
            break;
        default:
            setCursor(Qt::ArrowCursor);
            break;
        }
        QWidget::mouseMoveEvent(event);
    }
}

void FFRenderWidget::mouseReleaseEvent(QMouseEvent *event)
{

    if (event->button() == Qt::LeftButton) {
        resizeDir = NODIR;
        setCursor(Qt::ArrowCursor);
    }
}

void FFRenderWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);  // 先绘制父部件内容

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen;
    pen.setWidth(1);

    if (isFoucs) {
        pen.setColor(QColor(0,255,0));  // 悬停时绿色边框
    }
    else {
        pen.setColor(QColor(255,0,0));
    }
    painter.setPen(pen);
    painter.drawRect(this->rect());

}
void FFRenderWidget::enterEvent(QEvent *event)
{
    //    std::cout << "enterEvent" << std::endl;
    isFoucs = true;
    update();
    QWidget::enterEvent(event);
}

void FFRenderWidget::leaveEvent(QEvent *event)
{
    isFoucs = false;
    update();
    QWidget::leaveEvent(event);
}

int FFRenderWidget::getResizeDirction(const QPoint &pos)
{
    enum ResizeDir ret = NODIR;
    if (pos.x() < BORDER_WIDTH) {
        ret = LEFT;
    }
    else if (pos.x() > width() - BORDER_WIDTH) {
        ret = RIGHT;
    }
    else if (pos.y() < BORDER_WIDTH) {
        ret = TOP;
    }
    else if (pos.y() > height() - BORDER_WIDTH) {
        ret = BOTTOM;
    }

    if(pos.x() < BORDER_WIDTH && pos.y() > height() - BORDER_WIDTH){
        ret = BOTTOM_LEFT;
    }

    else if(pos.x() > width() - BORDER_WIDTH && pos.y() > height() - BORDER_WIDTH){
        ret = BOTTOM_RIGHT;
    }
    if(pos.x() < BORDER_WIDTH && pos.y() < BORDER_WIDTH){
        ret = TOP_LEFT;
    }
    else if(pos.x() > width() - BORDER_WIDTH && pos.y() < BORDER_WIDTH){
        ret = TOP_RIGHT;
    }

    return ret;
}

void FFRenderWidget::setFilterPos(int x, int y, int w, int h)
{
    std::lock_guard<std::mutex>lock(mutex);
    if(vFilter){
        overlayX = x;
        overlayY = y;
        overlayW = w;
        overlayH = h;
        vFilter->setOverlayPos(x,y,w,h);
    }
}

void FFRenderWidget::getOverlayPos(int *x, int *y, int *w, int *h)
{
    std::lock_guard<std::mutex>lock(mutex);
    *x = overlayX;
    *y = overlayY;
    *w= overlayW;
    *h = overlayH;
}


void FFRenderWidget::calcPos()
{
    if (parentWidget() == nullptr) {
        return;
    }

    // 获取父控件的几何信息（位置+尺寸）
    QRect parentRect = parentWidget()->geometry();

    // 确保父控件尺寸有效
    if (parentRect.width() <= 0 || parentRect.height() <= 0) {
        return;
    }

    // 获取当前控件在父控件中的位置（左上角坐标）
    QPoint posInParent = this->mapToParent(QPoint(0, 0));

    // 获取当前控件的尺寸
    QSize widgetSize = this->size();

    // 计算位置百分比并转换为参考分辨率（标准画布，我们已经在openCV那边固定好了）下的坐标
    double xPercent = static_cast<double>(posInParent.x()) / parentRect.width();
    double yPercent = static_cast<double>(posInParent.y()) / parentRect.height();
    overlayX = static_cast<int>(xPercent * WIDTH);
    overlayY = static_cast<int>(yPercent * HEIGHT);

    // 计算尺寸百分比并转换，即控件的水平和垂直高度
    double widthPercent = static_cast<double>(widgetSize.width()) / parentRect.width();
    double heightPercent = static_cast<double>(widgetSize.height()) / parentRect.height();
    overlayW = static_cast<int>(widthPercent * WIDTH);
    overlayH = static_cast<int>(heightPercent * HEIGHT);
}

void FFRenderWidget::setStackNumber(int number)
{
    std::lock_guard<std::mutex>lock(mutex);
    stackNumber = number;

}

int FFRenderWidget::getStackNumber()
{
    return stackNumber;
}

int FFRenderWidget::getType()
{
    return type;
}

