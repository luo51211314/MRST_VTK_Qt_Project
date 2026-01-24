#include "VtkAdapter.h"
#include "VtkViewHost.h"

#include <QLabel>
#include <QPixmap>
#include <QPainter>
#include <QDateTime>
#include <QtMath>

VtkAdapter::VtkAdapter(VtkViewHost* host, QObject* parent)
    : QObject(parent), host_(host)
{
    // 定时器到了就生成一帧
    connect(&mockTimer_, &QTimer::timeout, this, [this](){
        submitFrame(makeMockFrame());
    });
}

void VtkAdapter::initRemoteStream()
{
    if (!host_) return;

    // QLabel 作为“远程渲染显示器”
    remoteLabel_ = new QLabel("Remote render stream (waiting frames)...");
    remoteLabel_->setAlignment(Qt::AlignCenter);
    remoteLabel_->setMinimumSize(400, 300);
    remoteLabel_->setStyleSheet("QLabel{ color:#666; font-size:14px; }");

    host_->setRenderWidget(remoteLabel_);
    emit sigLog("[VTK] Remote stream view initialized (QLabel mounted).");
}

void VtkAdapter::startMockStream(int fps)
{
    if (!remoteLabel_) initRemoteStream();

    fps = qBound(1, fps, 120);
    int interval = int(1000.0 / fps + 0.5);

    frameId_ = 0;
    mockTimer_.start(interval);

    emit sigLog(QString("[VTK] Mock stream started (%1 fps).").arg(fps));
}

void VtkAdapter::stopMockStream()
{
    mockTimer_.stop();
    emit sigLog("[VTK] Mock stream stopped.");
}

void VtkAdapter::submitFrame(const QImage& img)
{
    if (img.isNull()) return;
    lastFrame_ = img;
    updateLabelPixmap();
}

void VtkAdapter::updateLabelPixmap()
{
    if (!remoteLabel_ || lastFrame_.isNull()) return;

    // 按 label 尺寸等比缩放显示
    QPixmap pm = QPixmap::fromImage(lastFrame_);
    remoteLabel_->setPixmap(pm.scaled(remoteLabel_->size(),
                                      Qt::KeepAspectRatio,
                                      Qt::SmoothTransformation));
}

QImage VtkAdapter::makeMockFrame()
{
    // 生成一张“像 VTK 渲染出来的画面”的模拟图
    const int W = 1280;
    const int H = 720;

    QImage img(W, H, QImage::Format_RGB32);
    img.fill(QColor(245, 245, 245));

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 背景渐变块
    p.fillRect(QRect(0,0,W,H), QColor(250,250,250));

    // 画一个“3D 物体投影”的假图形（旋转感觉）
    const double t = frameId_ * 0.06;
    const QPointF c(W*0.55, H*0.50);
    const double R = qMin(W,H)*0.22;

    // 坐标轴
    p.setPen(QPen(QColor(180,180,180), 2));
    p.drawLine(QPointF(W*0.12, H*0.85), QPointF(W*0.32, H*0.85));
    p.drawLine(QPointF(W*0.12, H*0.85), QPointF(W*0.12, H*0.65));
    p.drawText(QPointF(W*0.33, H*0.85), "X");
    p.drawText(QPointF(W*0.12, H*0.63), "Y");

    // “旋转立方体”感觉：画一个动态四边形+阴影
    QPointF pts[4];
    for (int i=0;i<4;i++){
        double ang = t + i * M_PI_2;
        double r   = R * (0.75 + 0.25*qSin(t*0.7));
        pts[i] = c + QPointF(qCos(ang)*r, qSin(ang)*r*0.65);
    }

    // 阴影
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0,0,0,30));
    QPolygonF shadow;
    for (auto &pt: pts) shadow << (pt + QPointF(18, 22));
    p.drawPolygon(shadow);

    // 主体
    p.setBrush(QColor(120, 170, 230, 220));
    p.setPen(QPen(QColor(80, 120, 180), 3));
    QPolygonF poly;
    for (auto &pt: pts) poly << pt;
    p.drawPolygon(poly);

    // 叠加一些“网格线/边”
    p.setPen(QPen(QColor(255,255,255,140), 2));
    for (int i=0;i<4;i++){
        p.drawLine(pts[i], pts[(i+2)%4]);
    }

    // HUD 信息（模拟 VTK overlay）
    p.setPen(QPen(QColor(60,60,60), 1));
    p.setBrush(QColor(255,255,255,200));
    QRect hud(20, 20, 420, 110);
    p.drawRoundedRect(hud, 8, 8);

    p.setPen(QColor(40,40,40));
    p.drawText(40, 55, "Remote VTK Render (Mock Frames)");
    p.drawText(40, 80, QString("Frame: %1").arg(frameId_));
    p.drawText(40, 105, "Source: Ubuntu(VTK) -> Network -> Qt Display");

    // 时间戳
    p.setPen(QColor(120,120,120));
    p.drawText(W-260, H-24, QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));

    p.end();

    frameId_++;
    return img;
}
