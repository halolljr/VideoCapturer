#ifndef FFCapHeaderWidget_H
#define FFCapHeaderWidget_H

#include <QWidget>
#include <QMouseEvent>
#include <QPainter>

QT_BEGIN_NAMESPACE
namespace Ui { class FFCapHeaderWidget; }
QT_END_NAMESPACE


class FFCapHeaderWidget : public QWidget
{
    Q_OBJECT

public:
    FFCapHeaderWidget(QWidget *parent = nullptr);
    ~FFCapHeaderWidget();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
private:
    int getResizeDirction(const QPoint &pos);
private:
    QPoint dragPos;
    QRect initialGeometry;
    int resizeDir;
};
#endif // FFCapHeaderWidget_H
