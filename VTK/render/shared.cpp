#include "shared.h"

// 辅助函数实现
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

// 读取裂缝数据函数
std::vector<Fracture> readFractures(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open fracture_geometry.csv: " + filename);
    }

    std::vector<Fracture> fractures;
    std::string line;
    int line_count = 0;

    if (std::getline(file, line)) {
        line_count++;
    }

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

// 读取井数据函数
std::vector<WellInfo> readWells(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open well_info.csv: " + filename);
    }

    std::vector<WellInfo> wells;
    std::string line;
    int line_count = 0;

    if (std::getline(file, line)) {
        line_count++;
    }

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

        if (values.size() >= 8) {
            try {
                WellInfo well;
                well.well_id = std::stoi(values[0]);
                well.node_idx = std::stoi(values[1]);
                // NOTE: well_info.csv 的 type 字段可能写错（例如写成 "Fracture"）。
                // 语义以“这是 well_info.csv 读取出来的数据”为准，统一当作 Well。
                well.type = "Well";
                well.x = std::stod(values[3]);
                well.y = std::stod(values[4]);
                well.z = std::stod(values[5]);
                well.WI = std::stod(values[6]);
                well.P_bhp = std::stod(values[7]);

                wells.push_back(well);
            } catch (const std::exception& e) {
                std::cerr << "Warning: Error parsing well at line " << line_count << ": " << e.what() << std::endl;
            }
        }
    }

    std::cout << "Read " << wells.size() << " wells from " << filename << std::endl;
    return wells;
}

// 读取网格信息
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

// 读取压力场数据
FieldData readFieldData(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) throw std::runtime_error("Could not open final_field.csv");

    FieldData fieldData;
    std::string line;
    std::vector<std::tuple<double, double, double, double>> points;

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

    double minP = 1e30, maxP = -1e30;

    for (const auto& point : points) {
        double x = std::get<0>(point);
        double y = std::get<1>(point);
        double z = std::get<2>(point);
        double p = std::get<3>(point);

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

// 创建颜色映射表
vtkSmartPointer<vtkLookupTable> createLookupTable(double minP, double maxP) {
    vtkSmartPointer<vtkLookupTable> lut = vtkSmartPointer<vtkLookupTable>::New();
    lut->SetTableRange(minP, maxP);
    lut->SetHueRange(0.667, 0.0);
    lut->SetSaturationRange(1.0, 1.0);
    lut->SetValueRange(1.0, 1.0);
    lut->SetNumberOfTableValues(256);
    lut->Build();
    return lut;
}

// 创建标量条
vtkSmartPointer<vtkScalarBarActor> createColorBar(vtkLookupTable* lut) {
    vtkSmartPointer<vtkScalarBarActor> scalarBar = vtkSmartPointer<vtkScalarBarActor>::New();
    scalarBar->SetLookupTable(lut);
    scalarBar->SetTitle("Pressure (MPa)");
    scalarBar->SetNumberOfLabels(5);
    scalarBar->SetWidth(0.1);
    scalarBar->SetHeight(0.5);
    scalarBar->GetLabelTextProperty()->SetColor(1, 1, 1);
    scalarBar->GetTitleTextProperty()->SetColor(1, 1, 1);
    return scalarBar;
}

// 创建堆叠层场景
void createStackedLayersScene(vtkRenderer* renderer, const FieldData& data, vtkLookupTable* lut, double zScaleFactor) {
    int num_layers_to_show = 5;
    int step = std::max(1, data.nz / (num_layers_to_show - 1));
    
    if (data.nz <= 5) step = 1;

    for (int k = 0; k < data.nz; k += step) {
        vtkSmartPointer<vtkStructuredGrid> layerGrid = vtkSmartPointer<vtkStructuredGrid>::New();
        layerGrid->SetDimensions(data.nx, data.ny, 1);

        vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
        vtkSmartPointer<vtkDoubleArray> pressureArr = vtkSmartPointer<vtkDoubleArray>::New();
        pressureArr->SetName("Pressure");

        for (int j = 0; j < data.ny; j++) {
            for (int i = 0; i < data.nx; i++) {
                double originalZ = data.z_coords[k];
                double visualZ = originalZ * zScaleFactor;
                
                points->InsertNextPoint(data.x_coords[i], data.y_coords[j], visualZ);
                
                int idx = k * data.nx * data.ny + j * data.nx + i;
                pressureArr->InsertNextValue(data.pressure[idx]);
            }
        }

        layerGrid->SetPoints(points);
        layerGrid->GetPointData()->SetScalars(pressureArr);

        vtkSmartPointer<vtkGeometryFilter> geomFilter = vtkSmartPointer<vtkGeometryFilter>::New();
        geomFilter->SetInputData(layerGrid);

        vtkSmartPointer<vtkDataSetMapper> mapper = vtkSmartPointer<vtkDataSetMapper>::New();
        mapper->SetInputConnection(geomFilter->GetOutputPort());
        mapper->SetLookupTable(lut);
        mapper->SetScalarRange(data.p_min, data.p_max);

        vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        
        actor->GetProperty()->SetOpacity(0.9);
        actor->GetProperty()->SetEdgeVisibility(1);
        actor->GetProperty()->SetEdgeColor(0.2, 0.2, 0.2);
        actor->GetProperty()->SetLineWidth(0.5);

        renderer->AddActor(actor);
    }
}

// 创建实体块场景
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

    vtkSmartPointer<vtkGeometryFilter> geomFilter = vtkSmartPointer<vtkGeometryFilter>::New();
    geomFilter->SetInputData(grid);

    vtkSmartPointer<vtkDataSetMapper> mapper = vtkSmartPointer<vtkDataSetMapper>::New();
    mapper->SetInputConnection(geomFilter->GetOutputPort());
    mapper->SetLookupTable(lut);
    mapper->SetScalarRange(data.p_min, data.p_max);

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetOpacity(1.0);
    actor->GetProperty()->SetEdgeVisibility(0);

    renderer->AddActor(actor);
}

// 绘制裂缝
void drawFractures(vtkRenderer* renderer, const std::vector<Fracture>& fractures, double zScaleFactor) {
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();

    for (const auto& fracture : fractures) {
        if (fracture.points.size() >= 2) {
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

                vtkIdType lineIds[2] = {p1, p2};
                lines->InsertNextCell(2, lineIds);
            }
        }
    }

    vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetLines(lines);

    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(polyData);

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0.0, 0.0, 0.0);
    actor->GetProperty()->SetLineWidth(2.0);
    actor->GetProperty()->SetOpacity(1.0);

    renderer->AddActor(actor);
}

// 绘制井
void drawWells(vtkRenderer* renderer, const std::vector<WellInfo>& wells, double zScaleFactor) {
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();

    for (const auto& well : wells) {
        // 井的位置，使用点表示
        double x = well.x;
        double y = well.y;
        double z = well.z * zScaleFactor;

        // 在井的位置绘制一个小十字叉
        const double crossSize = 15.0; // 增大十字叉的大小，使其更明显
        
        // 为了在实体块可视化中可见，将井绘制在实体块顶部上方
        if (zScaleFactor == 1.0) { // cube_visualizer使用1.0的缩放因子
            z += 5.0; // 将井提高到实体块顶部上方
        }
        
        // 水平线段
        vtkIdType p1 = points->InsertNextPoint(x - crossSize/2, y, z);
        vtkIdType p2 = points->InsertNextPoint(x + crossSize/2, y, z);
        vtkIdType lineIds1[2] = {p1, p2};
        lines->InsertNextCell(2, lineIds1);
        
        // 垂直线段
        vtkIdType p3 = points->InsertNextPoint(x, y - crossSize/2, z);
        vtkIdType p4 = points->InsertNextPoint(x, y + crossSize/2, z);
        vtkIdType lineIds2[2] = {p3, p4};
        lines->InsertNextCell(2, lineIds2);
    }

    vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetLines(lines);

    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(polyData);

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(1.0, 0.0, 0.0); // 红色
    actor->GetProperty()->SetLineWidth(3.0); // 比裂缝更粗的线
    actor->GetProperty()->SetOpacity(1.0);

    renderer->AddActor(actor);
}

// 保存图片
void saveImage(vtkRenderWindow* renderWindow, const std::string& filename) {
    vtkSmartPointer<vtkWindowToImageFilter> windowToImageFilter = vtkSmartPointer<vtkWindowToImageFilter>::New();
    windowToImageFilter->SetInput(renderWindow);
    windowToImageFilter->SetScale(2);
    windowToImageFilter->SetInputBufferTypeToRGB();
    windowToImageFilter->Update();

    vtkSmartPointer<vtkPNGWriter> writer = vtkSmartPointer<vtkPNGWriter>::New();
    writer->SetFileName(filename.c_str());
    writer->SetInputConnection(windowToImageFilter->GetOutputPort());
    writer->Write();
    std::cout << "Saved image: " << filename << std::endl;
}

// 设置相机
void setupCamera(vtkRenderer* renderer, double cx, double cy, double cz, double maxLen, bool isStacked) {
    vtkSmartPointer<vtkCamera> camera = renderer->GetActiveCamera();
    
    double dist = maxLen * 2.5;
    double zFactor = isStacked ? 2.0 : 1.0;
    
    camera->SetPosition(cx + dist, cy + dist, cz + dist * zFactor);
    camera->SetFocalPoint(cx, cy, cz);
    camera->SetViewUp(0, 0, 1);
    
    renderer->ResetCamera();
    camera->Zoom(1.1);
}

// 读取配置文件
bool readConfig(const std::string& filename, std::string& gridFile, std::string& fieldFile, std::string& fractureFile, std::string& wellFile) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open config file: " << filename << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        // 查找各个字段
        if (line.find("grid_file") != std::string::npos) {
            size_t start = line.find('"', line.find("grid_file") + 12);
            if (start != std::string::npos) {
                start += 1;
                size_t end = line.find('"', start);
                if (end != std::string::npos) {
                    gridFile = line.substr(start, end - start);
                }
            }
        } else if (line.find("field_file") != std::string::npos) {
            size_t start = line.find('"', line.find("field_file") + 13);
            if (start != std::string::npos) {
                start += 1;
                size_t end = line.find('"', start);
                if (end != std::string::npos) {
                    fieldFile = line.substr(start, end - start);
                }
            }
        } else if (line.find("fracture_file") != std::string::npos) {
            size_t start = line.find('"', line.find("fracture_file") + 16);
            if (start != std::string::npos) {
                start += 1;
                size_t end = line.find('"', start);
                if (end != std::string::npos) {
                    fractureFile = line.substr(start, end - start);
                }
            }
        } else if (line.find("well_file") != std::string::npos) {
            size_t start = line.find('"', line.find("well_file") + 11);
            if (start != std::string::npos) {
                start += 1;
                size_t end = line.find('"', start);
                if (end != std::string::npos) {
                    wellFile = line.substr(start, end - start);
                }
            }
        }
    }

    file.close();
    
    return !gridFile.empty() && !fieldFile.empty() && !fractureFile.empty() && !wellFile.empty();
}

