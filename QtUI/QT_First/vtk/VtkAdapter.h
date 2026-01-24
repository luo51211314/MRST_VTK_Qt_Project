#pragma once
#include <QObject>
#include <QImage>
#include <QTimer>
#include <QByteArray>



class VtkViewHost;
class QLabel;

class VtkAdapter : public QObject {
    Q_OBJECT
public:
    explicit VtkAdapter(VtkViewHost* host, QObject* parent=nullptr);

    // 远程渲染：初始化显示控件
    void initRemoteStream();

    // 模拟：开始/停止模拟帧流（代替 Ubuntu 发帧）
    void startMockStream(int fps = 30);
    void stopMockStream();

    // 真正远程接入时：Ubuntu 收到一帧图片，就调这个刷新
    void submitFrame(const QImage& img);

    // 模拟：收到一帧网络数据（JPEG/PNG 字节），Qt 端解码后显示
    void submitEncodedFrame(const QByteArray& bytes, const char* hintFormat = "JPG");


protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

signals:
    void sigLog(const QString& msg);

private:
    QImage makeMockFrame();      // 生成一张模拟帧
    void   updateLabelPixmap();  // 把 lastFrame_ 显示到 label

private:
    VtkViewHost* host_ = nullptr;
    QLabel*      remoteLabel_ = nullptr;

    QTimer       mockTimer_;
    QImage       lastFrame_;
    int          frameId_ = 0;

    // 交互状态
    bool   dragging_ = false;
    bool   rightDragging_ = false;
    QPoint lastPos_;

    double yaw_ = 0.0;      // 旋转
    double pitch_ = 0.0;
    double zoom_ = 1.0;     // 缩放
    double panX_ = 0.0;     // 平移（像素单位就行）
    double panY_ = 0.0;

    int    pickedId_ = -1;  // 模拟拾取结果

    void renderOnce();

};
