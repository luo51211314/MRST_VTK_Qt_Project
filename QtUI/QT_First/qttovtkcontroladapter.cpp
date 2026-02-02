#include "qttovtkcontroladapter.h"
#include <tuple>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include "VtkAdapter.h"
#include "VtkViewHost.h"
#include "QtToVTKInterface.h"

QtToVtkControlAdapter::QtToVtkControlAdapter(VtkAdapter* vtk, VtkViewHost* host)
    : vtk_(vtk), host_(host)
{}

// 真正转发到 VtkAdapter

bool QtToVtkControlAdapter::updateFractures(
    const std::vector<QtToVTK::FractureParameters>& fractures)
{
    if (!vtk_) return false;
    vtk_->setFractures(fractures);
    return true;
}

bool QtToVtkControlAdapter::updatePressureField(
    const std::vector<std::tuple<double,double,double,double>>& pressure_data)
{
    if (!vtk_) return false;
    vtk_->setPressureField(pressure_data);
    return true;
}

bool QtToVtkControlAdapter::updateSaturationField(
    const std::vector<std::tuple<double,double,double,double>>& saturation_data)
{
    if (!vtk_) return false;
    vtk_->setSaturationField(saturation_data);
    return true;
}

bool QtToVtkControlAdapter::updateGrid(const QtToVTK::GridParameters& grid_params)
{
    if (!vtk_) return false;
    vtk_->setGrid(grid_params);
    return true;
}

bool QtToVtkControlAdapter::updateWells(
    const std::vector<QtToVTK::WellParameters>& wells)
{
    if (!vtk_) return false;
    vtk_->setWells(wells);
    return true;
}

bool QtToVtkControlAdapter::resetView()
{
    if (!vtk_) return false;
    vtk_->resetViewState();   // 你在 VtkAdapter 里新增的那个 reset
    return true;
}

bool QtToVtkControlAdapter::saveViewState(const std::string& file_path)
{
    if (!vtk_) return false;
    return vtk_->saveViewState(QString::fromStdString(file_path));
}

bool QtToVtkControlAdapter::loadViewState(const std::string& file_path)
{
    if (!vtk_) return false;
    return vtk_->loadViewState(QString::fromStdString(file_path));
}

