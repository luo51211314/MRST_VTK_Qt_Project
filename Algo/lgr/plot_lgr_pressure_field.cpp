// =============================================================================
// 文件名: plot_pressure_field.cpp
// 描述: 读取 EDFM 模拟结果 (CSV)，利用 OpenCV 绘制 Z=Z0 切片上的压力场与裂缝交线
// 编译环境: Windows (MinGW) / Linux
// 依赖: OpenCV
// =============================================================================

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

// ======================= 数据结构定义 =======================

struct Point3D {
    double x, y, z;
};

struct GridInfo {
    int Nx, Ny, Nz;
    double Lx, Ly, Lz;
    double dx, dy, dz;
};

struct CellData {
    double x, y, z;
    double P, Sw, Sg;
};

struct FracGeo {
    int id;
    Point3D v[4]; // 4 vertices
};

// ======================= 辅助函数 =======================

// 颜色映射：将数值 val (min_v ~ max_v) 映射为 BGR 颜色 (Jet Colormap)
Scalar getJetColor(double val, double min_v, double max_v) {
    if (std::abs(max_v - min_v) < 1e-6) return Scalar(0, 255, 0); // 避免除零

    double norm = (val - min_v) / (max_v - min_v);
    norm = std::max(0.0, std::min(1.0, norm));

    double r = 0, g = 0, b = 0;
    // Jet Colormap 逻辑
    if (norm < 0.25) {
        b = 1.0;
        g = norm * 4.0;
    } else if (norm < 0.5) {
        b = 1.0 - (norm - 0.25) * 4.0;
        g = 1.0;
    } else if (norm < 0.75) {
        g = 1.0;
        r = (norm - 0.5) * 4.0;
    } else {
        g = 1.0 - (norm - 0.75) * 4.0;
        r = 1.0;
    }
    return Scalar(b * 255, g * 255, r * 255);
}

// 坐标转换：物理坐标 (x, y) -> 图像坐标 (col, row)
// 假设物理坐标 (0,0) 在左下角，OpenCV (0,0) 在左上角，需要翻转 Y 轴
Point2f worldToImg(double wx, double wy, double scale, int imgH) {
    float ix = (float)(wx * scale);
    float iy = (float)(imgH - (wy * scale)); 
    return Point2f(ix, iy);
}

// 计算线段 (p1, p2) 与平面 z = z_plane 的交点
bool getIntersection(Point3D p1, Point3D p2, double z_plane, Point3D& out) {
    // 检查线段是否跨越平面
    if ((p1.z < z_plane && p2.z > z_plane) || (p1.z > z_plane && p2.z < z_plane)) {
        double t = (z_plane - p1.z) / (p2.z - p1.z); // 线性插值
        out.x = p1.x + t * (p2.x - p1.x);
        out.y = p1.y + t * (p2.y - p1.y);
        out.z = z_plane;
        return true;
    }
    return false;
}

// 简单的 CSV 行解析器
vector<double> parseCSVLine(const string& str_in) {
    vector<double> values;
    stringstream ss(str_in);
    string token;
    while(getline(ss, token, ',')) {
        try {
            values.push_back(stod(token));
        } catch(...) {
            // 忽略非数字标题行或空值
        }
    }
    return values;
}

// ======================= 主逻辑 =======================

int main(int argc, char** argv) {
    // 默认参数
    double slice_z = 25.0; // 切片深度 (z=250)
    double scale = 1.0;     // 图像缩放 (像素/米) -> 1000m = 1000px
    
    // 允许通过命令行参数修改深度
    if(argc > 1) slice_z = std::stod(argv[1]);
    
    cout << "=== EDFM Visualization Tool ===" << endl;
    cout << "Target Slice Z: " << slice_z << " m" << endl;

    // ---------------------------------------------------------
    // 1. 读取网格信息 (grid_info.csv)
    // ---------------------------------------------------------
    GridInfo grid;
    string gridFilename = "grid_info_lgr.csv";
    ifstream gridFile(gridFilename);
    if(!gridFile.is_open()) { 
        cerr << "Error: Cannot open " << gridFilename << endl; 
        cerr << "Please make sure you ran the simulator and generated the csv files." << endl;
        return -1; 
    }
    
    string csvLine; // 【修改 1】：变量名从 line 改为 csvLine，避免冲突
    // 读取第一行数据
    if(getline(gridFile, csvLine)) {
        vector<double> tokens = parseCSVLine(csvLine);
        if(tokens.size() >= 9) {
            grid.Nx = (int)tokens[0]; grid.Ny = (int)tokens[1]; grid.Nz = (int)tokens[2];
            grid.Lx = tokens[3]; grid.Ly = tokens[4]; grid.Lz = tokens[5];
            grid.dx = tokens[6]; grid.dy = tokens[7]; grid.dz = tokens[8];
        } else {
            cerr << "Error: Invalid format in grid_info_lgr.csv" << endl; return -1;
        }
    }
    gridFile.close();

    cout << "Grid Loaded: " << grid.Nx << "x" << grid.Ny << "x" << grid.Nz 
         << " Size: " << grid.Lx << "x" << grid.Ly << "x" << grid.Lz << endl;

    // 根据网格大小决定图像尺寸
    int imgW = (int)(grid.Lx * scale);
    int imgH = (int)(grid.Ly * scale);
    // 确保图像不要太小或太大
    if (imgW > 2000) { scale *= 2000.0/imgW; imgW = 2000; imgH = (int)(grid.Ly * scale); }
    if (imgW < 500)  { scale *= 500.0/imgW;  imgW = 500;  imgH = (int)(grid.Ly * scale); }
    
    cout << "Image Size: " << imgW << "x" << imgH << " (Scale: " << scale << ")" << endl;
    
    Mat img(imgH, imgW, CV_8UC3, Scalar(255, 255, 255)); // 白色背景

    // ---------------------------------------------------------
    // 2. 读取场数据 (final_field.csv) 并绘制基质压力
    // ---------------------------------------------------------
    string fieldFilename = "final_field_lgr.csv";
    ifstream fieldFile(fieldFilename);
    if(!fieldFile.is_open()) { cerr << "Error: Cannot open " << fieldFilename << endl; return -1; }
    
    getline(fieldFile, csvLine); // 跳过 Header: x,y,z,P,Sw,Sg...

    double minP = 1e9, maxP = -1e9;
    vector<CellData> cells;
    
    // 读入所有网格数据
    while(getline(fieldFile, csvLine)) {
        vector<double> tokens = parseCSVLine(csvLine);
        if(tokens.size() < 6) continue; // 至少要有 leaf_id,parent_id,x,y,z,P

        CellData c;
        c.x = tokens[2]; c.y = tokens[3]; c.z = tokens[4];
        c.P = tokens[5];
        // Sw = tokens[6], Sg = tokens[7] (如果有的话)

        cells.push_back(c);
        if(c.P < minP) minP = c.P;
        if(c.P > maxP) maxP = c.P;
    }
    fieldFile.close();
    
    cout << "Field Data Loaded. Pressure Range: " << minP << " -> " << maxP << " bar" << endl;

    // 绘制基质网格 (如果网格中心 Z 坐标接近切片 Z)
    int cell_draw_count = 0;
    double child_dx = grid.dx / 2.0;
    double child_dy = grid.dy / 2.0;
    for(const auto& c : cells) {
        // 判断该网格是否包含 slice_z
        // 网格范围 [c.z - dz/2, c.z + dz/2]
        if (slice_z >= (c.z - grid.dz/2.0 - 0.1) && slice_z <= (c.z + grid.dz/2.0 + 0.1)) {
            double actual_dx = grid.dx;
            double actual_dy = grid.dy;
            double mod_x = fmod(c.x, grid.dx);
            if (std::abs(mod_x - grid.dx/2.0) > 1e-3) {
                actual_dx = child_dx;
                actual_dy = child_dy;
            }

            // 计算图像坐标
            double x_min = c.x - actual_dx/2.0;
            double y_max = c.y + actual_dy/2.0; // 物理Y最大对应图像Y最小

            Point2f p_tl = worldToImg(x_min, y_max, scale, imgH);
            Point2f p_br = worldToImg(x_min + actual_dx, y_max - actual_dy, scale, imgH);
            
            // 填充颜色
            Scalar color = getJetColor(c.P, minP, maxP);
            rectangle(img, p_tl, p_br, color, FILLED);
            
            // 可选：画个淡淡的网格线
            rectangle(img, p_tl, p_br, Scalar(150,150,150), 1);
            cell_draw_count++;
        }
    }
    cout << "Drew " << cell_draw_count << " matrix cells on plane Z=" << slice_z << endl;

    // ---------------------------------------------------------
    // 3. 读取裂缝几何 (fracture_geometry.csv) 并绘制交线
    // ---------------------------------------------------------
    string fracFilename = "fracture_geometry_lgr.csv";
    ifstream fracFile(fracFilename);
    if(!fracFile.is_open()) { cerr << "Error: Cannot open " << fracFilename << endl; return -1; }
    
    getline(fracFile, csvLine); // 跳过 Header
    
    vector<FracGeo> fracs;
    while(getline(fracFile, csvLine)) {
        vector<double> tokens = parseCSVLine(csvLine);
        if(tokens.size() < 13) continue; // id + 4*3 coords

        FracGeo f;
        f.id = (int)tokens[0];
        f.v[0] = {tokens[1], tokens[2], tokens[3]};
        f.v[1] = {tokens[4], tokens[5], tokens[6]};
        f.v[2] = {tokens[7], tokens[8], tokens[9]};
        f.v[3] = {tokens[10], tokens[11], tokens[12]};
        fracs.push_back(f);
    }
    fracFile.close();

    cout << "Processing " << fracs.size() << " fractures..." << endl;
    
    int intersection_count = 0;
    for(const auto& f : fracs) {
        vector<Point3D> intersections;
        
        // 四边形的四条边
        int idx[] = {0, 1, 2, 3, 0};
        for(int i=0; i<4; ++i) {
            Point3D pt;
            if(getIntersection(f.v[idx[i]], f.v[idx[i+1]], slice_z, pt)) {
                intersections.push_back(pt);
            }
        }
        
        // 如果有两个交点，则画一条线
        if(intersections.size() >= 2) {
            Point2f p1 = worldToImg(intersections[0].x, intersections[0].y, scale, imgH);
            Point2f p2 = worldToImg(intersections[1].x, intersections[1].y, scale, imgH);
            
            // 【修改 2】：调用 OpenCV line 函数，前面加上 cv:: 以防万一，
            // 且因为上面的变量已经改名，这里其实不会再冲突了
            cv::line(img, p1, p2, Scalar(0, 0, 0), 2, LINE_AA);
            intersection_count++;
        }
    }
    cout << "Found " << intersection_count << " fracture intersections." << endl;

    // ---------------------------------------------------------
    // 4. 添加标注并保存
    // ---------------------------------------------------------
    string txt_depth = "Depth Z = " + to_string((int)slice_z) + " m";
    string txt_max = "Max P: " + to_string((int)maxP) + " bar";
    string txt_min = "Min P: " + to_string((int)minP) + " bar";
    
    // 简单图例背景
    rectangle(img, Point(0,0), Point(200, 100), Scalar(255,255,255), FILLED);
    putText(img, txt_depth, Point(10, 30), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0,0,0), 2);
    putText(img, txt_max,   Point(10, 60), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(0,0,255), 1);
    putText(img, txt_min,   Point(10, 85), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255,0,0), 1);

    // 文件名
    string outFile = "pressure_slice_z" + to_string((int)slice_z) + ".png";
    imwrite(outFile, img);
    
    cout << "Success! Saved visualization to: " << outFile << endl;

    return 0;
}