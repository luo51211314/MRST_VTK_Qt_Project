#include "mockalgo.h"
#include <QDebug>
#include <chrono>

MockAlgo::MockAlgo() {}
MockAlgo::~MockAlgo()
{
    stopSimulation();
}

bool MockAlgo::initGrid(const QtUItoAlgo::GridParameters& params)
{
    std::lock_guard<std::mutex> lk(mtx_);
    grid_ = params;
    status_ = "GridInited";
    return true;
}

bool MockAlgo::addFractures(const std::vector<QtUItoAlgo::FractureInput>& fractures)
{
    std::lock_guard<std::mutex> lk(mtx_);
    fractures_ = fractures;
    status_ = "FracturesAdded";
    return true;
}

bool MockAlgo::setFluidProperties(const QtUItoAlgo::FluidProperties& props)
{
    std::lock_guard<std::mutex> lk(mtx_);
    fluid_ = props;
    status_ = "FluidSet";
    return true;
}

bool MockAlgo::addWells(const std::vector<QtUItoAlgo::WellParameters>& wells)
{
    std::lock_guard<std::mutex> lk(mtx_);
    wells_ = wells;
    status_ = "WellsAdded";
    return true;
}

bool MockAlgo::setSimulationParameters(const QtUItoAlgo::SimulationParameters& params)
{
    std::lock_guard<std::mutex> lk(mtx_);
    sim_ = params;
    status_ = "SimParamsSet";
    return true;
}

bool MockAlgo::runSimulation()
{
    qDebug() << "[MockAlgo] runSimulation() called, running_=" << running_ << " paused_=" << paused_;
    // 已在跑就不重复启动
    if (running_) return false;

    running_ = true;
    paused_ = false;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        status_ = "Running";
        current_time_ = 0.0;
    }

    worker_ = std::thread(&MockAlgo::loop, this);
    qDebug() << "[MockAlgo] worker thread started";
    return true;
}

bool MockAlgo::pauseSimulation()
{
    if (!running_) return false;
    paused_ = true;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        status_ = "Paused";
    }
    return true;
}

bool MockAlgo::resumeSimulation()
{
    if (!running_) return false;
    paused_ = false;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        status_ = "Running";
    }
    return true;
}

bool MockAlgo::stopSimulation()
{
    if (!running_ && !worker_.joinable()) return true;

    running_ = false;
    paused_ = false;

    if (worker_.joinable())
        worker_.join();

    {
        std::lock_guard<std::mutex> lk(mtx_);
        status_ = "Stopped";
    }
    return true;
}

std::string MockAlgo::getSimulationStatus()
{
    std::lock_guard<std::mutex> lk(mtx_);
    return status_;
}

double MockAlgo::getCurrentTime()
{
    std::lock_guard<std::mutex> lk(mtx_);
    return current_time_;
}

void MockAlgo::loop()
{
    qDebug() << "[MockAlgo] loop() entered";
    double progress = 0.0;
    double dt = 0.1;

    while (running_ && progress < 1.0)
    {
        if (paused_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            continue;
        }

        progress += 0.02;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            current_time_ += dt;
        }

        if (!callback_) {
            // 这个一旦出现，说明 setCallback 没传进来或被覆盖了
            qDebug() << "[MockAlgo] callback_ is NULL!";
        } else {
            // 每 10% 打一次，避免刷屏
            int pct = int(progress * 100.0);
            if (pct % 10 == 0) {
                qDebug() << "[MockAlgo] progress=" << pct << "% time=" << getCurrentTime();
            }
            callback_->onProgressUpdate(progress, getCurrentTime());
            if (pct % 10 == 0)
                callback_->onTimeStepChanged(dt);
        }


        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (running_) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            status_ = "Completed";
        }
        running_ = false;
        qDebug() << "[MockAlgo] completed, emit onSimulationCompleted()";
        if (callback_) callback_->onSimulationCompleted();
    }
}

// ===== IDataTransfer：先给“可用的假实现” =====
bool MockAlgo::exportResults(const std::string&) { return true; }
bool MockAlgo::exportGeometry(const std::string&) { return true; }

std::map<std::string, std::vector<double>> MockAlgo::getProductionData()
{
    // 简单 mock：一条曲线
    return { {"oil_rate", {1,2,3,4,5}} };
}

std::vector<std::tuple<double,double,double,double>> MockAlgo::getPressureField()
{
    // 简单 mock：几个点 (x,y,z,value)
    return {
            {0,0,0, 100.0},
            {1,0,0, 101.0},
            {0,1,0,  99.5},
            };
}
