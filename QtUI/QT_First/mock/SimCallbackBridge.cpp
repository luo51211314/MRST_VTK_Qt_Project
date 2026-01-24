#include "SimCallbackBridge.h"

SimCallbackBridge::SimCallbackBridge(QObject* parent)
    : QObject(parent)
{}

void SimCallbackBridge::onProgressUpdate(double progress, double current_time)
{
    emit sigProgress(progress, current_time);
}

void SimCallbackBridge::onSimulationCompleted()
{
    emit sigCompleted();
}

void SimCallbackBridge::onSimulationFailed(const std::string& error_message)
{
    emit sigFailed(QString::fromStdString(error_message));
}

void SimCallbackBridge::onTimeStepChanged(double new_time_step)
{
    emit sigTimeStep(new_time_step);
}
