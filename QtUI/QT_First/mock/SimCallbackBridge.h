#pragma once
#include <QObject>
#include <QString>

#include "QtUItoAlgoInterface.h"   // Common 接口

class SimCallbackBridge : public QObject, public QtUItoAlgo::ISimulationCallback
{
    Q_OBJECT
public:
    explicit SimCallbackBridge(QObject* parent = nullptr);

    // ===== ISimulationCallback =====
    void onProgressUpdate(double progress, double current_time) override;
    void onSimulationCompleted() override;
    void onSimulationFailed(const std::string& error_message) override;
    void onTimeStepChanged(double new_time_step) override;

signals:
    void sigProgress(double progress, double currentTime);
    void sigCompleted();
    void sigFailed(const QString& msg);
    void sigTimeStep(double dt);
};

