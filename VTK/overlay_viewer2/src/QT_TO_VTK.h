#ifndef QT_TO_VTK_INTERFACE_H
#define QT_TO_VTK_INTERFACE_H

#include <string>
#include <vector>
#include <map>

// Qt 到 VTK 的接口定义

namespace QtToVTK {

// 基础数据结构
struct Point3 {
    double x, y, z;
};

struct BoundingBox {
    Point3 min;
    Point3 max;
};

// 裂缝参数
struct FractureParameters {
    int id;
    double length;     // 裂缝长度
    double angle;      // 裂缝角度
    double dip;        // 裂缝倾角
    double aperture;   // 裂缝开度
    double perm;       // 裂缝渗透率
    Point3 center;     // 裂缝中心位置
};

// 区域选择参数
struct RegionSelection {
    BoundingBox bounds; // 选择区域边界
    bool include_fractures; // 是否包含裂缝
    bool include_cells; // 是否包含单元
};

// 网格参数
struct GridParameters {
    int Nx, Ny, Nz;
    double Lx, Ly, Lz;
};

// 井参数
struct WellParameters {
    int id;
    Point3 position;
    double bhp;
    double WI;
};

// 可视化更新类型
enum class UpdateType {
    FRACTURES,        // 裂缝更新
    PRESSURE_FIELD,   // 压力场更新
    SATURATION_FIELD, // 饱和度场更新
    GRID,             // 网格更新
    WELLS             // 井更新
};

// 可视化控制接口
class IVisualizationControl {
public:
    virtual ~IVisualizationControl() = default;
    
    // 更新裂缝数据
    virtual bool updateFractures(const std::vector<FractureParameters>& fractures) = 0;
    
    // 更新压力场数据
    virtual bool updatePressureField(const std::vector<std::tuple<double, double, double, double>>& pressure_data) = 0;
    
    // 更新饱和度场数据
    virtual bool updateSaturationField(const std::vector<std::tuple<double, double, double, double>>& saturation_data) = 0;
    
    // 更新网格数据
    virtual bool updateGrid(const GridParameters& grid_params) = 0;
    
    // 更新井数据
    virtual bool updateWells(const std::vector<WellParameters>& wells) = 0;
    
    // 重置视图
    virtual bool resetView() = 0;
    
    // 保存视图状态
    virtual bool saveViewState(const std::string& file_path) = 0;
    
    // 加载视图状态
    virtual bool loadViewState(const std::string& file_path) = 0;
};

// 数据传输接口
class IDataTransfer {
public:
    virtual ~IDataTransfer() = default;
    
    // 从本地备份加载数据
    virtual bool loadFromBackup(const std::string& backup_path) = 0;
    
    // 保存数据到本地备份
    virtual bool saveToBackup(const std::string& backup_path) = 0;
    
    // 获取当前裂缝数据
    virtual std::vector<FractureParameters> getCurrentFractures() = 0;
    
    // 获取当前网格参数
    virtual GridParameters getCurrentGrid() = 0;
    
    // 获取当前井数据
    virtual std::vector<WellParameters> getCurrentWells() = 0;
};

// 裂缝生成接口
class IFractureGenerator {
public:
    virtual ~IFractureGenerator() = default;
    
    // 生成裂缝
    virtual std::vector<FractureParameters> generateFractures(const FractureParameters& params, int count) = 0;
    
    // 修改现有裂缝
    virtual bool modifyFracture(int fracture_id, const FractureParameters& new_params) = 0;
    
    // 删除裂缝
    virtual bool deleteFracture(int fracture_id) = 0;
    
    // 批量修改裂缝
    virtual bool modifyFractures(const std::vector<int>& fracture_ids, const FractureParameters& new_params) = 0;
};

// 回调接口
class IUpdateCallback {
public:
    virtual ~IUpdateCallback() = default;
    
    // 可视化更新完成
    virtual void onVisualizationUpdated(UpdateType type) = 0;
    
    // 数据加载完成
    virtual void onDataLoaded() = 0;
    
    // 数据保存完成
    virtual void onDataSaved() = 0;
    
    // 操作失败
    virtual void onOperationFailed(const std::string& error_message) = 0;
};

}

#endif // QT_TO_VTK_INTERFACE_H
