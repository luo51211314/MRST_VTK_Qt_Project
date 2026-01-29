#include "QtToVtkControlAdapter.h"

#if QT_TOVTK_ENABLE_INTERFACE_IMPLEMENTATION

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

// 下面先给最小实现（后续我们再转发到 VtkAdapter）
bool QtToVtkControlAdapter::updateFractures(const std::vector<QtToVTK::FractureParameters>&)
{
    return vtk_ != nullptr;
}

bool QtToVtkControlAdapter::updatePressureField(const std::vector<std::tuple<double,double,double,double>>&)
{
    return vtk_ != nullptr;
}

bool QtToVtkControlAdapter::updateSaturationField(const std::vector<std::tuple<double,double,double,double>>&)
{
    return vtk_ != nullptr;
}

bool QtToVtkControlAdapter::updateGrid(const QtToVTK::GridParameters&)
{
    return vtk_ != nullptr;
}

bool QtToVtkControlAdapter::updateWells(const std::vector<QtToVTK::WellParameters>&)
{
    return vtk_ != nullptr;
}

bool QtToVtkControlAdapter::resetView()
{
    return vtk_ != nullptr;
}

bool QtToVtkControlAdapter::saveViewState(const std::string& file_path)
{
    // 先放一个最小可用：写一个空json，保证接口可调用
    QFile f(QString::fromStdString(file_path));
    if (!f.open(QIODevice::WriteOnly)) return false;
    QJsonObject o;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return true;
}

bool QtToVtkControlAdapter::loadViewState(const std::string& file_path)
{
    QFile f(QString::fromStdString(file_path));
    if (!f.open(QIODevice::ReadOnly)) return false;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    return doc.isObject();
}

#endif
