#ifndef QTUI_TO_ALGO_INTERFACE_H
#define QTUI_TO_ALGO_INTERFACE_H

#include <string>
#include <vector>
#include <map>
#include <cstdint> // 添加此头文件以支持 uint16_t

// QtUI 到 Algo 的接口定义

namespace QtUItoAlgo {

// 基础数据结构
struct Point3 {
    double x, y, z;
};

struct FractureInput {
    int id;
    Point3 vertices[4]; // 四个顶点，定义一个四边形面
    double aperture;    // 开度
    double perm;        // 渗透率
};

struct GridParameters {
    int Nx, Ny, Nz;
    double Lx, Ly, Lz;
    
    // LGR (局部网格加密) 参数
    bool enable_lgr = true;
    double d_threshold = 30.0;     // 影响区域阈值
    uint16_t lgr_Nrx = 4;          // X方向加密数
    uint16_t lgr_Nry = 4;          // Y方向加密数
    uint16_t lgr_Nrz = 2;          // Z方向加密数
};

struct FluidProperties {
    // 粘度 (cP)
    double mu_w = 1.0;
    double mu_o = 5.0;
    double mu_g = 0.2;
    
    // 压缩系数 (1/bar)
    double cw = 1e-8;
    double co = 1e-5;
    double cg = 1e-3;
    
    // 参考压力 (bar)
    double P_ref = 100.0;
    
    // 相对渗透率端点
    double Swi = 0.2;
    double Sor = 0.2;
    double Sgc = 0.05;
};

struct WellParameters {
    int target_fracture_id;
    double WI;
    double P_bhp;
};

struct SimulationParameters {
    double total_time_days;
    double initial_time_step;
    double min_time_step;
    double max_time_step;
};

// 表示单个节点的物理状态
struct NodeState {
    int node_id;
    double x, y, z;
    double P;   // 压力
    double Sw;  // 含水饱和度
    double Sg;  // 含气饱和度
    bool is_fracture; // true 为裂缝段(Segment)，false 为基质(Leaf)
};

// 模拟器控制接口
class ISimulatorController {
public:
    virtual ~ISimulatorController() = default;
    
    // 初始化网格
    virtual bool initGrid(const GridParameters& params) = 0;
    
    // 添加裂缝
    virtual bool addFractures(const std::vector<FractureInput>& fractures) = 0;
    
    // 设置流体属性
    virtual bool setFluidProperties(const FluidProperties& props) = 0;
    
    // 添加井
    virtual bool addWells(const std::vector<WellParameters>& wells) = 0;
    
    // 设置模拟参数
    virtual bool setSimulationParameters(const SimulationParameters& params) = 0;
    
    // 运行模拟
    virtual bool runSimulation() = 0;
    
    // 异步启动模拟计算（后台线程）
    virtual bool runSimulationAsync() = 0;
    
    // 同步执行单个时间步 (便于 UI 逐帧渲染或调试)
    virtual bool stepSimulation() = 0; 
    
    // 暂停模拟
    virtual bool pauseSimulation() = 0;
    
    // 继续模拟
    virtual bool resumeSimulation() = 0;
    
    // 停止模拟
    virtual bool stopSimulation() = 0;
    
    // 获取模拟状态
    virtual std::string getSimulationStatus() = 0;
    
    // 获取当前模拟时间
    virtual double getCurrentTime() = 0;
};

// 数据传输接口
class IDataTransfer {
public:
    virtual ~IDataTransfer() = default;
    
    // 导出模拟结果
    virtual bool exportResults(const std::string& output_dir) = 0;
    
    // 导出几何信息
    virtual bool exportGeometry(const std::string& output_path) = 0;
    
    // 获取产量数据
    virtual std::map<std::string, std::vector<double>> getProductionData() = 0;
    
    // 获取场数据 (替换原有的 getPressureField，输出完整的三相流场数据)
    virtual std::vector<NodeState> getFieldData() = 0;
};

// 回调接口
class ISimulationCallback {
public:
    virtual ~ISimulationCallback() = default;
    
    // 模拟进度更新
    virtual void onProgressUpdate(double progress, double current_time) = 0;
    
    // 模拟完成
    virtual void onSimulationCompleted() = 0;
    
    // 模拟失败
    virtual void onSimulationFailed(const std::string& error_message) = 0;
    
    // 时间步长调整
    virtual void onTimeStepChanged(double new_time_step) = 0;
};

}

#endif // QTUI_TO_ALGO_INTERFACE_H