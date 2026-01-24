#pragma once
#include <QObject>
#include <QImage>
#include <QTimer>

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
};
