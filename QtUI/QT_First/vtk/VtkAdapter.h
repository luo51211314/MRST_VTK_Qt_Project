#pragma once
#include <QObject>
#include <QImage>
#include <QTimer>
#include <QByteArray>
#include "qttovtkcontroladapter.h"
#include <tuple>
#include <vector>



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

    // === QtToVTK 数据输入 ===
    void setFractures(const std::vector<QtToVTK::FractureParameters>& fractures);
    void setGrid(const QtToVTK::GridParameters& grid);
    void setWells(const std::vector<QtToVTK::WellParameters>& wells);
    void setPressureField(const std::vector<std::tuple<double,double,double,double>>& p);
    void setSaturationField(const std::vector<std::tuple<double,double,double,double>>& s);

    void resetViewState();   // 对应 resetView
    bool saveViewState(const QString& file);
    bool loadViewState(const QString& file);


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

    // ====== 保存从 QtToVTK 进来的数据（新增）======
    std::vector<QtToVTK::FractureParameters> fractures_;
    QtToVTK::GridParameters grid_;
    std::vector<QtToVTK::WellParameters> wells_;
    std::vector<std::tuple<double,double,double,double>> pressure_;
    std::vector<std::tuple<double,double,double,double>> saturation_;


    void renderOnce();




};
