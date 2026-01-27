#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <unistd.h>
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
#include <vtkGeometryFilter.h> // 用于提取几何表面
#include <vtkWindowToImageFilter.h>
#include <vtkPNGWriter.h>
#include <vtkTransform.h>
#include <vtkTransformFilter.h>
#include <vtkLine.h>
#include <vtkPolyData.h>

// ----------------------
// 辅助函数
// ----------------------

// 去除字符串前后空格
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

// ----------------------
// 数据结构定义
// ----------------------

// 网格信息结构
struct GridInfo {
    int nx, ny, nz;
    double Lx, Ly, Lz;
    int npx, npy;
    double dx, dy, dz;
};

// 裂缝结构
struct Fracture {
    int id;
    std::vector<std::tuple<double, double, double>> points;
};

// 读取裂缝数据函数
std::vector<Fracture> readFractures(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open fracture_geometry.csv: " + filename);
    }

    std::vector<Fracture> fractures;
    std::string line;
    int line_count = 0;

    // 读取标题行
    if (std::getline(file, line)) {
        line_count++;
    }

    // 读取数据行
    while (std::getline(file, line)) {
        line_count++;
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> values;

        while (std::getline(ss, token, ',')) {
            values.push_back(trim(token));
        }

        if (values.size() >= 13) {
            try {
                Fracture frac;
                frac.id = std::stoi(values[0]);
                
                // 读取四个角点
                for (int i = 0; i < 4; i++) {
                    int index = 1 + i * 3;
                    double x = std::stod(values[index]);
                    double y = std::stod(values[index + 1]);
                    double z = std::stod(values[index + 2]);
                    frac.points.emplace_back(x, y, z);
                }

                fractures.push_back(frac);
            } catch (const std::exception& e) {
                std::cerr << "Warning: Error parsing fracture at line " << line_count << ": " << e.what() << std::endl;
            }
        }
    }

    std::cout << "Read " << fractures.size() << " fractures from " << filename << std::endl;
    return fractures;
}

// 绘制裂缝函数
void drawFractures(vtkRenderer* renderer, const std::vector<Fracture>& fractures, double zScaleFactor = 1.0) {
    // 创建点集和线段
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();

    // 绘制裂缝边界
    for (const auto& fracture : fractures) {
        if (fracture.points.size() >= 2) {
            // 绘制裂缝的四条边界线
            for (size_t i = 0; i < fracture.points.size(); i++) {
                size_t next_i = (i + 1) % fracture.points.size();
                
                double x0 = std::get<0>(fracture.points[i]);
                double y0 = std::get<1>(fracture.points[i]);
                double z0 = std::get<2>(fracture.points[i]) * zScaleFactor;
                
                double x1 = std::get<0>(fracture.points[next_i]);
                double y1 = std::get<1>(fracture.points[next_i]);
                double z1 = std::get<2>(fracture.points[next_i]) * zScaleFactor;

                vtkIdType p1 = points->InsertNextPoint(x0, y0, z0);
                vtkIdType p2 = points->InsertNextPoint(x1, y1, z1);

                // 创建线段
                vtkIdType lineIds[2] = {p1, p2};
                lines->InsertNextCell(2, lineIds);
            }
        }
    }

    // 创建PolyData
    vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetLines(lines);

    // 创建映射器
    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(polyData);

    // 创建Actor
    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0.0, 0.0, 0.0); // 黑色裂缝
    actor->GetProperty()->SetLineWidth(2.0);
    actor->GetProperty()->SetOpacity(1.0);

    renderer->AddActor(actor);
}

// 三维数据结构
struct FieldData {
    std::vector<double> x_coords;
    std::vector<double> y_coords;
    std::vector<double> z_coords;
    std::vector<double> pressure;
    int nx, ny, nz;
    double p_min, p_max; // 存储全局最大最小压力
};

// ----------------------
// 数据读取辅助函数
// ----------------------

GridInfo readGridInfo(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) throw std::runtime_error("Could not open grid_info.csv: " + filename);

    GridInfo grid;
    std::string line;
    if (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> values;
        while (std::getline(ss, token, ',')) values.push_back(trim(token));

        if (values.size() != 9) throw std::runtime_error("Invalid grid_info format");
        
        grid.nx = std::stoi(values[0]);
        grid.ny = std::stoi(values[1]);
        grid.nz = std::stoi(values[2]);
        grid.Lx = std::stod(values[3]);
        grid.Ly = std::stod(values[4]);
        grid.Lz = std::stod(values[5]);
        grid.dx = std::stod(values[8]);
        grid.dy = grid.Ly / grid.ny;
        grid.dz = grid.Lz / grid.nz;
    }
    return grid;
}

FieldData readFieldData(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) throw std::runtime_error("Could not open final_field.csv");

    FieldData fieldData;
    std::string line;
    std::vector<std::tuple<double, double, double, double>> points;

    // 跳过标题
    std::getline(file, line);

    while (std::getline(file, line)) {
        if (trim(line).empty()) continue;
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> values;
        while (std::getline(ss, token, ',')) values.push_back(trim(token));

        if (values.size() >= 4) {
            points.emplace_back(std::stod(values[0]), std::stod(values[1]), std::stod(values[2]), std::stod(values[3]));
        }
    }

    if (points.empty()) throw std::runtime_error("No valid data");

    // 提取坐标和维度
    std::vector<double> unique_x, unique_y, unique_z;
    for (const auto& p : points) {
        double x = std::get<0>(p);
        double y = std::get<1>(p);
        double z = std::get<2>(p);
        
        bool found_x = false; for(double v : unique_x) if(std::abs(v-x)<1e-6) found_x=true;
        if(!found_x) unique_x.push_back(x);
        
        bool found_y = false; for(double v : unique_y) if(std::abs(v-y)<1e-6) found_y=true;
        if(!found_y) unique_y.push_back(y);
        
        bool found_z = false; for(double v : unique_z) if(std::abs(v-z)<1e-6) found_z=true;
        if(!found_z) unique_z.push_back(z);
    }

    std::sort(unique_x.begin(), unique_x.end());
    std::sort(unique_y.begin(), unique_y.end());
    std::sort(unique_z.begin(), unique_z.end());

    fieldData.nx = unique_x.size();
    fieldData.ny = unique_y.size();
    fieldData.nz = unique_z.size();
    fieldData.x_coords = unique_x;
    fieldData.y_coords = unique_y;
    fieldData.z_coords = unique_z;
    fieldData.pressure.resize(fieldData.nx * fieldData.ny * fieldData.nz, 0.0);

    // 填充数据并寻找最大最小值
    double minP = 1e30, maxP = -1e30;

    for (const auto& point : points) {
        double x = std::get<0>(point);
        double y = std::get<1>(point);
        double z = std::get<2>(point);
        double p = std::get<3>(point);

        // 简单的网格查找 (假设规则网格)
        int i = -1, j = -1, k = -1;
        for(int idx=0; idx<fieldData.nx; ++idx) if(std::abs(unique_x[idx]-x)<1e-6) i=idx;
        for(int idx=0; idx<fieldData.ny; ++idx) if(std::abs(unique_y[idx]-y)<1e-6) j=idx;
        for(int idx=0; idx<fieldData.nz; ++idx) if(std::abs(unique_z[idx]-z)<1e-6) k=idx;

        if (i>=0 && j>=0 && k>=0) {
            fieldData.pressure[k * fieldData.nx * fieldData.ny + j * fieldData.nx + i] = p;
            if (p < minP) minP = p;
            if (p > maxP) maxP = p;
        }
    }
    fieldData.p_min = minP;
    fieldData.p_max = maxP;

    std::cout << "Data Range: Pressure [" << minP << ", " << maxP << "]" << std::endl;
    return fieldData;
}

// ----------------------
// 可视化核心函数
// ----------------------

// 创建共享的颜色映射表
vtkSmartPointer<vtkLookupTable> createLookupTable(double minP, double maxP) {
    vtkSmartPointer<vtkLookupTable> lut = vtkSmartPointer<vtkLookupTable>::New();
    lut->SetTableRange(minP, maxP);
    lut->SetHueRange(0.667, 0.0); // 蓝(低压) -> 红(高压)
    lut->SetSaturationRange(1.0, 1.0);
    lut->SetValueRange(1.0, 1.0);
    lut->SetNumberOfTableValues(256);
    lut->Build();
    return lut;
}

// 创建标量条 (Color Bar)
vtkSmartPointer<vtkScalarBarActor> createColorBar(vtkLookupTable* lut) {
    vtkSmartPointer<vtkScalarBarActor> scalarBar = vtkSmartPointer<vtkScalarBarActor>::New();
    scalarBar->SetLookupTable(lut);
    scalarBar->SetTitle("Pressure (MPa)");
    scalarBar->SetNumberOfLabels(5);
    scalarBar->SetWidth(0.1);
    scalarBar->SetHeight(0.5);
    scalarBar->GetLabelTextProperty()->SetColor(1, 1, 1); // 白色文字
    scalarBar->GetTitleTextProperty()->SetColor(1, 1, 1);
    return scalarBar;
}

// 场景 1: 分层堆叠切片 (模拟手绘图)
// 参数 zScaleFactor 用于拉伸Z轴，让层与层之间分开
void createStackedLayersScene(vtkRenderer* renderer, const FieldData& data, vtkLookupTable* lut, double zScaleFactor = 5.0) {
    
    // 我们不需要画出每一层，那样会变成一个实心块。
    // 我们只画几层代表性的，比如底层、顶层和中间的几层。
    int num_layers_to_show = 5; 
    int step = std::max(1, data.nz / (num_layers_to_show - 1));
    
    // 如果层数太少，就逐层画
    if (data.nz <= 5) step = 1;

    for (int k = 0; k < data.nz; k += step) {
        // 为每一层创建一个结构化网格
        vtkSmartPointer<vtkStructuredGrid> layerGrid = vtkSmartPointer<vtkStructuredGrid>::New();
        layerGrid->SetDimensions(data.nx, data.ny, 1); // 这是一个2D平面

        vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
        vtkSmartPointer<vtkDoubleArray> pressureArr = vtkSmartPointer<vtkDoubleArray>::New();
        pressureArr->SetName("Pressure");

        for (int j = 0; j < data.ny; j++) {
            for (int i = 0; i < data.nx; i++) {
                // 原始Z坐标
                double originalZ = data.z_coords[k];
                // 视觉上的Z坐标 (应用拉伸因子，模拟堆叠效果)
                double visualZ = originalZ * zScaleFactor; 
                
                points->InsertNextPoint(data.x_coords[i], data.y_coords[j], visualZ);
                
                // 获取对应压力值
                int idx = k * data.nx * data.ny + j * data.nx + i;
                pressureArr->InsertNextValue(data.pressure[idx]);
            }
        }

        layerGrid->SetPoints(points);
        layerGrid->GetPointData()->SetScalars(pressureArr);

        // 使用GeometryFilter将网格转换为多边形以便渲染
        vtkSmartPointer<vtkGeometryFilter> geomFilter = vtkSmartPointer<vtkGeometryFilter>::New();
        geomFilter->SetInputData(layerGrid);

        vtkSmartPointer<vtkDataSetMapper> mapper = vtkSmartPointer<vtkDataSetMapper>::New();
        mapper->SetInputConnection(geomFilter->GetOutputPort());
        mapper->SetLookupTable(lut);
        mapper->SetScalarRange(data.p_min, data.p_max);

        vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        
        // 稍微加一点透明度，看起来更像玻璃片
        actor->GetProperty()->SetOpacity(0.9);
        // 去掉网格线，或者保留看你喜好，这里去掉让颜色更纯粹
        actor->GetProperty()->SetEdgeVisibility(1); 
        actor->GetProperty()->SetEdgeColor(0.2, 0.2, 0.2);
        actor->GetProperty()->SetLineWidth(0.5);

        renderer->AddActor(actor);
    }
}

// 场景 2: 实体长方体 (Solid Block)
void createSolidBlockScene(vtkRenderer* renderer, const FieldData& data, vtkLookupTable* lut) {
    vtkSmartPointer<vtkStructuredGrid> grid = vtkSmartPointer<vtkStructuredGrid>::New();
    grid->SetDimensions(data.nx, data.ny, data.nz);

    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkDoubleArray> pressureArr = vtkSmartPointer<vtkDoubleArray>::New();
    pressureArr->SetName("Pressure");

    for (int k = 0; k < data.nz; k++) {
        for (int j = 0; j < data.ny; j++) {
            for (int i = 0; i < data.nx; i++) {
                points->InsertNextPoint(data.x_coords[i], data.y_coords[j], data.z_coords[k]);
                int idx = k * data.nx * data.ny + j * data.nx + i;
                pressureArr->InsertNextValue(data.pressure[idx]);
            }
        }
    }

    grid->SetPoints(points);
    grid->GetPointData()->SetScalars(pressureArr);

    // 提取外表面 (Skin)
    vtkSmartPointer<vtkGeometryFilter> geomFilter = vtkSmartPointer<vtkGeometryFilter>::New();
    geomFilter->SetInputData(grid);

    vtkSmartPointer<vtkDataSetMapper> mapper = vtkSmartPointer<vtkDataSetMapper>::New();
    mapper->SetInputConnection(geomFilter->GetOutputPort());
    mapper->SetLookupTable(lut);
    mapper->SetScalarRange(data.p_min, data.p_max);

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetOpacity(1.0); // 实体不透明
    actor->GetProperty()->SetEdgeVisibility(0); // 不显示网格线，看起来更像光滑的长方体

    renderer->AddActor(actor);
}

// 保存图片辅助函数
void saveImage(vtkRenderWindow* renderWindow, const std::string& filename) {
    vtkSmartPointer<vtkWindowToImageFilter> windowToImageFilter = vtkSmartPointer<vtkWindowToImageFilter>::New();
    windowToImageFilter->SetInput(renderWindow);
    windowToImageFilter->SetScale(2); // 2倍分辨率
    windowToImageFilter->SetInputBufferTypeToRGB();
    windowToImageFilter->Update();

    vtkSmartPointer<vtkPNGWriter> writer = vtkSmartPointer<vtkPNGWriter>::New();
    writer->SetFileName(filename.c_str());
    writer->SetInputConnection(windowToImageFilter->GetOutputPort());
    writer->Write();
    std::cout << "Saved image: " << filename << std::endl;
}

// 设置45度斜方俯视图
void setupCamera(vtkRenderer* renderer, double cx, double cy, double cz, double maxLen, bool isStacked) {
    vtkSmartPointer<vtkCamera> camera = renderer->GetActiveCamera();
    
    // 基础距离
    double dist = maxLen * 2.5;

    // 45度角位置 (X, Y, Z方向都偏移)
    // 对于堆叠图，因为Z被拉伸了，相机需要抬得更高
    double zFactor = isStacked ? 2.0 : 1.0; 
    
    // 设置位置为正对角线方向
    camera->SetPosition(cx + dist, cy + dist, cz + dist * zFactor);
    camera->SetFocalPoint(cx, cy, cz);
    camera->SetViewUp(0, 0, 1);
    
    renderer->ResetCamera();
    
    // 稍微调整一下缩放
    camera->Zoom(1.1);
}

int main(int argc, char* argv[]) {
    try {
        // 文件路径 (保持你的原始路径)
        std::string gridFile = "/root/csv_file/grid_info.csv";
        std::string fieldFile = "/root/csv_file/final_field.csv";
        std::string fractureFile = "/root/csv_file/fracture_geometry.csv";

        std::cout << "Reading data..." << std::endl;
        GridInfo gridInfo = readGridInfo(gridFile);
        FieldData fieldData = readFieldData(fieldFile);
        std::vector<Fracture> fractures = readFractures(fractureFile);

        // 创建渲染器和窗口
        vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
        renderer->SetBackground(0.0, 0.0, 0.0); // 黑色背景

        vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
        renderWindow->AddRenderer(renderer);
        renderWindow->SetSize(1200, 900);
        renderWindow->SetOffScreenRendering(1);

        // 创建颜色映射表和标量条 (两个场景通用)
        auto lut = createLookupTable(fieldData.p_min, fieldData.p_max);
        auto scalarBar = createColorBar(lut);
        renderer->AddActor2D(scalarBar);

        // 计算中心点 (用于相机聚焦)
        double cx = gridInfo.Lx / 2.0;
        double cy = gridInfo.Ly / 2.0;
        double cz = gridInfo.Lz / 2.0;
        double maxDim = std::max({gridInfo.Lx, gridInfo.Ly, gridInfo.Lz});

        // ==========================================
        // 渲染第一张图：堆叠切片图 (Stacked Slices)
        // ==========================================
        std::cout << "Rendering Image 1: Stacked Layers..." << std::endl;
        
        // 清除旧的Actor (如果有)
        renderer->RemoveAllViewProps();
        renderer->AddActor2D(scalarBar); // 重新加回scalarBar

        // 创建堆叠层Actor (zScaleFactor=10.0 意味着Z轴视觉上拉长10倍，方便看清层次)
        createStackedLayersScene(renderer, fieldData, lut, 10.0); 
        
        // 在堆叠层场景中绘制裂缝，使用相同的zScaleFactor
        drawFractures(renderer, fractures, 10.0);

        // 设置相机 (isStacked = true)
        // 注意：这里我们传入修改后的中心点Z，因为Z轴被拉伸了，中心点视觉上也变高了
        setupCamera(renderer, cx, cy, cz * 10.0, maxDim, true);
        
        renderWindow->Render();
        saveImage(renderWindow, "pressure_layers_stacked.png");

        // ==========================================
        // 渲染第二张图：实体长方体 (Solid Box)
        // ==========================================
        std::cout << "Rendering Image 2: Solid Box..." << std::endl;

        // 清除场景
        renderer->RemoveAllViewProps();
        renderer->AddActor2D(scalarBar);

        // 创建实体块Actor
        createSolidBlockScene(renderer, fieldData, lut);
        
        // 在实体块场景中绘制裂缝，使用zScaleFactor=1.0
        drawFractures(renderer, fractures, 1.0);

        // 设置相机 (isStacked = false)
        setupCamera(renderer, cx, cy, cz, maxDim, false);

        renderWindow->Render();
        saveImage(renderWindow, "pressure_solid_block.png");

        std::cout << "Done! Generated 'pressure_layers_stacked.png' and 'pressure_solid_block.png'." << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}