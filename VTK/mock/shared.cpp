#include "shared.h"

// 让 shared.cpp 直接知道 MockData 结构体定义
#include "mock_data.h"

// 辅助函数实现
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

// ======================
// CSV: 读取裂缝数据函数
// ======================
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

// ======================
// CSV: 读取井数据函数
// ======================
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

// ======================
// CSV: 读取网格信息
// ======================
GridInfo readGridInfo(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) throw std::runtime_error("Could not open grid_info.csv: " + filename);

    GridInfo grid{};
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

        grid.npx = 0;
        grid.npy = 0;
    }
    return grid;
}

// 线性插值函数
static double linearInterpolation(double x0, double y0, double x1, double y1, double x) {
    if (std::abs(x1 - x0) < 1e-6) return y0;
    return y0 + (y1 - y0) * (x - x0) / (x1 - x0);
}

// 三线性插值函数（在中心点网格上插值）
static double trilinearInterpolation(double x, double y, double z,
                                     const std::vector<double>& x_coords, const std::vector<double>& y_coords, const std::vector<double>& z_coords,
                                     const std::vector<double>& pressure, int nx, int ny) {
    int i = 0, j = 0, k = 0;
    while (i < (int)x_coords.size() - 1 && x_coords[i + 1] <= x) i++;
    while (j < (int)y_coords.size() - 1 && y_coords[j + 1] <= y) j++;
    while (k < (int)z_coords.size() - 1 && z_coords[k + 1] <= z) k++;

    i = std::min(i, (int)x_coords.size() - 2);
    j = std::min(j, (int)y_coords.size() - 2);
    k = std::min(k, (int)z_coords.size() - 2);

    double x0 = x_coords[i], x1 = x_coords[i + 1];
    double y0 = y_coords[j], y1 = y_coords[j + 1];
    double z0 = z_coords[k], z1 = z_coords[k + 1];

    double p000 = pressure[k * nx * ny + j * nx + i];
    double p001 = pressure[(k + 1) * nx * ny + j * nx + i];
    double p010 = pressure[k * nx * ny + (j + 1) * nx + i];
    double p011 = pressure[(k + 1) * nx * ny + (j + 1) * nx + i];
    double p100 = pressure[k * nx * ny + j * nx + (i + 1)];
    double p101 = pressure[(k + 1) * nx * ny + j * nx + (i + 1)];
    double p110 = pressure[k * nx * ny + (j + 1) * nx + (i + 1)];
    double p111 = pressure[(k + 1) * nx * ny + (j + 1) * nx + (i + 1)];

    double p00 = linearInterpolation(x0, p000, x1, p100, x);
    double p01 = linearInterpolation(x0, p001, x1, p101, x);
    double p10 = linearInterpolation(x0, p010, x1, p110, x);
    double p11 = linearInterpolation(x0, p011, x1, p111, x);

    double p0 = linearInterpolation(y0, p00, y1, p10, y);
    double p1 = linearInterpolation(y0, p01, y1, p11, y);

    return linearInterpolation(z0, p0, z1, p1, z);
}

// ======================
// CSV: 读取压力场数据（原样保留）
// ======================
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

        bool found_x = false; for (double v : unique_x) if (std::abs(v - x) < 1e-6) found_x = true;
        if (!found_x) unique_x.push_back(x);

        bool found_y = false; for (double v : unique_y) if (std::abs(v - y) < 1e-6) found_y = true;
        if (!found_y) unique_y.push_back(y);

        bool found_z = false; for (double v : unique_z) if (std::abs(v - z) < 1e-6) found_z = true;
        if (!found_z) unique_z.push_back(z);
    }

    std::sort(unique_x.begin(), unique_x.end());
    std::sort(unique_y.begin(), unique_y.end());
    std::sort(unique_z.begin(), unique_z.end());

    int nx_centers = (int)unique_x.size();
    int ny_centers = (int)unique_y.size();
    int nz_centers = (int)unique_z.size();

    std::vector<double> pressure_centers(nx_centers * ny_centers * nz_centers, 0.0);
    double minP = 1e30, maxP = -1e30;

    for (const auto& point : points) {
        double x = std::get<0>(point);
        double y = std::get<1>(point);
        double z = std::get<2>(point);
        double p = std::get<3>(point);

        int i = -1, j = -1, k = -1;
        for (int idx = 0; idx < nx_centers; ++idx) if (std::abs(unique_x[idx] - x) < 1e-6) i = idx;
        for (int idx = 0; idx < ny_centers; ++idx) if (std::abs(unique_y[idx] - y) < 1e-6) j = idx;
        for (int idx = 0; idx < nz_centers; ++idx) if (std::abs(unique_z[idx] - z) < 1e-6) k = idx;

        if (i >= 0 && j >= 0 && k >= 0) {
            pressure_centers[k * nx_centers * ny_centers + j * nx_centers + i] = p;
            if (p < minP) minP = p;
            if (p > maxP) maxP = p;
        }
    }

    // 这块是你原来写死的边界点（保留不动）
    std::vector<double> x_boundary, y_boundary, z_boundary;
    for (int i = 0; i <= 10; ++i) x_boundary.push_back(i * 100.0);
    for (int i = 0; i <= 5; ++i)  y_boundary.push_back(i * 100.0);
    for (int i = 0; i <= 2; ++i)  z_boundary.push_back(i * 25.0);

    int nx = (int)x_boundary.size();
    int ny = (int)y_boundary.size();
    int nz = (int)z_boundary.size();

    std::vector<double> pressure_boundary(nx * ny * nz, 0.0);

    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                double x = x_boundary[i];
                double y = y_boundary[j];
                double z = z_boundary[k];

                double p = trilinearInterpolation(x, y, z, unique_x, unique_y, unique_z,
                                                  pressure_centers, nx_centers, ny_centers);
                pressure_boundary[k * nx * ny + j * nx + i] = p;

                if (p < minP) minP = p;
                if (p > maxP) maxP = p;
            }
        }
    }

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

// 创建实体块场景（✅修复：半透明 + 画边线，否则内部裂缝/井会被挡住）
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

    // ✅关键：不透明会遮住内部裂缝/井
    actor->GetProperty()->SetOpacity(0.25);
    actor->GetProperty()->SetEdgeVisibility(1);
    actor->GetProperty()->SetEdgeColor(0.2, 0.2, 0.2);
    actor->GetProperty()->SetLineWidth(0.5);

    renderer->AddActor(actor);
}

// 绘制裂缝
void drawFractures(vtkRenderer* renderer, const std::vector<Fracture>& fractures, double zScaleFactor) {
    for (const auto& fracture : fractures) {
        if (fracture.points.size() >= 4) {
            vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
            vtkSmartPointer<vtkCellArray> polygons = vtkSmartPointer<vtkCellArray>::New();

            vtkIdType polygonPoints[4];

            for (size_t i = 0; i < 4; i++) {
                double x = std::get<0>(fracture.points[i]);
                double y = std::get<1>(fracture.points[i]);
                double z = std::get<2>(fracture.points[i]) * zScaleFactor;

                // 你原先的 clamp（保留）
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

            if (fracture.type == "natural") {
                actor->GetProperty()->SetColor(1.0, 0.5, 0.0);
            } else {
                actor->GetProperty()->SetColor(0.0, 0.0, 1.0);
            }

            actor->GetProperty()->SetOpacity(0.85);
            actor->GetProperty()->SetEdgeVisibility(1);
            actor->GetProperty()->SetEdgeColor(0.2, 0.2, 0.2);
            actor->GetProperty()->SetLineWidth(0.8);

            renderer->AddActor(actor);
        }
    }
}

// ------------------------------
// ✅内部小工具：画一个圆柱（给 drawWells fallback 用）
// ------------------------------
static void AddCylinderBetween(vtkRenderer* renderer,
                               const double startPoint[3],
                               const double endPoint[3],
                               double radius,
                               double r, double g, double b,
                               double opacity) {
    double direction[3] = { endPoint[0] - startPoint[0],
                            endPoint[1] - startPoint[1],
                            endPoint[2] - startPoint[2] };

    double length = std::sqrt(direction[0]*direction[0] +
                              direction[1]*direction[1] +
                              direction[2]*direction[2]);

    if (length < 1e-8) return;

    double center[3] = { (startPoint[0] + endPoint[0]) / 2.0,
                         (startPoint[1] + endPoint[1]) / 2.0,
                         (startPoint[2] + endPoint[2]) / 2.0 };

    vtkSmartPointer<vtkCylinderSource> cylinderSource = vtkSmartPointer<vtkCylinderSource>::New();
    cylinderSource->SetRadius(radius);
    cylinderSource->SetHeight(length);
    cylinderSource->SetResolution(24);

    double normalizedDir[3] = { direction[0], direction[1], direction[2] };
    vtkMath::Normalize(normalizedDir);

    // vtkCylinderSource 默认轴向是 Y
    double defaultDir[3] = { 0.0, 1.0, 0.0 };

    double rotationAxis[3];
    vtkMath::Cross(defaultDir, normalizedDir, rotationAxis);

    double dot = vtkMath::Dot(defaultDir, normalizedDir);
    dot = std::max(-1.0, std::min(1.0, dot));
    double angleObj = vtkMath::DegreesFromRadians(std::acos(dot));

    vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
    transform->Translate(center[0], center[1], center[2]);

    if (vtkMath::Norm(rotationAxis) > 0.0001) {
        transform->RotateWXYZ(angleObj, rotationAxis);
    } else {
        if (dot < 0) transform->RotateX(180.0);
    }

    vtkSmartPointer<vtkTransformFilter> transformFilter = vtkSmartPointer<vtkTransformFilter>::New();
    transformFilter->SetInputConnection(cylinderSource->GetOutputPort());
    transformFilter->SetTransform(transform);

    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(transformFilter->GetOutputPort());

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(r, g, b);
    actor->GetProperty()->SetOpacity(opacity);

    renderer->AddActor(actor);
}

// 绘制井（✅修复：人工裂缝不足时也能画井）
void drawWells(vtkRenderer* renderer,
               const std::vector<WellInfo>& wells,
               const std::vector<Fracture>& fractures,
               double zScaleFactor) {

    // 收集人工裂缝
    std::vector<Fracture> artificialFractures;
    artificialFractures.reserve(fractures.size());
    for (const auto& fracture : fractures) {
        if (fracture.type == "artificial") {
            artificialFractures.push_back(fracture);
        }
    }

    // ✅兜底：人工裂缝不足时，直接按 well 自己的位置画竖井
    if (artificialFractures.size() < 3) {
        std::cerr << "Warning: Not enough artificial fractures (need >=3). "
                  << "Fallback: draw vertical wells by well.position." << std::endl;

        // 竖井高度：用你的 clamp 范围（0..50）保持一致
        const double z0 = 0.0;
        const double z1 = 50.0 * zScaleFactor;

        for (const auto& well : wells) {
            double x = well.x;
            double y = well.y;

            // well.z 也参与（避免井完全贴地）
            double wz = well.z * zScaleFactor;
            double startPoint[3] = { x, y, std::max(z0, wz - 10.0) };
            double endPoint[3]   = { x, y, std::min(z1, wz + 10.0) };

            // 如果你更想画“贯穿全层”，就把上面两行换成：
            // double startPoint[3] = { x, y, z0 };
            // double endPoint[3]   = { x, y, z1 };

            AddCylinderBetween(renderer, startPoint, endPoint,
                               /*radius*/3.0,
                               /*color*/1.0, 0.0, 0.0,
                               /*opacity*/1.0);
        }
        return;
    }

    // --------------------------------
    // 下面保持你原来的“横井”逻辑不变
    // --------------------------------
    const Fracture& frac1 = artificialFractures[0];
    const Fracture& frac3 = artificialFractures[2];

    double frac1_x = std::get<0>(frac1.points[0]);
    double frac3_x = std::get<0>(frac3.points[0]);

    for (size_t i = 0; i < wells.size(); i++) {
        const auto& well = wells[i];
        double z = well.z * zScaleFactor;

        if (zScaleFactor == 1.0) {
            z += 5.0;
        }

        double well_y = well.y;
        if (i == 1) {
            well_y = 200;
        } else if (i == 2) {
            well_y = 300;
        }

        double startPoint[3] = { frac1_x, well_y, z };
        double endPoint[3]   = { frac3_x, well_y, z };

        AddCylinderBetween(renderer, startPoint, endPoint,
                           /*radius*/5.0,
                           /*color*/1.0, 0.0, 0.0,
                           /*opacity*/1.0);
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
    drawFractures(renderer, fractures, zScaleFactor);
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

        if (line.find("grid_file") != std::string::npos) {
            size_t start = line.find('"', line.find("grid_file") + 12);
            if (start != std::string::npos) {
                start += 1;
                size_t end = line.find('"', start);
                if (end != std::string::npos) gridFile = line.substr(start, end - start);
            }
        } else if (line.find("field_file") != std::string::npos) {
            size_t start = line.find('"', line.find("field_file") + 13);
            if (start != std::string::npos) {
                start += 1;
                size_t end = line.find('"', start);
                if (end != std::string::npos) fieldFile = line.substr(start, end - start);
            }
        } else if (line.find("fracture_file") != std::string::npos) {
            size_t start = line.find('"', line.find("fracture_file") + 16);
            if (start != std::string::npos) {
                start += 1;
                size_t end = line.find('"', start);
                if (end != std::string::npos) fractureFile = line.substr(start, end - start);
            }
        } else if (line.find("well_file") != std::string::npos) {
            size_t start = line.find('"', line.find("well_file") + 11);
            if (start != std::string::npos) {
                start += 1;
                size_t end = line.find('"', start);
                if (end != std::string::npos) wellFile = line.substr(start, end - start);
            }
        }
    }

    file.close();
    return !gridFile.empty() && !fieldFile.empty() && !fractureFile.empty() && !wellFile.empty();
}

// =====================================================================
// NEW: MockData -> shared 输入转换（你要的“直接接 mock 数据”的入口）
// =====================================================================

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

GridInfo GridInfoFromMock(const MockData::GridData& g) {
    GridInfo out{};
    out.nx = g.Nx;
    out.ny = g.Ny;
    out.nz = g.Nz;
    out.Lx = g.Lx;
    out.Ly = g.Ly;
    out.Lz = g.Lz;
    out.dx = g.dx;
    out.dy = g.dy;
    out.dz = g.dz;
    out.npx = 0;
    out.npy = 0;
    return out;
}

// 从 mock 的 cell center 压力，构建 shared 的 FieldData（边界点压力）
FieldData FieldDataFromMockCells(const MockData::GridData& g, const std::vector<MockData::CellData>& cells) {
    FieldData out{};

    const int Nx = g.Nx;
    const int Ny = g.Ny;
    const int Nz = g.Nz;

    if (Nx <= 0 || Ny <= 0 || Nz <= 0) {
        throw std::runtime_error("FieldDataFromMockCells: invalid GridData dims");
    }
    if (g.dx <= 0 || g.dy <= 0 || g.dz <= 0) {
        throw std::runtime_error("FieldDataFromMockCells: invalid GridData spacing (dx/dy/dz must be > 0)");
    }

    // center coords（用于插值的“原始网格”）
    std::vector<double> cx(Nx), cy(Ny), cz(Nz);
    for (int i = 0; i < Nx; ++i) cx[i] = (i + 0.5) * g.dx;
    for (int j = 0; j < Ny; ++j) cy[j] = (j + 0.5) * g.dy;
    for (int k = 0; k < Nz; ++k) cz[k] = (k + 0.5) * g.dz;

    const int nCenters = Nx * Ny * Nz;
    std::vector<double> sumP(nCenters, 0.0);
    std::vector<int>    cntP(nCenters, 0);

    auto idxCenter = [Nx, Ny](int i, int j, int k) {
        return k * Nx * Ny + j * Nx + i;
    };

    // 落格子（不依赖 cells 顺序）
    for (const auto& c : cells) {
        const double fx = c.center.x / g.dx - 0.5;
        const double fy = c.center.y / g.dy - 0.5;
        const double fz = c.center.z / g.dz - 0.5;

        int i = (int)std::llround(fx);
        int j = (int)std::llround(fy);
        int k = (int)std::llround(fz);

        i = clampi(i, 0, Nx - 1);
        j = clampi(j, 0, Ny - 1);
        k = clampi(k, 0, Nz - 1);

        int idx = idxCenter(i, j, k);
        sumP[idx] += c.pressure;
        cntP[idx] += 1;
    }

    // 得到完整 pressure_centers
    std::vector<double> pressure_centers(nCenters, 0.0);
    double minP = 1e30, maxP = -1e30;
    bool hasAny = false;

    for (int k = 0; k < Nz; ++k) {
        for (int j = 0; j < Ny; ++j) {
            for (int i = 0; i < Nx; ++i) {
                int idx = idxCenter(i, j, k);
                double p = 0.0;

                if (cntP[idx] > 0) {
                    p = sumP[idx] / (double)cntP[idx];
                    hasAny = true;
                    if (p < minP) minP = p;
                    if (p > maxP) maxP = p;
                }
                pressure_centers[idx] = p;
            }
        }
    }

    if (!hasAny) {
        minP = 0.0;
        maxP = 1.0;
    }

    for (int idx = 0; idx < nCenters; ++idx) {
        if (cntP[idx] == 0) pressure_centers[idx] = minP;
    }

    // 边界坐标：0..Lx step dx（大小 Nx+1）
    std::vector<double> xb(Nx + 1), yb(Ny + 1), zb(Nz + 1);
    for (int i = 0; i <= Nx; ++i) xb[i] = i * g.dx;
    for (int j = 0; j <= Ny; ++j) yb[j] = j * g.dy;
    for (int k = 0; k <= Nz; ++k) zb[k] = k * g.dz;

    const int NX = (int)xb.size();
    const int NY = (int)yb.size();
    const int NZ = (int)zb.size();

    std::vector<double> pressure_boundary(NX * NY * NZ, 0.0);

    for (int kk = 0; kk < NZ; ++kk) {
        for (int jj = 0; jj < NY; ++jj) {
            for (int ii = 0; ii < NX; ++ii) {
                double x = xb[ii];
                double y = yb[jj];
                double z = zb[kk];

                double p = trilinearInterpolation(x, y, z, cx, cy, cz, pressure_centers, Nx, Ny);
                pressure_boundary[kk * NX * NY + jj * NX + ii] = p;

                if (p < minP) minP = p;
                if (p > maxP) maxP = p;
            }
        }
    }

    out.nx = NX;
    out.ny = NY;
    out.nz = NZ;
    out.x_coords = std::move(xb);
    out.y_coords = std::move(yb);
    out.z_coords = std::move(zb);
    out.pressure = std::move(pressure_boundary);
    out.p_min = minP;
    out.p_max = maxP;

    std::cout << "[Mock->FieldData] boundary grid: "
              << out.nx << "x" << out.ny << "x" << out.nz
              << "  pressure range [" << out.p_min << ", " << out.p_max << "]"
              << std::endl;

    return out;
}

std::vector<Fracture> FracturesFromMock(const std::vector<MockData::FractureData>& fracs) {
    std::vector<Fracture> out;
    out.reserve(fracs.size());

    for (const auto& f : fracs) {
        Fracture frac{};
        frac.id = f.id;
        frac.type = (f.id < 100) ? "natural" : "artificial";
        for (const auto& p : f.vertices) {
            frac.points.emplace_back(p.x, p.y, p.z);
        }
        out.push_back(std::move(frac));
    }
    return out;
}

std::vector<WellInfo> WellsFromMock(const std::vector<MockData::WellData>& wells) {
    std::vector<WellInfo> out;
    out.reserve(wells.size());

    for (const auto& w : wells) {
        WellInfo wi{};
        wi.well_id = w.id;
        wi.node_idx = 0;
        wi.type = "mock";
        wi.x = w.position.x;
        wi.y = w.position.y;
        wi.z = w.position.z;
        wi.WI = 0.0;
        wi.P_bhp = w.bhp;
        out.push_back(wi);
    }
    return out;
}

bool BuildSharedInputsFromMock(
    const MockData::GridData& g,
    const std::vector<MockData::CellData>& cells,
    const std::vector<MockData::FractureData>& fracs,
    const std::vector<MockData::WellData>& wells,
    GridInfo& outGrid,
    FieldData& outField,
    std::vector<Fracture>& outFracs,
    std::vector<WellInfo>& outWells
) {
    try {
        outGrid = GridInfoFromMock(g);
        outField = FieldDataFromMockCells(g, cells);
        outFracs = FracturesFromMock(fracs);
        outWells = WellsFromMock(wells);

        std::cout << "[Mock->SharedInputs] fractures=" << outFracs.size()
                  << " wells=" << outWells.size()
                  << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Mock->SharedInputs] ERROR: " << e.what() << std::endl;
        return false;
    }
}

