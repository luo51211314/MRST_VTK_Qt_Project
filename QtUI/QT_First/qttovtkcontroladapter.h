#ifndef QTTOVTKCONTROLADAPTER_H
#define QTTOVTKCONTROLADAPTER_H

#pragma once
#include <string>
#include <vector>
#include <tuple>

// =======================================================
// QtToVtkControlAdapter：Qt侧实现 QtToVTK::IVisualizationControl 的适配器
// 说明：由于当前 QtToVTKInterface.h 存在类型顺序问题（GridParameters/WellParameters 未先定义）
//      我们默认不直接 include 原接口头，避免编译被卡死。
// 当接口文件修好后：把 QT_TOVTK_ENABLE_INTERFACE_IMPLEMENTATION 设为 1
// =======================================================

// ✅ 先默认关闭（等接口修好后你再改成 1）
#ifndef QT_TOVTK_ENABLE_INTERFACE_IMPLEMENTATION
#define QT_TOVTK_ENABLE_INTERFACE_IMPLEMENTATION
#endif

// ---- 前置声明（不依赖接口头的顺序） ----
namespace QtToVTK {
struct FractureParameters;
struct GridParameters;
struct WellParameters;


}

class VtkAdapter;
class VtkViewHost;

// =======================================================
// 开启实现（接口修好后）：继承并实现 QtToVTK::IVisualizationControl
// =======================================================
#include "QtToVTKInterface.h"

class QtToVtkControlAdapter final : public QtToVTK::IVisualizationControl
{
public:
    QtToVtkControlAdapter(VtkAdapter* vtk, VtkViewHost* host);

    bool updateFractures(const std::vector<QtToVTK::FractureParameters>& fractures) override;
    bool updatePressureField(const std::vector<std::tuple<double,double,double,double>>& pressure_data) override;
    bool updateSaturationField(const std::vector<std::tuple<double,double,double,double>>& saturation_data) override;
    bool updateGrid(const QtToVTK::GridParameters& grid_params) override;
    bool updateWells(const std::vector<QtToVTK::WellParameters>& wells) override;
    bool resetView() override;
    bool saveViewState(const std::string& file_path) override;
    bool loadViewState(const std::string& file_path) override;

    bool isEnabled() const { return true; }

private:
    VtkAdapter*  vtk_  = nullptr;
    VtkViewHost* host_ = nullptr;
};

#endif




