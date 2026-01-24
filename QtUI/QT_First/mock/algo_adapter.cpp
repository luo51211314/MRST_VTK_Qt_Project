#include "algo_adapter.h"
#include <QDebug>

AlgoAdapter::AlgoAdapter()
{
    engine_ = std::make_unique<MockAlgo>();
}

AlgoAdapter::~AlgoAdapter() = default;

void AlgoAdapter::setCallback(QtUItoAlgo::ISimulationCallback* cb)
{
    engine_->setCallback(cb);
}

// ===== 参数下发：这里先打印 + 转发给 engine_ =====
bool AlgoAdapter::initGrid(const QtUItoAlgo::GridParameters& params)
{
    qDebug() << "[AlgoAdapter] initGrid"
             << params.Nx << params.Ny << params.Nz
             << params.Lx << params.Ly << params.Lz;
    return engine_->initGrid(params);
}

bool AlgoAdapter::addFractures(const std::vector<QtUItoAlgo::FractureInput>& fractures)
{
    qDebug() << "[AlgoAdapter] addFractures" << fractures.size();
    return engine_->addFractures(fractures);
}

bool AlgoAdapter::setFluidProperties(const QtUItoAlgo::FluidProperties& props)
{
    qDebug() << "[AlgoAdapter] setFluidProperties"
             << "mu_w" << props.mu_w << "mu_o" << props.mu_o << "mu_g" << props.mu_g;
    return engine_->setFluidProperties(props);
}

bool AlgoAdapter::addWells(const std::vector<QtUItoAlgo::WellParameters>& wells)
{
    qDebug() << "[AlgoAdapter] addWells" << wells.size();
    return engine_->addWells(wells);
}

bool AlgoAdapter::setSimulationParameters(const QtUItoAlgo::SimulationParameters& params)
{
    qDebug() << "[AlgoAdapter] setSimulationParameters"
             << params.total_time_days << params.initial_time_step
             << params.min_time_step << params.max_time_step;
    return engine_->setSimulationParameters(params);
}

// ===== 控制：同样转发 =====
bool AlgoAdapter::runSimulation()          { qDebug() << "[AlgoAdapter] runSimulation"; return engine_->runSimulation(); }
bool AlgoAdapter::pauseSimulation()        { qDebug() << "[AlgoAdapter] pauseSimulation"; return engine_->pauseSimulation(); }
bool AlgoAdapter::resumeSimulation()       { qDebug() << "[AlgoAdapter] resumeSimulation"; return engine_->resumeSimulation(); }
bool AlgoAdapter::stopSimulation()         { qDebug() << "[AlgoAdapter] stopSimulation"; return engine_->stopSimulation(); }
std::string AlgoAdapter::getSimulationStatus() { return engine_->getSimulationStatus(); }
double AlgoAdapter::getCurrentTime()       { return engine_->getCurrentTime(); }

// ===== IDataTransfer =====
bool AlgoAdapter::exportResults(const std::string& output_dir) { return engine_->exportResults(output_dir); }
bool AlgoAdapter::exportGeometry(const std::string& output_path) { return engine_->exportGeometry(output_path); }
std::map<std::string, std::vector<double>> AlgoAdapter::getProductionData() { return engine_->getProductionData(); }
std::vector<std::tuple<double,double,double,double>> AlgoAdapter::getPressureField() { return engine_->getPressureField(); }
