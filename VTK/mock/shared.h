#ifndef SHARED_H
#define SHARED_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <tuple>

// VTK头文件
#include <vtkSmartPointer.h>
#include <vtkStructuredGrid.h>
#include <vtkPoints.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkDataSetMapper.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkLookupTable.h>
#include <vtkScalarBarActor.h>
#include <vtkTextProperty.h>
#include <vtkCamera.h>
#include <vtkProperty.h>
#include <vtkColorTransferFunction.h>
#include <vtkGeometryFilter.h>
#include <vtkWindowToImageFilter.h>
#include <vtkPNGWriter.h>
#include <vtkTransform.h>
#include <vtkTransformFilter.h>
#include <vtkLine.h>
#include <vtkPolyData.h>
#include <vtkCylinderSource.h>
#include <vtkCellArray.h>
#include <vtkMath.h>

// 辅助函数
extern std::string trim(const std::string& str);

// 数据结构定义
struct GridInfo {
    int nx, ny, nz;
    double Lx, Ly, Lz;
    int npx, npy;
    double dx, dy, dz;
};

struct Fracture {
    int id;
    std::string type; // "natural" or "artificial"
    std::vector<std::tuple<double, double, double>> points;
};

struct WellInfo {
    int well_id;
    int node_idx;
    std::string type;
    double x;
    double y;
    double z;
    double WI;
    double P_bhp;
};

struct FieldData {
    std::vector<double> x_coords;
    std::vector<double> y_coords;
    std::vector<double> z_coords;
    std::vector<double> pressure;
    int nx, ny, nz;
    double p_min, p_max;
};

// 数据读取函数（CSV版本）
extern GridInfo readGridInfo(const std::string& filename);
extern std::vector<Fracture> readFractures(const std::string& filename);
extern FieldData readFieldData(const std::string& filename);
extern std::vector<WellInfo> readWells(const std::string& filename);

// 可视化函数
extern vtkSmartPointer<vtkLookupTable> createLookupTable(double minP, double maxP);
extern vtkSmartPointer<vtkScalarBarActor> createColorBar(vtkLookupTable* lut);
extern void createStackedLayersScene(vtkRenderer* renderer, const FieldData& data, vtkLookupTable* lut, double zScaleFactor = 5.0);
extern void createSolidBlockScene(vtkRenderer* renderer, const FieldData& data, vtkLookupTable* lut);
extern void drawFractures(vtkRenderer* renderer, const std::vector<Fracture>& fractures, double zScaleFactor = 1.0);
extern void drawWells(vtkRenderer* renderer, const std::vector<WellInfo>& wells, const std::vector<Fracture>& fractures, double zScaleFactor = 1.0);
extern void saveImage(vtkRenderWindow* renderWindow, const std::string& filename);
extern void setupCamera(vtkRenderer* renderer, double cx, double cy, double cz, double maxLen, bool isStacked);
extern void createFracturesWellsScene(vtkRenderer* renderer, const std::vector<Fracture>& fractures, const std::vector<WellInfo>& wells, double zScaleFactor = 1.0);

// 配置文件读取函数
extern bool readConfig(const std::string& filename, std::string& gridFile, std::string& fieldFile, std::string& fractureFile, std::string& wellFile);

// ============================
// NEW: MockData -> shared 输入
// ============================
// 只 forward declare，避免 shared.h 必须 include mock_data.h
namespace MockData {
    struct GridData;
    struct CellData;
    struct FractureData;
    struct WellData;
}

extern GridInfo GridInfoFromMock(const MockData::GridData& g);

extern FieldData FieldDataFromMockCells(
    const MockData::GridData& g,
    const std::vector<MockData::CellData>& cells
);

extern std::vector<Fracture> FracturesFromMock(
    const std::vector<MockData::FractureData>& fracs
);

extern std::vector<WellInfo> WellsFromMock(
    const std::vector<MockData::WellData>& wells
);

// 一次性构建 shared 渲染输入（推荐 runtime 直接调用这个）
extern bool BuildSharedInputsFromMock(
    const MockData::GridData& g,
    const std::vector<MockData::CellData>& cells,
    const std::vector<MockData::FractureData>& fracs,
    const std::vector<MockData::WellData>& wells,
    GridInfo& outGrid,
    FieldData& outField,
    std::vector<Fracture>& outFracs,
    std::vector<WellInfo>& outWells
);

#endif // SHARED_H

