#ifndef VTK_TO_QT_INTERFACE_H
#define VTK_TO_QT_INTERFACE_H

#include <string>
#include <vector>
#include <functional>

// VTK 到 Qt 的接口定义

namespace VTKtoQt {

// 基础数据结构
struct Point3 {
    double x, y, z;
};

struct BoundingBox {
    Point3 min;
    Point3 max;
};

struct FractureSelection {
    int id;
    BoundingBox bbox;
    std::vector<Point3> vertices;
};

struct CellSelection {
    int id;
    Point3 center;
    double pressure;
    double Sw;
    double Sg;
};

struct WellSelection {
    int id;
    Point3 position;
    double bhp;
    double rate;
};

// 交互事件类型
enum class EventType {
    PICK_FRACTURE,
    PICK_CELL,
    PICK_WELL,
    BOX_SELECT_FRACTURES,
    BOX_SELECT_CELLS,
    REGION_SELECT,
    FRACTURE_PARAMETERS_CHANGED,
    FRACTURE_REGENERATED,
    PRESSURE_FIELD_GENERATED,
    MOUSE_MOVE,
    CAMERA_LOCK,
    CAMERA_UNLOCK
};

// 交互事件数据
struct EventData {
    EventType type;
    union {
        FractureSelection fracture;
        CellSelection cell;
        WellSelection well;
        std::vector<FractureSelection> fractures;
        std::vector<CellSelection> cells;
        BoundingBox region;
        FractureSelection fracture_parameters;
        std::vector<FractureSelection> regenerated_fractures;
        Point3 mouse_position;
        bool camera_locked;
    } data;
};

// 交互回调接口
class IInteractionCallback {
public:
    virtual ~IInteractionCallback() = default;
    
    // 处理交互事件
    virtual void onInteractionEvent(const EventData& event) = 0;
    
    // 处理鼠标移动
    virtual void onMouseMove(const Point3& world_position) = 0;
    
    // 处理相机状态变化
    virtual void onCameraStateChanged(bool locked) = 0;
};

// 可视化状态接口
class IVisualizationState {
public:
    virtual ~IVisualizationState() = default;
    
    // 获取当前选中的裂缝
    virtual std::vector<FractureSelection> getSelectedFractures() = 0;
    
    // 获取当前选中的单元
    virtual std::vector<CellSelection> getSelectedCells() = 0;
    
    // 获取当前选中的井
    virtual std::vector<WellSelection> getSelectedWells() = 0;
    
    // 获取相机锁定状态
    virtual bool isCameraLocked() = 0;
    
    // 获取当前视图的边界框
    virtual BoundingBox getCurrentViewBounds() = 0;
};

// 可视化命令接口
class IVisualizationCommand {
public:
    virtual ~IVisualizationCommand() = default;
    
    // 高亮裂缝
    virtual bool highlightFractures(const std::vector<int>& fracture_ids) = 0;
    
    // 高亮单元
    virtual bool highlightCells(const std::vector<int>& cell_ids) = 0;
    
    // 高亮井
    virtual bool highlightWells(const std::vector<int>& well_ids) = 0;
    
    // 清除所有高亮
    virtual bool clearHighlights() = 0;
    
    // 设置相机位置
    virtual bool setCameraPosition(const Point3& position, const Point3& focal_point, const Point3& view_up) = 0;
    
    // 放大到选中对象
    virtual bool zoomToSelection() = 0;
    
    // 放大到指定边界框
    virtual bool zoomToBounds(const BoundingBox& bounds) = 0;
    
    // 重置视图
    virtual bool resetView() = 0;
};

// HUD 显示接口
class IHUDDisplay {
public:
    virtual ~IHUDDisplay() = default;
    
    // 显示拾取信息
    virtual bool showPickInfo(const std::string& info) = 0;
    
    // 显示框选信息
    virtual bool showBoxSelectInfo(const std::string& info) = 0;
    
    // 显示鼠标位置
    virtual bool showMousePosition(const Point3& position) = 0;
    
    // 显示相机状态
    virtual bool showCameraState(bool locked) = 0;
    
    // 显示模拟时间
    virtual bool showSimulationTime(double time) = 0;
    
    // 清除 HUD 信息
    virtual bool clearHUD() = 0;
};

}

#endif // VTK_TO_QT_INTERFACE_H