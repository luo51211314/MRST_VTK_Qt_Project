#pragma once
#include <QObject>
#include <QString>
#include "QtUItoAlgoInterface.h"

class SimCallbackBridge : public QObject,
                          public QtUItoAlgo::ISimulationCallback
{
    Q_OBJECT
public:
    explicit SimCallbackBridge(QObject* parent = nullptr);

    // ===== ISimulationCallback 接口（只声明，不实现）=====
    void onProgressUpdate(double progress, double current_time) override;
    void onSimulationCompleted() override;
    void onSimulationFailed(const std::string& error_message) override;
    void onTimeStepChanged(double new_time_step) override;

signals:
    void sigProgress(double p, double t);
    void sigCompleted();
    void sigFailed(const QString& msg);
    void sigTimeStep(double dt);   // ⚠️ 注意：这个 signal 必须声明
};

