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
    int parent_id;     // 适配 LGR：记录所属粗网格 ID
    Point3 center;
    double dx, dy, dz; // 适配 LGR：每个细分网格尺寸不同，必须独立传递
    double pressure;
    double Sw;
    double Sg;
};

// 全局物理裂缝几何数据 (仅用于轮廓显示)
struct FractureData {
    int id;
    std::vector<Point3> vertices;
    double aperture;
    double perm;
};

// 适配 EDFM：相交裂缝段数据 (带物理状态的计算节点)
struct SegmentData {
    int id;
    int frac_id;
    int matrix_leaf_id;
    std::vector<Point3> poly_vertices; // 裁剪后的多边形顶点，用于 VTK 渲染多边形面
    Point3 center;
    double pressure;
    double Sw;
    double Sg;
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
    // 移除了全局的 dx, dy, dz，因为 LGR 导致网格非均一，尺寸由 CellData 提供
};

// 可视化数据接口
class IVisualizationData {
public:
    virtual ~IVisualizationData() = default;
    
    // 获取网格数据
    virtual GridData getGridData() = 0;
    
    // 获取单元数据 (包含 LGR 细网格)
    virtual std::vector<CellData> getCellData() = 0;
    
    // 获取全局裂缝轮廓数据
    virtual std::vector<FractureData> getFractureData() = 0;

    // 获取 EDFM 裂缝段数据及状态
    virtual std::vector<SegmentData> getSegmentData() = 0;
    
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
    
    // 更新全局裂缝数据
    virtual bool updateFractureData(const std::vector<FractureData>& fracture_data) = 0;

    // 更新 EDFM 裂缝段状态数据
    virtual bool updateSegmentData(const std::vector<SegmentData>& segment_data) = 0;
    
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
    
    // 显示含水饱和度场 (拆分水气)
    virtual bool showWaterSaturationField(bool show) = 0;

    // 显示含气饱和度场 (拆分水气)
    virtual bool showGasSaturationField(bool show) = 0;
    
    // 显示全局裂缝轮廓
    virtual bool showFractures(bool show) = 0;

    // 显示 EDFM 裂缝段场数据
    virtual bool showSegments(bool show) = 0;
    
    // 显示井
    virtual bool showWells(bool show) = 0;
    
    // 显示网格
    virtual bool showGrid(bool show) = 0;
    
    // 设置压力场颜色范围
    virtual bool setPressureRange(double min, double max) = 0;
    
    // 设置含水饱和度颜色范围
    virtual bool setWaterSaturationRange(double min, double max) = 0;

    // 设置含气饱和度颜色范围
    virtual bool setGasSaturationRange(double min, double max) = 0;
    
    // 重置视图
    virtual bool resetView() = 0;
    
    // 保存当前视图
    virtual bool saveView(const std::string& file_path) = 0;
};

}

#endif // ALGO_TO_VTK_INTERFACE_H