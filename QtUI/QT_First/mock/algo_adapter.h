#pragma once
#include <memory>
#include <string>

#include "QtUItoAlgoInterface.h"
//#include "mockalgo.h"   // 先复用现有 MockAlgo（保证功能不掉）
#include "edfm_3d_blackoil_integrated_simulator.h" // 真实 simulator 的头

// AlgoAdapter：未来这里将调用真实算法模块；现在先“转发给 MockAlgo”
class AlgoAdapter : public QtUItoAlgo::ISimulatorController,
                    public QtUItoAlgo::IDataTransfer
{
public:
    AlgoAdapter();
    ~AlgoAdapter() override;

    // UI 侧会把 callbackBridge_ 传进来
    void setCallback(QtUItoAlgo::ISimulationCallback* cb);

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
    // 当前阶段：复用 MockAlgo；后面换成真实 Algo 引擎即可
    //std::unique_ptr<MockAlgo> engine_;
    std::unique_ptr<Simulator> engine_;

};
