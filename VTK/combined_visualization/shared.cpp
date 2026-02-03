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
                // 区分天然裂缝和人工裂缝
                if (frac.id < 100) {
                    frac.type = "natural";
                } else {
                    frac.type = "artificial";
                }
                
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
                well.type = values[2];
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

// 线性插值函数
double linearInterpolation(double x0, double y0, double x1, double y1, double x) {
    if (std::abs(x1 - x0) < 1e-6) return y0;
    return y0 + (y1 - y0) * (x - x0) / (x1 - x0);
}

// 三线性插值函数
double trilinearInterpolation(double x, double y, double z,
                             const std::vector<double>& x_coords, const std::vector<double>& y_coords, const std::vector<double>& z_coords,
                             const std::vector<double>& pressure, int nx, int ny) {
    // 找到包含点 (x,y,z) 的网格单元
    int i = 0, j = 0, k = 0;
    while (i < x_coords.size() - 1 && x_coords[i+1] <= x) i++;
    while (j < y_coords.size() - 1 && y_coords[j+1] <= y) j++;
    while (k < z_coords.size() - 1 && z_coords[k+1] <= z) k++;

    // 确保索引在有效范围内
    i = std::min(i, (int)x_coords.size() - 2);
    j = std::min(j, (int)y_coords.size() - 2);
    k = std::min(k, (int)z_coords.size() - 2);

    // 获取网格单元的八个顶点
    double x0 = x_coords[i], x1 = x_coords[i+1];
    double y0 = y_coords[j], y1 = y_coords[j+1];
    double z0 = z_coords[k], z1 = z_coords[k+1];

    // 获取八个顶点的压力值
    double p000 = pressure[k * nx * ny + j * nx + i];
    double p001 = pressure[(k+1) * nx * ny + j * nx + i];
    double p010 = pressure[k * nx * ny + (j+1) * nx + i];
    double p011 = pressure[(k+1) * nx * ny + (j+1) * nx + i];
    double p100 = pressure[k * nx * ny + j * nx + (i+1)];
    double p101 = pressure[(k+1) * nx * ny + j * nx + (i+1)];
    double p110 = pressure[k * nx * ny + (j+1) * nx + (i+1)];
    double p111 = pressure[(k+1) * nx * ny + (j+1) * nx + (i+1)];

    // 沿 x 轴插值
    double p00 = linearInterpolation(x0, p000, x1, p100, x);
    double p01 = linearInterpolation(x0, p001, x1, p101, x);
    double p10 = linearInterpolation(x0, p010, x1, p110, x);
    double p11 = linearInterpolation(x0, p011, x1, p111, x);

    // 沿 y 轴插值
    double p0 = linearInterpolation(y0, p00, y1, p10, y);
    double p1 = linearInterpolation(y0, p01, y1, p11, y);

    // 沿 z 轴插值
    return linearInterpolation(z0, p0, z1, p1, z);
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

    // 提取原始格子中心点数据
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

    int nx_centers = unique_x.size();
    int ny_centers = unique_y.size();
    int nz_centers = unique_z.size();

    // 构建原始中心点压力数据
    std::vector<double> pressure_centers(nx_centers * ny_centers * nz_centers, 0.0);
    double minP = 1e30, maxP = -1e30;

    for (const auto& point : points) {
        double x = std::get<0>(point);
        double y = std::get<1>(point);
        double z = std::get<2>(point);
        double p = std::get<3>(point);

        int i = -1, j = -1, k = -1;
        for(int idx=0; idx<nx_centers; ++idx) if(std::abs(unique_x[idx]-x)<1e-6) i=idx;
        for(int idx=0; idx<ny_centers; ++idx) if(std::abs(unique_y[idx]-y)<1e-6) j=idx;
        for(int idx=0; idx<nz_centers; ++idx) if(std::abs(unique_z[idx]-z)<1e-6) k=idx;

        if (i>=0 && j>=0 && k>=0) {
            pressure_centers[k * nx_centers * ny_centers + j * nx_centers + i] = p;
            if (p < minP) minP = p;
            if (p > maxP) maxP = p;
        }
    }

    // 计算边界点坐标
    std::vector<double> x_boundary, y_boundary, z_boundary;
    
    // x 边界：0, 100, 200, ..., 1000
    for (int i = 0; i <= 10; ++i) {
        x_boundary.push_back(i * 100.0);
    }
    
    // y 边界：0, 100, 200, 300, 400, 500
    for (int i = 0; i <= 5; ++i) {
        y_boundary.push_back(i * 100.0);
    }
    
    // z 边界：0, 25, 50
    for (int i = 0; i <= 2; ++i) {
        z_boundary.push_back(i * 25.0);
    }

    int nx = x_boundary.size();
    int ny = y_boundary.size();
    int nz = z_boundary.size();

    // 使用三线性插值计算边界点的压力值
    std::vector<double> pressure_boundary(nx * ny * nz, 0.0);

    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                double x = x_boundary[i];
                double y = y_boundary[j];
                double z = z_boundary[k];
                
                // 使用三线性插值计算压力值
                double p = trilinearInterpolation(x, y, z, unique_x, unique_y, unique_z, pressure_centers, nx_centers, ny_centers);
                pressure_boundary[k * nx * ny + j * nx + i] = p;
                
                // 更新压力范围
                if (p < minP) minP = p;
                if (p > maxP) maxP = p;
            }
        }
    }

    // 设置返回的FieldData
    fieldData.nx = nx;
    fieldData.ny = ny;
    fieldData.nz = nz;
    fieldData.x_coords = x_boundary;
    fieldData.y_coords = y_boundary;
    fieldData.z_coords = z_boundary;
    fieldData.pressure = pressure_boundary;
    fieldData.p_min = minP;
    fieldData.p_max = maxP;

    std::cout << "Data Range: Pressure [" << minP << ", " << maxP << "]" << std::endl;
    std::cout << "Grid: " << nx << "x" << ny << "x" << nz << " (boundary points)" << std::endl;
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
    for (const auto& fracture : fractures) {
        if (fracture.points.size() >= 4) {
            vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
            vtkSmartPointer<vtkCellArray> polygons = vtkSmartPointer<vtkCellArray>::New();

            // 为裂缝创建多边形
            vtkIdType polygonPoints[4];
            bool validFracture = true;
            
            for (size_t i = 0; i < 4; i++) {
                double x = std::get<0>(fracture.points[i]);
                double y = std::get<1>(fracture.points[i]);
                double z = std::get<2>(fracture.points[i]) * zScaleFactor;
                
                // 确保裂缝在压力图范围内
                if (x < 0) x = 0;
                if (x > 1000) x = 1000;
                if (y < 0) y = 0;
                if (y > 500) y = 500;
                if (z < 0) z = 0;
                if (z > 50 * zScaleFactor) z = 50 * zScaleFactor;
                
                polygonPoints[i] = points->InsertNextPoint(x, y, z);
            }
            polygons->InsertNextCell(4, polygonPoints);

            vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
            polyData->SetPoints(points);
            polyData->SetPolys(polygons);

            vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
            mapper->SetInputData(polyData);

            vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
            actor->SetMapper(mapper);
            
            // 设置颜色：天然裂缝为橙色，人工裂缝为蓝色
            if (fracture.type == "natural") {
                actor->GetProperty()->SetColor(1.0, 0.5, 0.0); // 橙色
            } else {
                actor->GetProperty()->SetColor(0.0, 0.0, 1.0); // 蓝色
            }
            
            actor->GetProperty()->SetOpacity(0.8);
            actor->GetProperty()->SetEdgeVisibility(1);
            actor->GetProperty()->SetEdgeColor(0.2, 0.2, 0.2);
            actor->GetProperty()->SetLineWidth(0.5);

            renderer->AddActor(actor);
        }
    }
}

// 绘制井
void drawWells(vtkRenderer* renderer, const std::vector<WellInfo>& wells, const std::vector<Fracture>& fractures, double zScaleFactor) {
    // 找到第一个和第三个人工裂缝（蓝色）
    std::vector<Fracture> artificialFractures;
    for (const auto& fracture : fractures) {
        if (fracture.type == "artificial") {
            artificialFractures.push_back(fracture);
        }
    }

    if (artificialFractures.size() < 3) {
        std::cerr << "Warning: Not enough artificial fractures found (need at least 3)" << std::endl;
        return;
    }

    // 获取第一个和第三个人工裂缝
    Fracture& frac1 = artificialFractures[0];
    Fracture& frac3 = artificialFractures[2];

    // 输出人工裂缝的位置信息
    std::cout << "Artificial Fracture 1 (ID: " << frac1.id << ") Points:" << std::endl;
    for (size_t i = 0; i < frac1.points.size(); i++) {
        double x = std::get<0>(frac1.points[i]);
        double y = std::get<1>(frac1.points[i]);
        double z = std::get<2>(frac1.points[i]);
        std::cout << "  Point " << i << ": (" << x << ", " << y << ", " << z << ")" << std::endl;
    }
    
    std::cout << "Artificial Fracture 3 (ID: " << frac3.id << ") Points:" << std::endl;
    for (size_t i = 0; i < frac3.points.size(); i++) {
        double x = std::get<0>(frac3.points[i]);
        double y = std::get<1>(frac3.points[i]);
        double z = std::get<2>(frac3.points[i]);
        std::cout << "  Point " << i << ": (" << x << ", " << y << ", " << z << ")" << std::endl;
    }

    // 从人工裂缝中提取x值
    // 第一个人工裂缝的x值（起点）
    double frac1_x = std::get<0>(frac1.points[0]);
    // 第三个人工裂缝的x值（终点）
    double frac3_x = std::get<0>(frac3.points[0]);
    
    // 输出人工裂缝的x值
    std::cout << "First artificial fracture x: " << frac1_x << std::endl;
    std::cout << "Third artificial fracture x: " << frac3_x << std::endl;

    // 绘制三口井，修改其他两口井的y坐标
    for (size_t i = 0; i < wells.size(); i++) {
        const auto& well = wells[i];
        // 井的Z位置
        double z = well.z * zScaleFactor;

        // 为了在实体块可视化中可见，将井绘制在实体块顶部上方
        if (zScaleFactor == 1.0) { // cube_visualizer使用1.0的缩放因子
            z += 5.0; // 将井提高到实体块顶部上方
        }

        // 修改井的y坐标
        double well_y = well.y;
        if (i == 1) {
            well_y = 200; // 第二口井的y改为100
        } else if (i == 2) {
            well_y = 300; // 第三口井的y改为200
        }

        // 井的起点和终点
        // 起点：第一条人工裂缝的x, 当前井的y, 当前井的z
        double startPoint[3] = {frac1_x, well_y, z};
        // 终点：第三条人工裂缝的x, 当前井的y, 当前井的z
        double endPoint[3] = {frac3_x, well_y, z};

        // 计算井的长度和方向
        double direction[3] = {endPoint[0] - startPoint[0], endPoint[1] - startPoint[1], endPoint[2] - startPoint[2]};
        double length = std::sqrt(direction[0]*direction[0] + direction[1]*direction[1] + direction[2]*direction[2]);
        
        // 计算中心点
        double center[3] = {(startPoint[0] + endPoint[0])/2, (startPoint[1] + endPoint[1])/2, (startPoint[2] + endPoint[2])/2};
        
        // 输出井的坐标信息
        std::cout << "Well ID: " << well.well_id << std::endl;
        std::cout << "Start Point: (" << startPoint[0] << ", " << startPoint[1] << ", " << startPoint[2] << ")" << std::endl;
        std::cout << "End Point: (" << endPoint[0] << ", " << endPoint[1] << ", " << endPoint[2] << ")" << std::endl;
        std::cout << "Center: (" << center[0] << ", " << center[1] << ", " << center[2] << ")" << std::endl;
        std::cout << "Length: " << length << std::endl;

        // 创建圆柱体
        vtkSmartPointer<vtkCylinderSource> cylinderSource = vtkSmartPointer<vtkCylinderSource>::New();
        cylinderSource->SetRadius(5.0); // 半径为5m
        cylinderSource->SetHeight(length);
        cylinderSource->SetResolution(20);

        // 归一化方向向量 (井的实际方向)
        double normalizedDir[3] = {direction[0], direction[1], direction[2]};
        vtkMath::Normalize(normalizedDir);

        // vtkCylinderSource 的默认方向是 Y 轴 (0, 1, 0)
        double defaultDir[3] = {0.0, 1.0, 0.0};

        // 计算旋转轴 (默认方向 cross 实际方向)
        double rotationAxis[3];
        vtkMath::Cross(defaultDir, normalizedDir, rotationAxis);

        // 计算旋转角度 (弧度 -> 角度)
        // dot product = cos(theta)
        double angleObj = vtkMath::DegreesFromRadians(acos(vtkMath::Dot(defaultDir, normalizedDir)));

        // 创建变换
        vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
        
        // 1. 平移到中心
        transform->Translate(center[0], center[1], center[2]);

        // 2. 旋转
        // 如果角度不为0，且旋转轴有效，则进行旋转
        if (vtkMath::Norm(rotationAxis) > 0.0001) {
             transform->RotateWXYZ(angleObj, rotationAxis);
        } else {
             // 特殊情况：如果方向完全相反 (0, -1, 0)，Cross结果为0，需要手动翻转
             if (vtkMath::Dot(defaultDir, normalizedDir) < 0) {
                 transform->RotateX(180.0);
             }
        }

        // 应用变换
        vtkSmartPointer<vtkTransformFilter> transformFilter = vtkSmartPointer<vtkTransformFilter>::New();
        transformFilter->SetInputConnection(cylinderSource->GetOutputPort());
        transformFilter->SetTransform(transform);

        // 创建映射器和 actor
        vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(transformFilter->GetOutputPort());

        vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(1.0, 0.0, 0.0); // 红色
        actor->GetProperty()->SetOpacity(1.0);

        renderer->AddActor(actor);
    }
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

// 创建只显示裂缝和井的场景
void createFracturesWellsScene(vtkRenderer* renderer, const std::vector<Fracture>& fractures, const std::vector<WellInfo>& wells, double zScaleFactor) {
    // 绘制裂缝
    drawFractures(renderer, fractures, zScaleFactor);
    
    // 绘制井
    drawWells(renderer, wells, fractures, zScaleFactor);
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
