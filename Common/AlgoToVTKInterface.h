#ifndef ALGO_TO_VTK_INTERFACE_H
#define ALGO_TO_VTK_INTERFACE_H

#include <string>
#include <vector>
#include <map>

// Algo 到 VTK 的接口定义

namespace AlgoToVTK {

// 基础数据结构
struct Point3 {
    double x, y, z;
};

struct CellData {
    int id;
    Point3 center;
    double pressure;
    double Sw;
    double Sg;
};

struct FractureData {
    int id;
    std::vector<Point3> vertices;
    double aperture;
    double perm;
};

struct WellData {
    int id;
    Point3 position;
    double bhp;
    double rate;
};

struct GridData {
    int Nx, Ny, Nz;
    double Lx, Ly, Lz;
    double dx, dy, dz;
};

// 可视化数据接口
class IVisualizationData {
public:
    virtual ~IVisualizationData() = default;
    
    // 获取网格数据
    virtual GridData getGridData() = 0;
    
    // 获取单元数据
    virtual std::vector<CellData> getCellData() = 0;
    
    // 获取裂缝数据
    virtual std::vector<FractureData> getFractureData() = 0;
    
    // 获取井数据
    virtual std::vector<WellData> getWellData() = 0;
    
    // 获取产量数据
    virtual std::map<std::string, std::vector<double>> getProductionData() = 0;
    
    // 获取模拟时间点
    virtual std::vector<double> getTimeSteps() = 0;
};

// 数据更新接口
class IDataUpdate {
public:
    virtual ~IDataUpdate() = default;
    
    // 更新单元数据
    virtual bool updateCellData(const std::vector<CellData>& cell_data) = 0;
    
    // 更新裂缝数据
    virtual bool updateFractureData(const std::vector<FractureData>& fracture_data) = 0;
    
    // 更新井数据
    virtual bool updateWellData(const std::vector<WellData>& well_data) = 0;
    
    // 更新产量数据
    virtual bool updateProductionData(const std::map<std::string, std::vector<double>>& production_data) = 0;
    
    // 更新时间步
    virtual bool updateTimeStep(double time_step) = 0;
    
    // 重新生成裂缝
    virtual bool regenerateFractures(const std::vector<FractureData>& new_fractures) = 0;
};

// 可视化控制接口
class IVisualizationControl {
public:
    virtual ~IVisualizationControl() = default;
    
    // 显示压力场
    virtual bool showPressureField(bool show) = 0;
    
    // 显示饱和度场
    virtual bool showSaturationField(bool show) = 0;
    
    // 显示裂缝
    virtual bool showFractures(bool show) = 0;
    
    // 显示井
    virtual bool showWells(bool show) = 0;
    
    // 显示网格
    virtual bool showGrid(bool show) = 0;
    
    // 设置压力场颜色范围
    virtual bool setPressureRange(double min, double max) = 0;
    
    // 设置饱和度颜色范围
    virtual bool setSaturationRange(double min, double max) = 0;
    
    // 重置视图
    virtual bool resetView() = 0;
    
    // 保存当前视图
    virtual bool saveView(const std::string& file_path) = 0;
};

}

#endif // ALGO_TO_VTK_INTERFACE_H