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
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QPainterPath>





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

// ===================== 新增：数据入口实现 =====================

void VtkAdapter::setFractures(const std::vector<QtToVTK::FractureParameters>& fractures)
{
    fractures_ = fractures;
    emit sigLog(QString("[VTK][DATA] setFractures count=%1").arg(fractures_.size()));
    renderOnce();
}

void VtkAdapter::setGrid(const QtToVTK::GridParameters& grid)
{
    grid_ = grid;
    emit sigLog(QString("[VTK][DATA] setGrid nx=%1 ny=%2 nz=%3")
                    .arg(grid_.Nx).arg(grid_.Ny).arg(grid_.Nz));
    renderOnce();
}

void VtkAdapter::setWells(const std::vector<QtToVTK::WellParameters>& wells)
{
    wells_ = wells;
    emit sigLog(QString("[VTK][DATA] setWells count=%1").arg(wells_.size()));
    renderOnce();
}

void VtkAdapter::setPressureField(const std::vector<std::tuple<double,double,double,double>>& p)
{
    pressure_ = p;
    emit sigLog(QString("[VTK][DATA] setPressureField n=%1").arg(pressure_.size()));
    renderOnce();
}

void VtkAdapter::setSaturationField(const std::vector<std::tuple<double,double,double,double>>& s)
{
    saturation_ = s;
    emit sigLog(QString("[VTK][DATA] setSaturationField n=%1").arg(saturation_.size()));
    renderOnce();
}

void VtkAdapter::resetViewState()
{
    yaw_ = pitch_ = 0.0;
    zoom_ = 1.0;
    panX_ = panY_ = 0.0;
    pickedId_ = -1;
    emit sigLog("[VTK][DATA] resetViewState");
    renderOnce();
}

bool VtkAdapter::saveViewState(const QString& file)
{
    // 先做最小实现：只存交互参数（后续接真VTK时再扩展）
    QJsonObject o;
    o["yaw"] = yaw_;
    o["pitch"] = pitch_;
    o["zoom"] = zoom_;
    o["panX"] = panX_;
    o["panY"] = panY_;
    o["pickedId"] = pickedId_;

    QJsonDocument doc(o);
    QFile f(file);
    if (!f.open(QIODevice::WriteOnly)) {
        emit sigLog(QString("[VTK][ERR] saveViewState open failed: %1").arg(file));
        return false;
    }
    f.write(doc.toJson(QJsonDocument::Indented));
    emit sigLog(QString("[VTK][DATA] saveViewState ok: %1").arg(file));
    return true;
}

bool VtkAdapter::loadViewState(const QString& file)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        emit sigLog(QString("[VTK][ERR] loadViewState open failed: %1").arg(file));
        return false;
    }
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        emit sigLog(QString("[VTK][ERR] loadViewState invalid json: %1").arg(file));
        return false;
    }
    QJsonObject o = doc.object();
    yaw_ = o.value("yaw").toDouble(yaw_);
    pitch_ = o.value("pitch").toDouble(pitch_);
    zoom_ = o.value("zoom").toDouble(zoom_);
    panX_ = o.value("panX").toDouble(panX_);
    panY_ = o.value("panY").toDouble(panY_);
    pickedId_ = o.value("pickedId").toInt(pickedId_);

    emit sigLog(QString("[VTK][DATA] loadViewState ok: %1").arg(file));
    renderOnce();
    return true;
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
    const int W = 1280;
    const int H = 720;

    QImage img(W, H, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // ---------------------------
    // 1) 背景：柔和渐变 + 暗角
    // ---------------------------
    {
        QLinearGradient bg(0, 0, 0, H);
        bg.setColorAt(0.0, QColor(252, 252, 252));
        bg.setColorAt(1.0, QColor(238, 242, 247));
        p.fillRect(QRect(0, 0, W, H), bg);

        // vignette（暗角）
        QRadialGradient vig(QPointF(W*0.55, H*0.55), qMax(W, H) * 0.75);
        vig.setColorAt(0.0, QColor(0,0,0,0));
        vig.setColorAt(1.0, QColor(0,0,0,55));
        p.fillRect(QRect(0,0,W,H), vig);
    }

    // 交互映射
    const double yawRad   = qDegreesToRadians(yaw_);
    const double pitchRad = qDegreesToRadians(pitch_);
    const QPointF center(W*0.60 + panX_, H*0.55 + panY_);
    const double R = qMin(W, H) * 0.22 * zoom_;
    const double yScale = 0.40 + 0.60 * std::abs(std::cos(pitchRad)); // 始终正，避免跳变

    // ---------------------------
    // 2) 地面网格：细、淡、有透视感
    // ---------------------------
    auto project = [&](double x, double y)->QPointF {
        // 2D “伪透视”：x 受 yaw 旋转，y 受 pitch 压缩，再整体下移一点当“地面”
        double xr = x*std::cos(yawRad) - y*std::sin(yawRad);
        double yr = x*std::sin(yawRad) + y*std::cos(yawRad);
        return center + QPointF(xr, yr * yScale + R*0.55);
    };

    {
        p.save();
        p.setPen(QPen(QColor(90, 110, 130, 35), 1));

        const int lines = 18;
        const double step = R * 0.18;
        for (int i = -lines; i <= lines; ++i) {
            // 横线
            QPointF a = project(-lines*step, i*step);
            QPointF b = project( lines*step, i*step);
            p.drawLine(a, b);

            // 竖线
            QPointF c = project(i*step, -lines*step);
            QPointF d = project(i*step,  lines*step);
            p.drawLine(c, d);
        }

        // 轴线更亮一点
        p.setPen(QPen(QColor(255,255,255,90), 2));
        p.drawLine(project(-lines*step, 0), project(lines*step, 0));
        p.drawLine(project(0, -lines*step), project(0, lines*step));

        p.restore();
    }



    // ---------------------------
    // 4) “晶体/岩心”主体：更真实的材质 + 边缘光
    // ---------------------------
    // 顶面四边形（旋转）
    QPointF top[4];
    for (int i=0; i<4; ++i) {
        double ang = yawRad + i * M_PI_2;
        double rr  = R * (0.80 + 0.12*std::sin(yawRad*0.7));
        top[i] = center + QPointF(std::cos(ang)*rr, std::sin(ang)*rr*yScale);
    }

    // 挤出（厚度）
    QPointF off(30.0*zoom_*std::cos(yawRad),
                22.0*zoom_*std::sin(yawRad) + 26.0*zoom_);
    QPointF bot[4];
    for (int i=0;i<4;i++) bot[i] = top[i] + off;

    // 阴影（更自然：软阴影）
    {
        p.save();
        QPainterPath sh;
        QPolygonF poly;
        for (int i=0;i<4;i++) poly << (bot[i] + QPointF(10, 14));
        sh.addPolygon(poly);
        p.setPen(Qt::NoPen);

        QRadialGradient sg(center + QPointF(30, 55), R*1.6);
        sg.setColorAt(0.0, QColor(0,0,0,55));
        sg.setColorAt(1.0, QColor(0,0,0,0));
        p.setBrush(sg);
        p.drawPath(sh);
        p.restore();
    }

    // 侧面：做“渐变面”更像材质
    auto drawSide = [&](int i){
        int j = (i+1)%4;
        QPolygonF side;
        side << top[i] << top[j] << bot[j] << bot[i];

        // 根据面法线方向做明暗（简化版）
        double light = 0.55 + 0.45 * std::cos(yawRad + i*M_PI_2);
        int baseA = 210;
        QColor c1(70, 140, 220, baseA);
        QColor c2(40, 95, 170, baseA);

        // 侧面渐变：上亮下暗
        QLinearGradient g(top[i], bot[i]);
        g.setColorAt(0.0, QColor(
                              int(c1.red()   * (0.85 + 0.25*light)),
                              int(c1.green() * (0.85 + 0.25*light)),
                              int(c1.blue()  * (0.85 + 0.25*light)),
                              baseA));
        g.setColorAt(1.0, QColor(
                              int(c2.red()   * (0.85 + 0.25*light)),
                              int(c2.green() * (0.85 + 0.25*light)),
                              int(c2.blue()  * (0.85 + 0.25*light)),
                              baseA));

        p.setBrush(g);
        p.setPen(QPen(QColor(20, 60, 120, 90), 1));
        p.drawPolygon(side);
    };

    for (int i=0;i<4;i++) drawSide(i);

    // 顶/底面：高光 + 边缘光
    QPolygonF topPoly, botPoly;
    for (int i=0;i<4;i++){ topPoly << top[i]; botPoly << bot[i]; }

    // flipped 判定：pitch 过 90 度视为翻面
    auto normDeg = [](double a){
        a = std::fmod(a, 360.0);
        if (a < -180.0) a += 360.0;
        if (a >  180.0) a -= 360.0;
        return a;
    };
    const bool flipped = std::abs(normDeg(pitch_)) > 90.0;

    auto drawFace = [&](const QPolygonF& poly, bool isTop){
        p.save();

        // 面材质：径向高光
        QPointF c = (poly[0] + poly[1] + poly[2] + poly[3]) / 4.0;
        QRadialGradient fg(c + QPointF(-R*0.10, -R*0.12), R*0.9);
        if (isTop) {
            fg.setColorAt(0.0, QColor(160, 210, 255, 240));
            fg.setColorAt(1.0, QColor(80, 150, 230, 220));
        } else {
            fg.setColorAt(0.0, QColor(120, 170, 240, 170));
            fg.setColorAt(1.0, QColor(50, 105, 175, 160));
        }

        p.setBrush(fg);
        p.setPen(QPen(QColor(255,255,255,120), isTop ? 2 : 1));
        p.drawPolygon(poly);

        // 边缘光
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255,255,255,90), 1));
        p.drawPolygon(poly);

        p.restore();
    };

    if (!flipped) {
        drawFace(botPoly, false);
        drawFace(topPoly, true);
    } else {
        drawFace(topPoly, false);
        drawFace(botPoly, true);
    }

    // 顶面“十字”线：画在可见顶面
    {
        const QPolygonF& visTop = flipped ? botPoly : topPoly;
        p.setPen(QPen(QColor(255,255,255,120), 2));
        p.drawLine(visTop[0], visTop[2]);
        p.drawLine(visTop[1], visTop[3]);
    }

    // ---------------------------
    // 5) HUD：现代卡片风格
    // ---------------------------
    auto drawHud = [&](){
        p.save();

        QRectF hud(26, 26, 420, 190);

        // 阴影
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0,0,0,35));
        p.drawRoundedRect(hud.translated(3, 4), 12, 12);

        // 卡片背景（半透明玻璃感）
        p.setBrush(QColor(255,255,255,215));
        p.setPen(QPen(QColor(210,220,232,220), 1));
        p.drawRoundedRect(hud, 12, 12);

        // 标题
        QFont title = p.font();
        title.setPointSize(11);
        title.setBold(true);
        p.setFont(title);
        p.setPen(QColor(30,40,55));
        p.drawText(QPointF(hud.left()+16, hud.top()+28), "Remote VTK Render (Mock)");

        // 分割线
        p.setPen(QPen(QColor(180,190,205,120), 1));
        p.drawLine(QPointF(hud.left()+16, hud.top()+40), QPointF(hud.right()-16, hud.top()+40));

        // 内容：数字用等宽字体更专业
        QFont mono("Consolas");
        mono.setPointSize(10);
        p.setFont(mono);
        p.setPen(QColor(55,65,80));

        double y = hud.top() + 62;
        auto row = [&](const QString& k, const QString& v){
            p.setPen(QColor(95,105,120));
            p.drawText(QPointF(hud.left()+16, y), k);
            p.setPen(QColor(35,45,60));
            p.drawText(QPointF(hud.left()+160, y), v);
            y += 22;
        };

        row("Frame", QString::number(frameId_));
        row("Yaw / Pitch", QString("%1 / %2").arg(yaw_, 0, 'f', 1).arg(pitch_, 0, 'f', 1));
        row("Zoom", QString::number(zoom_, 'f', 2));
        row("Pan", QString("(%1, %2)").arg(panX_, 0, 'f', 0).arg(panY_, 0, 'f', 0));
        row("PickedId", QString::number(pickedId_));
        row("Grid", QString("%1 x %2 x %3").arg(grid_.Nx).arg(grid_.Ny).arg(grid_.Nz));
        row("Fractures", QString::number(fractures_.size()));

        p.restore();
    };
    drawHud();

    // ---------------------------
    // 6) 右下角时间戳：更克制
    // ---------------------------
    {
        p.setPen(QColor(90, 100, 115, 140));
        QFont f = p.font();
        f.setPointSize(9);
        p.setFont(f);
        p.drawText(QRectF(0,0,W-18,H-12), Qt::AlignRight | Qt::AlignBottom,
                   QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
    }

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



