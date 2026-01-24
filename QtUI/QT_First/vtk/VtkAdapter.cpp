#include "VtkAdapter.h"
#include "VtkViewHost.h"

#include <QLabel>
#include <QPixmap>
#include <QPainter>
#include <QDateTime>
#include <QtMath>
#include <QBuffer>
#include <QByteArray>
#include <QEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QRandomGenerator>
#include <cmath>



VtkAdapter::VtkAdapter(VtkViewHost* host, QObject* parent)
    : QObject(parent), host_(host)
{
    connect(&mockTimer_, &QTimer::timeout, this, [this](){
        // 1) 生成一帧“渲染图”
        QImage frame = makeMockFrame();

        // 2) 模拟：Ubuntu 端会发 JPEG/PNG 字节流，这里先本地编码成 bytes
        QByteArray bytes;
        QBuffer buf(&bytes);
        buf.open(QIODevice::WriteOnly);

        // 用 JPG 更像真实流媒体；80 是质量（越大越清晰也越大）
        frame.save(&buf, "JPG", 80);

        // 3) 模拟：Qt 端“收到 bytes” → 解码 → 显示
        submitEncodedFrame(bytes, "JPG");

        // 可选：每秒打一次日志，确认“真的在收 bytes”
        if (frameId_ % 30 == 0) {
            emit sigLog(QString("[VTK] recv mock bytes = %1 KB")
                            .arg(bytes.size() / 1024.0, 0, 'f', 1));
        }
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

    remoteLabel_->setMouseTracking(true);
    remoteLabel_->setFocusPolicy(Qt::StrongFocus);    // 允许键盘
    remoteLabel_->installEventFilter(this);           // ✅ 关键

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

void VtkAdapter::renderOnce()
{
    // 生成一帧图
    QImage frame = makeMockFrame();

    // 走你现在的“bytes->decode->display”链路
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    frame.save(&buf, "JPG", 80);
    submitEncodedFrame(bytes, "JPG");
}


void VtkAdapter::submitFrame(const QImage& img)
{
    if (img.isNull()) return;
    lastFrame_ = img;
    updateLabelPixmap();
}

void VtkAdapter::submitEncodedFrame(const QByteArray& bytes, const char* hintFormat)
{
    if (bytes.isEmpty()) return;

    QImage img;
    if (hintFormat && *hintFormat) {
        img.loadFromData(bytes, hintFormat);  // "JPG" / "PNG"
    } else {
        img.loadFromData(bytes);              // 让 Qt 自己猜格式
    }

    if (img.isNull()) {
        emit sigLog("[VTK] decode failed: invalid image bytes.");
        return;
    }
    submitFrame(img);
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

    // 用交互变量驱动（而不是 frameId）
    const double t = qDegreesToRadians(yaw_);     // yaw 控制旋转角
    const double tilt = qDegreesToRadians(pitch_); // pitch 控制“透视/压扁”感觉

    // pan：把像素级平移映射到画布（你现在 pan 是像素累加，直接用就行）
    const QPointF c(W*0.55 + panX_, H*0.50 + panY_);

    // zoom：缩放半径
    const double R = qMin(W,H)*0.22 * zoom_;

    // pitch 越大，y方向压缩越强（模拟俯仰）
    const double yScale = 0.45 + 0.55 * qCos(tilt);


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
        pts[i] = c + QPointF(qCos(ang)*r, qSin(ang)*r*yScale);
    }

    // ===== 立体厚度：生成下表面（向右下挤出）=====
    double rad = qDegreesToRadians(yaw_);
    QPointF off(26.0 * zoom_ * std::cos(rad),
                26.0 * zoom_ * std::sin(rad) + 20.0 * zoom_);


    QPointF pts2[4];
    for (int i=0;i<4;i++) pts2[i] = pts[i] + off;


    // ===== 阴影：用下表面投影做阴影，更像 3D =====
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0,0,0,35));
    QPolygonF shadow;
    for (int i=0;i<4;i++) shadow << (pts2[i] + QPointF(8, 10));
    p.drawPolygon(shadow);

    // ===== 侧面：4 个面，做简易明暗（光照感）=====
    auto sideBrush = [&](int i)->QColor {
        // 简单光照：不同侧面不同深浅
        // 也可以根据 yaw_ 做更真实的光照，这里先够用
        int a = 170;
        int delta = (i==0 || i==2) ? 20 : -15;
        return QColor(90, 140, 210, a + delta);
    };

    for (int i=0;i<4;i++) {
        int j = (i+1)%4;
        QPolygonF side;
        side << pts[i] << pts[j] << pts2[j] << pts2[i];

        p.setPen(QPen(QColor(60,110,180,180), 2));
        p.setBrush(sideBrush(i));
        p.drawPolygon(side);
    }


    const bool flipped = std::cos(qDegreesToRadians(pitch_)) < 0.0;

    auto drawTopFace = [&](const QPolygonF& poly){
        p.setBrush(QColor(120, 170, 230, 220));       // 亮
        p.setPen(QPen(QColor(80, 120, 180), 3));
        p.drawPolygon(poly);
    };

    auto drawBottomFace = [&](const QPolygonF& poly){
        p.setBrush(QColor(90, 140, 210, 160));        // 深
        p.setPen(QPen(QColor(50, 90, 150, 120), 2));
        p.drawPolygon(poly);
    };

    QPolygonF topPoly, bottomPoly;
    for (int i=0;i<4;i++) {
        topPoly << pts[i];
        bottomPoly << pts2[i];
    }

    // flipped==false：pts 是上表面，pts2 是下表面（正常）
    // flipped==true ：翻面后，pts2 变成“看到的上表面”，pts 变成下表面（交换）
    if (!flipped) {
        drawBottomFace(bottomPoly);
        drawTopFace(topPoly);
    } else {
        drawBottomFace(topPoly);
        drawTopFace(bottomPoly);
    }

    // ===== 上表面对角线（你原来那两条线）=====
    p.setPen(QPen(QColor(255,255,255,150), 2));
    for (int i=0;i<4;i++){
        p.drawLine(pts[i], pts[(i+2)%4]);
    }


    // HUD 信息（模拟 VTK overlay）
    p.setPen(QPen(QColor(60,60,60), 1));
    p.setBrush(QColor(255,255,255,200));
    QRect hud(20, 20, 520, 190);

    p.drawRoundedRect(hud, 8, 8);

    p.setPen(QColor(40,40,40));
    p.drawText(40, 55, "Remote VTK Render (Mock Frames)");
    p.drawText(40, 80, QString("Frame: %1").arg(frameId_));
    p.drawText(40, 105, "Source: Ubuntu(VTK) -> Network -> Qt Display");

    p.drawText(40, 130, QString("Yaw: %1  Pitch: %2").arg(yaw_, 0, 'f', 1).arg(pitch_, 0, 'f', 1));
    p.drawText(40, 155, QString("Zoom: %1  Pan: (%2, %3)")
                            .arg(zoom_, 0, 'f', 2).arg(panX_, 0, 'f', 0).arg(panY_, 0, 'f', 0));
    p.drawText(40, 180, QString("PickedId: %1").arg(pickedId_));


    // 时间戳
    p.setPen(QColor(120,120,120));
    p.drawText(W-260, H-24, QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));

    p.end();

    frameId_++;
    return img;
}


bool VtkAdapter::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj != remoteLabel_) return QObject::eventFilter(obj, ev);

    switch (ev->type())
    {
    case QEvent::MouseButtonPress: {
        auto* e = static_cast<QMouseEvent*>(ev);
        lastPos_ = e->pos();

        if (e->button() == Qt::LeftButton) {
            dragging_ = true;
        } else if (e->button() == Qt::RightButton) {
            rightDragging_ = true;
        }
        return true;
    }
    case QEvent::MouseMove: {
        auto* e = static_cast<QMouseEvent*>(ev);
        QPoint delta = e->pos() - lastPos_;
        lastPos_ = e->pos();

        if (dragging_) {
            // 左键拖拽：旋转
            yaw_   += delta.x() * 0.4;
            pitch_ += delta.y() * 0.4;
            pitch_ = std::fmod(pitch_, 360.0);
            if (pitch_ > 180.0) pitch_ -= 360.0;
            if (pitch_ < -180.0) pitch_ += 360.0;


            emit sigLog(QString("[VTK][UI] rotate yaw=%1 pitch=%2")
                            .arg(yaw_, 0, 'f', 1).arg(pitch_, 0, 'f', 1));
            renderOnce();
        }
        if (rightDragging_) {
            // 右键拖拽：平移
            panX_ += delta.x();
            panY_ += delta.y();

            emit sigLog(QString("[VTK][UI] pan x=%1 y=%2")
                            .arg(panX_, 0, 'f', 0).arg(panY_, 0, 'f', 0));
            renderOnce();
        }
        return true;
    }
    case QEvent::MouseButtonRelease: {
        auto* e = static_cast<QMouseEvent*>(ev);
        if (e->button() == Qt::LeftButton) dragging_ = false;
        if (e->button() == Qt::RightButton) rightDragging_ = false;
        return true;
    }
    case QEvent::MouseButtonDblClick: {
        // 双击：重置视图（模拟 reset view）
        yaw_ = pitch_ = 0.0;
        zoom_ = 1.0;
        panX_ = panY_ = 0.0;
        pickedId_ = -1;
        emit sigLog("[VTK][UI] reset view");
        renderOnce();

        return true;
    }
    case QEvent::Wheel: {
        auto* e = static_cast<QWheelEvent*>(ev);
        const double numDegrees = e->angleDelta().y() / 8.0;
        const double numSteps = numDegrees / 15.0; // 一格
        zoom_ *= std::pow(1.12, numSteps);
        zoom_ = qBound(0.1, zoom_, 20.0);

        emit sigLog(QString("[VTK][UI] zoom=%1").arg(zoom_, 0, 'f', 2));
        renderOnce();
        return true;
    }
    case QEvent::KeyPress: {
        auto* e = static_cast<QKeyEvent*>(ev);
        if (e->key() == Qt::Key_Space) {
            // 空格：模拟一次 pick
            pickedId_ = int(QRandomGenerator::global()->bounded(1, 200));
            emit sigLog(QString("[VTK][UI] pick id=%1").arg(pickedId_));
            renderOnce();

            return true;
        }
        return false;
    }
    default:
        break;
    }

    return QObject::eventFilter(obj, ev);
}



