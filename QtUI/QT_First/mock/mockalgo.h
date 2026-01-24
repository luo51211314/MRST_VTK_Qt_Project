#pragma once

#include <atomic>
#include <thread>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>
#include <map>

#include "QtUItoAlgoInterface.h"   // Common 接口

class MockAlgo : public QtUItoAlgo::ISimulatorController,
                 public QtUItoAlgo::IDataTransfer
{
public:
    MockAlgo();
    ~MockAlgo() override;

    // 你现在 MainWindow 里调用的 setCallback，改成接接口指针
    void setCallback(QtUItoAlgo::ISimulationCallback* cb) { callback_ = cb; }

    // ===== ISimulatorController =====
    bool initGrid(const QtUItoAlgo::GridParameters& params) override;
    bool addFractures(const std::vector<QtUItoAlgo::FractureInput>& fractures) override;
    bool setFluidProperties(const QtUItoAlgo::FluidProperties& props) override;
    bool addWells(const std::vector<QtUItoAlgo::WellParameters>& wells) override;
    bool setSimulationParameters(const QtUItoAlgo::SimulationParameters& params) override;

    bool runSimulation() override;
    bool pauseSimulation() override;
    bool resumeSimulation() override;
    bool stopSimulation() override;

    std::string getSimulationStatus() override;
    double getCurrentTime() override;

    // ===== IDataTransfer =====
    bool exportResults(const std::string& output_dir) override;
    bool exportGeometry(const std::string& output_path) override;
    std::map<std::string, std::vector<double>> getProductionData() override;
    std::vector<std::tuple<double,double,double,double>> getPressureField() override;

private:
    void loop();

private:
    QtUItoAlgo::ISimulationCallback* callback_ = nullptr;

    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    std::thread worker_;

    std::mutex mtx_;
    double current_time_ = 0.0;
    std::string status_ = "Idle";

    // 先存起来，后面真算法接入时这些就是“真实参数”
    QtUItoAlgo::GridParameters grid_;
    std::vector<QtUItoAlgo::FractureInput> fractures_;
    QtUItoAlgo::FluidProperties fluid_;
    std::vector<QtUItoAlgo::WellParameters> wells_;
    QtUItoAlgo::SimulationParameters sim_;
};
