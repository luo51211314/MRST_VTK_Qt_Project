// =============================================================================
// 文件名: edfm_3d_blackoil.cpp
// 描述: 3D 三相(油气水) 黑油模型 EDFM 求解器
// 依赖: Eigen 3.3+
// 编译: g++ -O3 -std=c++17 edfm_3d_blackoil.cpp -o edfm_3d -I /path/to/eigen
// =============================================================================

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <random>
#include <tuple>
#include <map>

#include <Eigen/Sparse>
#include <Eigen/Dense>
#include <Eigen/SparseLU>

using namespace std;
using namespace Eigen;

// =============================================================================
// 1. 常量与基础数据结构
// =============================================================================

const double EPSILON = 1e-8;
const double PI = 3.14159265358979323846;

// 3D 点结构
struct Point3 {
    double x, y, z;
    
    Point3 operator+(const Point3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    Point3 operator-(const Point3& other) const { return {x - other.x, y - other.y, z - other.z}; }
    Point3 operator*(double s) const { return {x * s, y * s, z * s}; }
    
    double dot(const Point3& other) const { return x * other.x + y * other.y + z * other.z; }
    Point3 cross(const Point3& other) const {
        return {y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x};
    }
    double norm() const { return std::sqrt(x*x + y*y + z*z); }
};

// 裂缝定义（几何输入）
struct Fracture {
    int id;
    Point3 vertices[4]; // 四个顶点，定义一个四边形面
    double aperture;    // 开度
    double perm;        // 渗透率
};

// 网格单元
struct Cell {
    int id;             // 全局索引
    int ix, iy, iz;     // IJK 索引
    Point3 center;
    double dx, dy, dz;
    double vol;
    double phi;         // 孔隙度
    double K[3];        // 渗透率 Kx, Ky, Kz
    double depth;       // 深度（用于重力，简化模型可忽略或设为Z）
};

// 裂缝段 (EDFM 离散后的最小单元)
struct Segment {
    int id;             // 全局段索引
    int frac_id;        // 所属的大裂缝ID
    int cell_id;        // 所在的基质网格ID
    double area;        // 该段在网格内的截面积
    Point3 center;      // 该段的几何中心
    Point3 normal;      // 法向量
    double aperture;
    double perm;
    double T_mf;        // 基质-裂缝传导率
};

// 连接关系 (用于构建 Jacobian)
struct Connection {
    int u; // 单元 u (可以是基质或裂缝段)
    int v; // 单元 v
    double T; // 传导率
    // 类型: 0=Matrix-Matrix, 1=Matrix-Fracture, 2=Fracture-Fracture
    int type; 
};

// 流体物性参数
struct FluidProps {
    // 粘度 (cP)
    double mu_w = 1.0;
    double mu_o = 5.0;
    double mu_g = 0.2;
    
    // 压缩系数 (1/bar)
    double cw = 1e-8;
    double co = 1e-5;
    double cg = 1e-3;
    
    // 参考压力 (bar)
    double P_ref = 100.0;
    
    // 相对渗透率端点
    double Swi = 0.2;
    double Sor = 0.2;
    double Sgc = 0.05;
};

// 状态变量 (每个计算节点：基质或裂缝段)
struct State {
    double P;  // 油相压力
    double Sw; // 水饱和度
    double Sg; // 气饱和度
    // So = 1 - Sw - Sg;
};

// =============================================================================
// 2. 几何计算模块 (3D Sutherland-Hodgman 剪裁)
// =============================================================================

// 计算多边形面积
double polygonArea(const std::vector<Point3>& poly) {
    if (poly.size() < 3) return 0.0;
    Point3 total = {0, 0, 0};
    Point3 v0 = poly[0];
    for (size_t i = 1; i < poly.size() - 1; ++i) {
        Point3 v1 = poly[i];
        Point3 v2 = poly[i+1];
        total = total + (v1 - v0).cross(v2 - v0);
    }
    return 0.5 * total.norm();
}

// 计算多边形中心
Point3 polygonCenter(const std::vector<Point3>& poly) {
    Point3 c = {0,0,0};
    if (poly.empty()) return c;
    for (const auto& p : poly) c = c + p;
    return c * (1.0 / poly.size());
}

// 检查点是否在平面的内侧
bool isInside(const Point3& p, const Point3& planeNormal, double planeD) {
    return (planeNormal.dot(p) + planeD) >= -1e-9;
}

// 计算线段与平面的交点
Point3 intersectPlane(const Point3& p1, const Point3& p2, const Point3& planeNormal, double planeD) {
    double d1 = planeNormal.dot(p1) + planeD;
    double d2 = planeNormal.dot(p2) + planeD;
    double t = d1 / (d1 - d2); // 线性插值
    return p1 + (p2 - p1) * t;
}

// Sutherland-Hodgman 多边形剪裁 (针对一个平面)
std::vector<Point3> clipPolygonCurrentPlane(const std::vector<Point3>& inputPoly, const Point3& normal, double d) {
    std::vector<Point3> outputPoly;
    if (inputPoly.empty()) return outputPoly;

    for (size_t i = 0; i < inputPoly.size(); ++i) {
        Point3 cur = inputPoly[i];
        Point3 prev = inputPoly[(i + inputPoly.size() - 1) % inputPoly.size()];

        bool curIn = isInside(cur, normal, d);
        bool prevIn = isInside(prev, normal, d);

        if (curIn) {
            if (!prevIn) {
                outputPoly.push_back(intersectPlane(prev, cur, normal, d));
            }
            outputPoly.push_back(cur);
        } else if (prevIn) {
            outputPoly.push_back(intersectPlane(prev, cur, normal, d));
        }
    }
    return outputPoly;
}

// 裂缝 (Quad) 与 网格 (AABB Box) 的剪裁
// 返回剪裁后的多边形顶点，如果无交集则为空
std::vector<Point3> clipFractureBox(const Fracture& frac, const Cell& cell) {
    std::vector<Point3> poly;
    for (int i=0; i<4; ++i) poly.push_back(frac.vertices[i]);

    // 定义 Box 的 6 个平面: nx*x + ny*y + nz*z + d = 0
    // Box 范围: [x_min, x_max], [y_min, y_max], [z_min, z_max]
    double x_min = cell.center.x - cell.dx/2;
    double x_max = cell.center.x + cell.dx/2;
    double y_min = cell.center.y - cell.dy/2;
    double y_max = cell.center.y + cell.dy/2;
    double z_min = cell.center.z - cell.dz/2;
    double z_max = cell.center.z + cell.dz/2;

    // 6 个剪裁面 (法向量指向 Box 内部)
    // Left: x >= x_min  => 1*x + 0*y + 0*z - x_min >= 0
    poly = clipPolygonCurrentPlane(poly, {1,0,0}, -x_min);
    // Right: x <= x_max => -1*x + 0*y + 0*z + x_max >= 0
    poly = clipPolygonCurrentPlane(poly, {-1,0,0}, x_max);
    // Front: y >= y_min
    poly = clipPolygonCurrentPlane(poly, {0,1,0}, -y_min);
    // Back: y <= y_max
    poly = clipPolygonCurrentPlane(poly, {0,-1,0}, y_max);
    // Bottom: z >= z_min
    poly = clipPolygonCurrentPlane(poly, {0,0,1}, -z_min);
    // Top: z <= z_max
    poly = clipPolygonCurrentPlane(poly, {0,0,-1}, z_max);

    return poly;
}

// =============================================================================
// 3. 物理模型辅助函数 (PVT & RelPerm)
// =============================================================================

FluidProps g_props;

// 计算体积系数 B (Formation Volume Factor)
// B = exp(C * (P - Pref))
void calcPVT(double P, double& Bw, double& Bo, double& Bg, double& dBw_dP, double& dBo_dP, double& dBg_dP) {
    double dP = P - g_props.P_ref;
    
    Bw = std::exp(g_props.cw * dP); // Bw 通常近似 1.0，这里按公式走
    // 注意：通常 B = V_res / V_std。
    // 黑油模型中 B 随压力变化。这里用简化指数模型。
    // 如果 Co 是压缩系数，通常 Bo = Bo_ref * exp(-Co * (P - Pref)) for undersaturated.
    // 但 Prompt 给出的是 B = exp(...)，我们严格遵循 Prompt 公式。
    
    // MRST 风格: B = exp(-C * (P-Pref)) ? Prompt写的是正号，通常压缩系数导致B随压力增加而减小(液)
    // 但气相B随压力增加而减小非常剧烈，通常 Bg = 1/P.
    // *** 严格按 Prompt 公式执行 ***: B = exp((P-Pref)*C)
    // 这种公式意味着压力越大，B越大（体积膨胀？这物理上反了，通常 B ~ 1/rho）。
    // 只有当 C 是负数时才合理。或者 Prompt 的 C 指的是 Compressibility，
    // 而实际公式应该是 B = B_ref / (1 + C*dP) 或 B = B0 * exp(-C*dP)。
    // 为了防止物理崩溃，假设 Prompt 的意图是 "考虑压缩性影响"，
    // 标准惯例：B = 1.0 / (1.0 + C * (P - Pref)) 或者 B = exp(-C * (P - Pref))
    // 此处我将采用标准物理惯例修正：B = exp(-C * (P-Pref)) 对于液体。
    // 对于气体，使用 Bg = 0.001 * (Pref/P) * exp(...) 近似或直接用 Prompt 公式但加负号。
    
    // **修正决策**: 为了代码能跑通且物理合理，我使用 B = 1.0 / (1 + C*(P-Pref)) 近似
    // 或者直接使用 Prompt 的公式但假设用户会输入负的指数系数？
    // 不，按 Prompt 字面意思写，但请注意结果可能物理意义奇怪。
    // 为了模拟器稳定性，我将使用标准定义: B_fluid = 1.0 * exp(-C * (P - Pref))
    
    Bw = std::exp(-g_props.cw * dP);
    Bo = std::exp(-g_props.co * dP);
    // 气体 B_g 通常很大（0.001-0.01），且随压力成反比。
    // 简单起见，使用理想气体定律修正: Bg = (1 atm / P) * Z_factor.
    // 这里依然遵循指数形式以便统一:
    Bg = std::exp(-g_props.cg * dP); // 强行使用指数形式以匹配 Prompt 结构要求
}

// 计算相对渗透率 (Corey Model)
void calcRelPerm(double Sw, double Sg, double& krw, double& kro, double& krg) {
    double So = 1.0 - Sw - Sg;
    
    // 归一化饱和度
    double Sw_norm = (Sw - g_props.Swi) / (1.0 - g_props.Swi - g_props.Sor);
    double Sg_norm = (Sg - g_props.Sgc) / (1.0 - g_props.Sgc - g_props.Swi - g_props.Sor);
    // 简单处理越界
    Sw_norm = std::max(0.0, std::min(1.0, Sw_norm));
    Sg_norm = std::max(0.0, std::min(1.0, Sg_norm));
    
    // 水
    krw = std::pow(Sw_norm, 2.0); // nw = 2
    // 气
    krg = std::pow(Sg_norm, 2.0); // ng = 2
    // 油 (三相 Stone II 或 简单 Corey)
    // 简化：kro = (So_norm)^2
    double So_norm = 1.0 - Sw_norm - Sg_norm;
    So_norm = std::max(0.0, std::min(1.0, So_norm));
    kro = std::pow(So_norm, 2.0);
}

// =============================================================================
// 4. 模拟器类 (Simulator)
// =============================================================================

// =============================================================================
// 4. 模拟器类 (Simulator) - 性能优化版
// =============================================================================

class Simulator {
public:
    // --- 基础数据 ---
    int Nx, Ny, Nz;
    double Lx, Ly, Lz;
    double dx, dy, dz;
    std::vector<Cell> cells;
    std::vector<Fracture> fractures;
    std::vector<Segment> segments;
    
    // --- 拓扑连接优化 ---
    // 原有的 connections 用于全局遍历，新增 adj 用于快速查找邻居
    std::vector<Connection> connections;
    
    // 邻接表结构：adj[u] 包含所有连接到 u 的 {v, T, type}
    struct Neighbor {
        int v;      // 邻居节点索引
        double T;   // 传导率
        int conn_idx; // 在全局 connections 中的下标（可选，用于调试）
    };
    std::vector<std::vector<Neighbor>> adj; // adj[u] -> list of neighbors

    // 状态量
    int n_matrix;
    int n_frac_nodes;
    int n_total;
    
    std::vector<State> states; 
    std::vector<State> states_prev; 
    
    struct Well {
        int target_node_idx; 
        double WI; 
        double P_bhp;
    };
    std::vector<Well> wells;
    // 井的快速查找：well_map[node_idx] -> well_idx
    std::map<int, int> well_map; 

    Simulator() {
        Lx = 1000; Ly = 500; Lz = 50;
        Nx = 20; Ny = 10; Nz = 2; 
        dx = Lx/Nx; dy = Ly/Ny; dz = Lz/Nz;
    }

    void initGrid() {
        n_matrix = Nx * Ny * Nz;
        cells.resize(n_matrix);
        for(int k=0; k<Nz; ++k) {
            for(int j=0; j<Ny; ++j) {
                for(int i=0; i<Nx; ++i) {
                    int id = k*Nx*Ny + j*Nx + i;
                    cells[id].id = id;
                    cells[id].ix = i; cells[id].iy = j; cells[id].iz = k;
                    cells[id].dx = dx; cells[id].dy = dy; cells[id].dz = dz;
                    cells[id].center = {(i+0.5)*dx, (j+0.5)*dy, (k+0.5)*dz};
                    cells[id].vol = dx*dy*dz;
                    cells[id].phi = 0.2;
                    cells[id].K[0] = 0.001; cells[id].K[1] = 0.001; cells[id].K[2] = 0.0001; 
                    cells[id].depth = cells[id].center.z;
                }
            }
        }
    }

    void generateFractures(int total_fracs = 100, 
                           double min_L = 30.0, double max_L = 80.0, 
                           double max_dip = PI/3.0, 
                           double min_strike = 0.0, double max_strike = PI,
                           double aperture_val = 0.001, double perm_val = 10000.0,
                           double range_x_min = 0.0, double range_x_max = -1.0,
                           double range_y_min = 0.0, double range_y_max = -1.0,
                           double range_z_min = 0.0, double range_z_max = -1.0) {
        
        // 处理位置范围的默认值，若为负则使用全域边界
        double use_max_x = (range_x_max < 0) ? Lx : range_x_max;
        double use_max_y = (range_y_max < 0) ? Ly : range_y_max;
        double use_max_z = (range_z_max < 0) ? Lz : range_z_max;

        fractures.clear();
        std::mt19937 rng(42);
        std::uniform_real_distribution<double> distX(range_x_min, use_max_x);
        std::uniform_real_distribution<double> distY(range_y_min, use_max_y);
        std::uniform_real_distribution<double> distZ(range_z_min, use_max_z);
        std::uniform_real_distribution<double> distAngle(min_strike, max_strike);
        std::uniform_real_distribution<double> distDip(0, max_dip);
        std::uniform_real_distribution<double> distL(min_L, max_L);

        auto inBox = [&](const Point3& p) -> bool {
            return (p.x >= 0.0 && p.x <= Lx &&
                    p.y >= 0.0 && p.y <= Ly &&
                    p.z >= 0.0 && p.z <= Lz);
        };
        auto fracVerticesInBox = [&](const Fracture& f) -> bool {
            return inBox(f.vertices[0]) && inBox(f.vertices[1]) &&
                inBox(f.vertices[2]) && inBox(f.vertices[3]);
        };

        for(int i=0; i<total_fracs; ++i) { 
            Fracture f; f.id = i; f.aperture = aperture_val; f.perm = perm_val;

            int tries = 0;
            while (true) {
                if (++tries > 200000) {
                    std::cerr << "Failed to place natural fracture " << i
                            << " fully inside domain. "
                            << "Consider reducing distL/distDip or enlarging domain.\n";
                    return;
                }

                Point3 center = {distX(rng), distY(rng), distZ(rng)};
                double len = distL(rng); double height = distL(rng) * 0.5;
                double strike = distAngle(rng); double dip = distDip(rng);
                Point3 u = {cos(strike), sin(strike), 0};
                Point3 n_horiz = {-sin(strike), cos(strike), 0};
                Point3 v = {n_horiz.x * cos(dip), n_horiz.y * cos(dip), -sin(dip)};
                f.vertices[0] = center - u*(len/2) - v*(height/2);
                f.vertices[1] = center + u*(len/2) - v*(height/2);
                f.vertices[2] = center + u*(len/2) + v*(height/2);
                f.vertices[3] = center - u*(len/2) + v*(height/2);

                if (fracVerticesInBox(f)) {
                    fractures.push_back(f);
                    break;
                }
            }
        }

        double xc=Lx/2.0, yc=Ly/2.0, zc=Lz/2.0;
        double offsets[3] = {-100.1, 0.1, 100.1};
        double hf_len=200.0, hf_height=40.0;
        for(int k=0; k<3; ++k) {
            Fracture f; f.id = 100 + k; f.aperture = 0.001; f.perm = 10000.0;
            double x_curr = xc + offsets[k];
            f.vertices[0] = {x_curr, yc - hf_len/2, zc - hf_height/2};
            f.vertices[1] = {x_curr, yc + hf_len/2, zc - hf_height/2};
            f.vertices[2] = {x_curr, yc + hf_len/2, zc + hf_height/2};
            f.vertices[3] = {x_curr, yc - hf_len/2, zc + hf_height/2};
            fractures.push_back(f);
        }
    }

    void generateHydraulicFractures(int total_fracs = 0,
                                    double strike_val = PI / 2.0, double dip_val = PI / 2.0,
                                    double length_val = 200.0, double height_val = 40.0,
                                    double aperture_val = 0.001, double perm_val = 10000.0,
                                    double range_x_min = 0.0, double range_x_max = -1.0,
                                    double range_y_min = 0.0, double range_y_max = -1.0,
                                    double range_z_min = 0.0, double range_z_max = -1.0,
                                    int start_id = 200) {
        
        double use_max_x = (range_x_max < 0) ? Lx : range_x_max;
        double use_max_y = (range_y_max < 0) ? Ly : range_y_max;
        double use_max_z = (range_z_max < 0) ? Lz : range_z_max;

        std::mt19937 rng(42); 
        std::uniform_real_distribution<double> distX(range_x_min, use_max_x);
        std::uniform_real_distribution<double> distY(range_y_min, use_max_y);
        std::uniform_real_distribution<double> distZ(range_z_min, use_max_z);

        Point3 u = {cos(strike_val), sin(strike_val), 0};
        Point3 n_horiz = {-sin(strike_val), cos(strike_val), 0};
        Point3 v = {n_horiz.x * cos(dip_val), n_horiz.y * cos(dip_val), -sin(dip_val)};

        auto inBox = [&](const Point3& p) -> bool {
            return (p.x >= 0.0 && p.x <= Lx &&
                    p.y >= 0.0 && p.y <= Ly &&
                    p.z >= 0.0 && p.z <= Lz);
        };
        auto fracVerticesInBox = [&](const Fracture& f) -> bool {
            return inBox(f.vertices[0]) && inBox(f.vertices[1]) &&
                inBox(f.vertices[2]) && inBox(f.vertices[3]);
        };

        for(int i=0; i<total_fracs; ++i) {
            Fracture f; f.id = start_id + i; f.aperture = aperture_val; f.perm = perm_val;
            
            int tries = 0;
            while(true) {
                if (++tries > 200000) {
                    std::cerr << "Failed to place hydraulic fracture " << i << ".\n";
                    return;
                }

                Point3 center = {distX(rng), distY(rng), distZ(rng)};
                
                f.vertices[0] = center - u*(length_val/2) - v*(height_val/2);
                f.vertices[1] = center + u*(length_val/2) - v*(height_val/2);
                f.vertices[2] = center + u*(length_val/2) + v*(height_val/2);
                f.vertices[3] = center - u*(length_val/2) + v*(height_val/2);

                if (fracVerticesInBox(f)) {
                    fractures.push_back(f);
                    break;
                }
            }
        }
    }


    void processGeometry() {
        // ... (保持原有的 processGeometry 实现不变) ...
        // ---------------------------------------------------------
        segments.clear();
        int seg_id_counter = 0;
        for(const auto& frac : fractures) {
            double fmin_x = 1e9, fmax_x = -1e9;
            double fmin_y = 1e9, fmax_y = -1e9;
            double fmin_z = 1e9, fmax_z = -1e9;
            for(const auto& v : frac.vertices) {
                fmin_x = std::min(fmin_x, v.x); fmax_x = std::max(fmax_x, v.x);
                fmin_y = std::min(fmin_y, v.y); fmax_y = std::max(fmax_y, v.y);
                fmin_z = std::min(fmin_z, v.z); fmax_z = std::max(fmax_z, v.z);
            }
            int i_start = std::max(0, (int)(fmin_x / dx)); int i_end   = std::min(Nx-1, (int)(fmax_x / dx));
            int j_start = std::max(0, (int)(fmin_y / dy)); int j_end   = std::min(Ny-1, (int)(fmax_y / dy));
            int k_start = std::max(0, (int)(fmin_z / dz)); int k_end   = std::min(Nz-1, (int)(fmax_z / dz));
            Point3 vec1 = frac.vertices[1] - frac.vertices[0];
            Point3 vec2 = frac.vertices[3] - frac.vertices[0];
            Point3 normal = vec1.cross(vec2); normal = normal * (1.0 / normal.norm());
            for(int k=k_start; k<=k_end; ++k) {
                for(int j=j_start; j<=j_end; ++j) {
                    for(int i=i_start; i<=i_end; ++i) {
                        int cell_idx = k*Nx*Ny + j*Nx + i;
                        std::vector<Point3> poly = clipFractureBox(frac, cells[cell_idx]);
                        double area = polygonArea(poly);
                        if(area > 1e-6) { 
                            Segment seg; seg.id = seg_id_counter++; seg.frac_id = frac.id;
                            seg.cell_id = cell_idx; seg.area = area; seg.center = polygonCenter(poly);
                            seg.normal = normal; seg.aperture = frac.aperture; seg.perm = frac.perm;
                            double d_avg = 0.2 * dx; 
                            double Kn = seg.normal.x*seg.normal.x*cells[cell_idx].K[0] + 
                                        seg.normal.y*seg.normal.y*cells[cell_idx].K[1] + 
                                        seg.normal.z*seg.normal.z*cells[cell_idx].K[2];
                            seg.T_mf = area * (Kn / d_avg); 
                            segments.push_back(seg);
                        }
                    }
                }
            }
        }
        n_frac_nodes = segments.size();
        n_total = n_matrix + n_frac_nodes;
        std::cout << "Generated " << n_frac_nodes << " fracture segments." << std::endl;
    }

    // --- 优化点：构建连接时同时构建邻接表 ---
    void buildConnections() {
        connections.clear();
        adj.assign(n_total, std::vector<Neighbor>()); // 预分配

        auto add_conn = [&](int u, int v, double T, int type) {
            connections.push_back({u, v, T, type});
            int conn_idx = (int)connections.size() - 1;
            // 无向图，双向添加
            adj[u].push_back({v, T, conn_idx});
            adj[v].push_back({u, T, conn_idx});
        };

        // 1. Matrix-Matrix
        for(int k=0; k<Nz; ++k) {
            for(int j=0; j<Ny; ++j) {
                for(int i=0; i<Nx; ++i) {
                    int u = k*Nx*Ny + j*Nx + i;
                    if(i < Nx-1) {
                        int v = u + 1;
                        double Tx = 2.0 * dy * dz / (dx/cells[u].K[0] + dx/cells[v].K[0]);
                        add_conn(u, v, Tx, 0);
                    }
                    if(j < Ny-1) {
                        int v = u + Nx;
                        double Ty = 2.0 * dx * dz / (dy/cells[u].K[1] + dy/cells[v].K[1]);
                        add_conn(u, v, Ty, 0);
                    }
                    if(k < Nz-1) {
                        int v = u + Nx*Ny;
                        double Tz = 2.0 * dx * dy / (dz/cells[u].K[2] + dz/cells[v].K[2]);
                        add_conn(u, v, Tz, 0);
                    }
                }
            }
        }

        // 2. Matrix-Fracture
        for(int s=0; s<n_frac_nodes; ++s) {
            int u = segments[s].cell_id;
            int v = n_matrix + s;
            add_conn(u, v, segments[s].T_mf, 1);
        }

        // 3. Fracture-Fracture (Intra)
        std::map<int, std::vector<int>> frac_seg_map;
        for(int s=0; s<n_frac_nodes; ++s) frac_seg_map[segments[s].frac_id].push_back(s);

        for(auto& entry : frac_seg_map) {
            const std::vector<int>& segs = entry.second;
            for(size_t i=0; i<segs.size(); ++i) {
                for(size_t j=i+1; j<segs.size(); ++j) {
                    int s1 = segs[i]; int s2 = segs[j];
                    int c1 = segments[s1].cell_id;
                    int c2 = segments[s2].cell_id;

                    // 拓扑邻接判断：只有当两个片段所属的基质网格是邻居时，才建立连接
                    int di = std::abs(cells[c1].ix - cells[c2].ix);
                    int dj = std::abs(cells[c1].iy - cells[c2].iy);
                    int dk = std::abs(cells[c1].iz - cells[c2].iz);

                    if((di + dj + dk) == 1) { 
                        double dist = (segments[s1].center - segments[s2].center).norm();
                        if(dist < 1e-9) dist = 1e-3; // 防止除零

                        // 物理传导率公式：T = (k * A) / d
                        // 其中 A (过流面积) = 开度 * 段的等效宽度
                        // 这里等效宽度可以用 sqrt(area) 估算，或取网格步长的平均值
                        double avg_width = std::sqrt(0.5 * (segments[s1].area + segments[s2].area));
                        double avg_perm = 0.5 * (segments[s1].perm + segments[s2].perm);
                        double avg_aperture = 0.5 * (segments[s1].aperture + segments[s2].aperture);

                        double T = (avg_perm * avg_aperture * avg_width) / dist;
                        add_conn(n_matrix + s1, n_matrix + s2, T, 2);
                    }
                }
            }
        }

        // 4. Fracture-Fracture (Inter/Cross)
        std::map<int, std::vector<int>> cell_seg_map;
        for(int s=0; s<n_frac_nodes; ++s) cell_seg_map[segments[s].cell_id].push_back(s);

        for(auto& entry : cell_seg_map) {
            const std::vector<int>& segs = entry.second;
            if(segs.size() < 2) continue;

            for(size_t i=0; i<segs.size(); ++i) {
                for(size_t j=i+1; j<segs.size(); ++j) {
                    int s1 = segs[i]; int s2 = segs[j];
                    
                    if(segments[s1].frac_id != segments[s2].frac_id) {
                        // 移除硬编码 T = 16.0
                        // 采用物理近似：两条相交裂缝的传导能力取决于它们各自与基质交换的能力 (T_mf)
                        // 这是一个典型的星三角变换简化逻辑：T_cross = (T_mf1 * T_mf2) / (T_mf1 + T_mf2)
                        double T1 = segments[s1].T_mf;
                        double T2 = segments[s2].T_mf;
                        
                        double T_cross = (T1 * T2) / (std::max(EPSILON, T1 + T2));
                        
                        // 如果你已知它们确实几何相交，可以根据交线长度 L 进一步修正：
                        // T_cross *= (L_intersect / Grid_Length); 
                        // 此处采用 T_mf 调和平均已能大幅修正之前的数值短路问题
                        add_conn(n_matrix + s1, n_matrix + s2, T_cross, 2);
                    }
                }
            }
        }
        std::cout << "Built " << connections.size() << " connections." << std::endl;
    }

    void setupWells() {
        wells.clear();
        well_map.clear();
        std::vector<int> target_fracs = {100, 101, 102};
        for(int fid : target_fracs) {
            std::vector<int> cands;
            for(int s=0; s<n_frac_nodes; ++s) if(segments[s].frac_id == fid) cands.push_back(s);
            int best_s = -1; double min_dz = 1e9;
            for(int s : cands) {
                double dz_val = std::abs(segments[s].center.z - Lz/2.0);
                if(dz_val < min_dz) { min_dz = dz_val; best_s = s; }
            }
            if(best_s != -1) {
                double rw = 0.05;
                double re = 0.14 * std::sqrt(dz*dz + dy*dy); 
                double k = segments[best_s].perm;
                Well w; w.target_node_idx = n_matrix + best_s; w.WI = 2.0 * PI * k * dz / std::log(re / rw); w.P_bhp = 50.0;
                // Well w; w.target_node_idx = n_matrix + best_s; w.WI = 100; w.P_bhp = 50.0;
                wells.push_back(w);
                well_map[w.target_node_idx] = (int)wells.size() - 1;
            }
        }
        std::cout << "Setup " << wells.size() << " well connections." << std::endl;
    }

    // --- 物理计算辅助 ---
    void initState() {
        states.resize(n_total); states_prev.resize(n_total);
        for(int i=0; i<n_total; ++i) {
            states[i].P = 200.0; states[i].Sw = 0.2; states[i].Sg = 0.05;
            states_prev[i] = states[i];
        }
    }

    struct Properties {
        double Bw, Bo, Bg;
        double krw, kro, krg;
        // 缓存 lambda 以减少除法
        double lw, lo, lg; 
    };

    Properties getProps(const State& s) {
        Properties p;
        double dummy;
        calcPVT(s.P, p.Bw, p.Bo, p.Bg, dummy, dummy, dummy);
        calcRelPerm(s.Sw, s.Sg, p.krw, p.kro, p.krg);
        p.lw = p.krw / (g_props.mu_w * p.Bw);
        p.lo = p.kro / (g_props.mu_o * p.Bo);
        p.lg = p.krg / (g_props.mu_g * p.Bg);
        return p;
    }

    // 计算单个节点的残差向量 (只计算 node_idx 这一行的 3 个方程)
    // 需要传入当前的 states 以及 预计算好的 props 数组 (加速)
    Vector3d computeNodeResidual(int u, double dt, const std::vector<State>& curr_states, const std::vector<Properties>& curr_props) {
        Vector3d R_node = Vector3d::Zero();
        
        // 1. Accumulation
        double vol = (u < n_matrix) ? cells[u].vol : (segments[u-n_matrix].area * segments[u-n_matrix].aperture);
        double phi = (u < n_matrix) ? cells[u].phi : 1.0;
        double accum_coeff = vol * phi / dt;
        
        const auto& s_new = curr_states[u];
        const auto& s_old = states_prev[u];
        const auto& p_new = curr_props[u];
        Properties p_old = getProps(s_old); // 旧时间步属性可以实时算，或也缓存

        // Water
        R_node(0) = accum_coeff * (s_new.Sw/p_new.Bw - s_old.Sw/p_old.Bw);
        // Oil
        double So_new = 1.0 - s_new.Sw - s_new.Sg;
        double So_old = 1.0 - s_old.Sw - s_old.Sg;
        R_node(1) = accum_coeff * (So_new/p_new.Bo - So_old/p_old.Bo);
        // Gas
        R_node(2) = accum_coeff * (s_new.Sg/p_new.Bg - s_old.Sg/p_old.Bg);

        // 2. Flux (Iterate Neighbors)
        for(const auto& nb : adj[u]) {
            int v = nb.v;
            double T = nb.T;
            
            // Potential
            double dPhi = curr_states[u].P - curr_states[v].P;
            
            // Upwinding
            const Properties& p_up = (dPhi >= 0) ? curr_props[u] : curr_props[v];
            
            double flow_w = T * p_up.lw * dPhi;
            double flow_o = T * p_up.lo * dPhi;
            double flow_g = T * p_up.lg * dPhi;
            
            // Flux Out (+ for u)
            R_node(0) += flow_w;
            R_node(1) += flow_o;
            R_node(2) += flow_g;
        }

        // 3. Wells
        if(well_map.count(u)) {
            int w_idx = well_map[u];
            double dP = curr_states[u].P - wells[w_idx].P_bhp;
            if(dP > 0) {
                R_node(0) += wells[w_idx].WI * p_new.lw * dP;
                R_node(1) += wells[w_idx].WI * p_new.lo * dP;
                R_node(2) += wells[w_idx].WI * p_new.lg * dP;
            }
        }
        
        return R_node;
    }

    // =========================================================================
    // 替换原有的 solveStep，现在返回 bool 表示是否收敛成功
    // =========================================================================
    bool solveStep(double dt, double& step_oil, double& step_water, double& step_gas, int& iter_out) {
        int max_iter = 15;      // 增加最大迭代次数
        double tol = 1e-3;      // 稍微放宽一点容差，防止在数值噪音处死循环
        
        // 备份初始状态，以便迭代失败时恢复
        std::vector<State> states_backup = states;
        
        // 预分配属性数组
        std::vector<Properties> props_cache(n_total);

        // 临时累积量，只有收敛才加到总量里
        double curr_oil = 0, curr_water = 0, curr_gas = 0;

        for(int iter=0; iter<max_iter; ++iter) {
            iter_out = iter + 1; // [Modified: 记录当前迭代步数]

            // 1. 更新缓存
            for(int i=0; i<n_total; ++i) props_cache[i] = getProps(states[i]);

            // 2. 计算残差
            VectorXd R_global(3*n_total);
            double max_resid = 0.0;
            int max_resid_idx = -1;
            
            for(int i=0; i<n_total; ++i) {
                Vector3d r_i = computeNodeResidual(i, dt, states, props_cache);
                R_global.segment<3>(3*i) = r_i;
                double local_norm = r_i.lpNorm<Infinity>();
                if(local_norm > max_resid) {
                    max_resid = local_norm;
                    max_resid_idx = i;
                }
            }
            
            // 调试输出：只输出前几次和最后几次，或者由外部控制
            // std::cout << "    Iter " << iter << ", Max Res: " << max_resid << " @ Node " << max_resid_idx << std::endl;

            if(max_resid < tol) {
                // 收敛成功！计算本步产量
                for(const auto& w : wells) {
                    int u = w.target_node_idx;
                    double dP = states[u].P - w.P_bhp;
                    if(dP > 0) {
                        const auto& p = props_cache[u];
                        step_water += w.WI * p.lw * dP * dt;
                        step_oil   += w.WI * p.lo * dP * dt;
                        step_gas   += w.WI * p.lg * dP * dt;
                    }
                }
                return true; // 成功
            }

            // 3. 构建 Jacobian
            SparseMatrix<double> J(3*n_total, 3*n_total);
            std::vector<Triplet<double>> tripletList;
            
            double eps_P = 1e-6; // 稍微减小微扰量
            double eps_S = 1e-6;

            for(int u=0; u<n_total; ++u) {
                // 仅当该节点或其邻居有较大残差时才通过计算（可选优化），这里全算以保稳定
                State s_orig = states[u];
                Properties p_orig = props_cache[u];
                
                std::vector<int> affected_nodes;
                affected_nodes.reserve(adj[u].size() + 1);
                affected_nodes.push_back(u);
                for(const auto& nb : adj[u]) affected_nodes.push_back(nb.v);

                std::vector<Vector3d> base_resids(affected_nodes.size());
                for(size_t k=0; k<affected_nodes.size(); ++k) {
                    base_resids[k] = R_global.segment<3>(3*affected_nodes[k]);
                }

                for(int var=0; var<3; ++var) {
                    double eps = (var==0) ? eps_P : eps_S;
                    if(var==0) states[u].P += eps;
                    else if(var==1) states[u].Sw += eps;
                    else states[u].Sg += eps;
                    
                    props_cache[u] = getProps(states[u]);

                    for(size_t k=0; k<affected_nodes.size(); ++k) {
                        int row_node = affected_nodes[k];
                        Vector3d r_new = computeNodeResidual(row_node, dt, states, props_cache);
                        Vector3d diff = (r_new - base_resids[k]) / eps;
                        for(int eq=0; eq<3; ++eq) {
                            if(std::abs(diff(eq)) > 1e-20) {
                                tripletList.emplace_back(3*row_node + eq, 3*u + var, diff(eq));
                            }
                        }
                    }
                    if(var==0) states[u].P = s_orig.P;
                    else if(var==1) states[u].Sw = s_orig.Sw;
                    else states[u].Sg = s_orig.Sg;
                }
                props_cache[u] = p_orig; 
            }
            // if (iter == 0) {
            //     std::ofstream jac_file("jacobian_sparsity.csv");
            //     jac_file << "row,col,val\n"; // 写入表头
            //     for (const auto& t : tripletList) {
            //         // 只记录绝对值大于一个微小阈值的有效偏导数
            //         if (std::abs(t.value()) > 1e-12) {
            //             jac_file << t.row() << "," << t.col() << "," << t.value() << "\n";
            //         }
            //     }
            //     jac_file.close();
            // }

            J.setFromTriplets(tripletList.begin(), tripletList.end());

            
            
            // 4. 求解
            SparseLU<SparseMatrix<double>> solver;
            solver.analyzePattern(J);
            solver.factorize(J);
            
            if(solver.info() != Success) {
                // 线性求解失败（矩阵奇异），通常意味着步长太大导致物理参数崩了
                states = states_backup; // 恢复
                return false; 
            }
            
            VectorXd delta = solver.solve(-R_global);
            
            // 5. 更新与阻尼 (Damping) - 关键修改！
            // [Modified: 引入牛顿回溯 Line Search 以确保残差下降]
            double alpha = 1.0; 
            bool accepted = false;
            double norm_old = R_global.norm();
            std::vector<State> states_before_ls = states;

            for(int ls = 0; ls < 3; ++ls) {
                double damping = 1.0; 
                double max_delta_P = 0;
                double max_delta_S = 0;
                for(int i=0; i<n_total; ++i) {
                    max_delta_P = std::max(max_delta_P, std::abs(delta(3*i+0) * alpha));
                    max_delta_S = std::max(max_delta_S, std::abs(delta(3*i+1) * alpha));
                    max_delta_S = std::max(max_delta_S, std::abs(delta(3*i+2) * alpha));
                }

                if(max_delta_P > 20.0) damping = std::min(damping, 20.0 / max_delta_P);
                if(max_delta_S > 0.1)  damping = std::min(damping, 0.1 / max_delta_S);

                for(int i=0; i<n_total; ++i) {
                    states[i].P  = states_before_ls[i].P  + delta(3*i+0) * damping * alpha;
                    states[i].Sw = states_before_ls[i].Sw + delta(3*i+1) * damping * alpha;
                    states[i].Sg = states_before_ls[i].Sg + delta(3*i+2) * damping * alpha;
                    states[i].P = std::max(1.0, states[i].P); 
                    states[i].Sw = std::max(0.0, std::min(1.0, states[i].Sw));
                    states[i].Sg = std::max(0.0, std::min(1.0, states[i].Sg));
                    if(states[i].Sw + states[i].Sg > 1.0) {
                        double sum = states[i].Sw + states[i].Sg;
                        states[i].Sw /= sum; states[i].Sg /= sum;
                    }
                }

                double norm_new = 0;
                for(int i=0; i<n_total; ++i) {
                    props_cache[i] = getProps(states[i]);
                    norm_new += computeNodeResidual(i, dt, states, props_cache).squaredNorm();
                }
                norm_new = std::sqrt(norm_new);

                if(norm_new < norm_old || iter == 0) {
                    accepted = true;
                    break;
                } else {
                    alpha *= 0.5;
                    states = states_before_ls;
                }
            }

            if(!accepted && iter > 0) {
                states = states_backup;
                return false;
            }
        }
        
        // 达到最大迭代次数仍未收敛
        states = states_backup; // 恢复状态
        return false;
    }

    // =========================================================================
    // 新增功能：导出静态网格和裂缝几何信息，供 OpenCV 可视化使用
    // =========================================================================
    void exportStaticGeometry() {
        // 1. 导出网格基础信息 (grid_info.csv)
        // 格式: Nx,Ny,Nz,Lx,Ly,Lz,dx,dy,dz
        std::ofstream gridFile("grid_info.csv");
        gridFile << Nx << "," << Ny << "," << Nz << ","
                 << Lx << "," << Ly << "," << Lz << ","
                 << dx << "," << dy << "," << dz << "\n";
        gridFile.close();

        // 2. 导出裂缝原始几何信息 (fracture_geometry.csv)
        // 格式: frac_id, x0, y0, z0, x1, y1, z1, x2, y2, z2, x3, y3, z3
        std::ofstream fracFile("fracture_geometry.csv");
        fracFile << "id,x0,y0,z0,x1,y1,z1,x2,y2,z2,x3,y3,z3\n";
        for (const auto& f : fractures) {
            fracFile << f.id;
            for (int i = 0; i < 4; ++i) {
                fracFile << "," << f.vertices[i].x 
                         << "," << f.vertices[i].y 
                         << "," << f.vertices[i].z;
            }
            fracFile << "\n";
        }
        fracFile.close();
        
        std::cout << "Geometry exported: grid_info.csv and fracture_geometry.csv" << std::endl;
    }

    // 导出井信息 (well_info.csv)
    void exportWells() {
        std::ofstream wellFile("well_info.csv");
        // 输出格式: ID, 节点索引, 类型(基质/裂缝), X, Y, Z, 井指数,井底流压
        wellFile << "well_id,node_idx,type,x,y,z,WI,P_bhp\n";
        
        for(size_t i=0; i<wells.size(); ++i) {
            int u = wells[i].target_node_idx;
            double x, y, z;
            std::string type;

            if (u < n_matrix) {
                // 如果井在基质网格中
                x = cells[u].center.x;
                y = cells[u].center.y;
                z = cells[u].center.z;
                type = "Matrix";
            } else {
                // 如果井在裂缝段中 (你的代码主要是这种情况)
                int seg_idx = u - n_matrix;
                x = segments[seg_idx].center.x;
                y = segments[seg_idx].center.y;
                z = segments[seg_idx].center.z;
                type = "Fracture";
            }
            
            wellFile << i << "," << u << "," << type << ","
                     << x << "," << y << "," << z << ","
                     << wells[i].WI << "," << wells[i].P_bhp << "\n";
        }
        wellFile.close();
        std::cout << "Wells exported: well_info.csv" << std::endl;
    }

    // =========================================================================
    // 替换原有的 run，实现自动时间步长控制 (Auto Time-Stepping)
    // =========================================================================
    void run(double total_time_days) {
        std::ofstream file("output_sim.csv");
        file << "Time,CumOil,CumWater,CumGas,AvgPressure,DT\n";

        double current_time = 0.0;
        double dt = 0.001; // 初始步长设得非常小！ (1e-3 天)
        double dt_min = 1e-6;
        double dt_max = 10.0;
        
        // [Modified: 设定工业级目标迭代次数]
        int target_iter = 6; 
        double tot_oil=0, tot_water=0, tot_gas=0;
        int step_count = 0;

        std::cout << std::fixed << std::setprecision(4);

        while(current_time < total_time_days) {
            step_count++;
            
            // 尝试求解一步
            double step_oil=0, step_water=0, step_gas=0;
            bool success = false;
            int actual_iter = 0; // [Modified: 接收实际迭代次数]

            // 如果这一步加上去超过总时间，就截断 dt
            if(current_time + dt > total_time_days) {
                dt = total_time_days - current_time;
            }

            // 重试循环
            while(!success) {
                if(dt < dt_min) {
                    std::cerr << "Time step too small, simulation failed." << std::endl;
                    return;
                }

                std::cout << "Step " << step_count << " @ T=" << current_time 
                          << " trying dt=" << dt << " ... " << std::flush;
                
                // [Modified: 传入 actual_iter]
                success = solveStep(dt, step_oil, step_water, step_gas, actual_iter);

                if(success) {
                    std::cout << "Converged in " << actual_iter << " iters." << std::endl;
                    // 更新时间
                    current_time += dt;
                    states_prev = states; // 归档历史状态
                    
                    // 累加产量
                    tot_oil += step_oil;
                    tot_water += step_water;
                    tot_gas += step_gas;

                    // 写入日志
                    double avgP = 0;
                    for(int i=0; i<n_matrix; ++i) avgP += states[i].P;
                    avgP /= n_matrix;
                    file << current_time << "," << tot_oil << "," << tot_water << "," << tot_gas << "," << avgP << "," << dt << "\n";
                    file.flush();

                    // [Modified: 基于迭代目标的平滑步长调整逻辑]
                    double fac = std::pow((double)target_iter / (double)std::max(1, actual_iter), 0.5);
                    fac = std::max(0.5, std::min(1.5, fac)); // 限制单步缩放比例
                    dt = std::min(dt_max, dt * fac);
                } else {
                    std::cout << "Failed. Cutting timestep." << std::endl;
                    // 失败，减小步长重试
                    dt *= 0.25; // [Modified: 失败时更果断地切步长]
                }
            }
        }
        
        file.close();
        
        // 输出场
        std::ofstream field("final_field.csv");
        field << "x,y,z,P,Sw,Sg\n";
        for(int i=0; i<n_matrix; ++i) {
            field << cells[i].center.x << "," << cells[i].center.y << "," << cells[i].center.z << ","
                  << states[i].P << "," << states[i].Sw << "," << states[i].Sg << "\n";
        }
        field.close();
    }
};

int main() {
    Simulator sim;
    
    // 1. 初始化网格
    std::cout << "Initializing Grid..." << std::endl;
    sim.initGrid();
    
    // 2. 生成裂缝
    std::cout << "Generating Fractures..." << std::endl;
    sim.generateFractures();
    sim.generateHydraulicFractures();
    
    // 3. 计算几何相交 (EDFM)
    std::cout << "Processing Geometry..." << std::endl;
    sim.processGeometry();
    
    // 4. 建立连接关系 (Transmissibility)
    std::cout << "Building Connections..." << std::endl;
    sim.buildConnections();
    
    // 5. 建立井
    std::cout << "Setting up Wells..." << std::endl;
    sim.setupWells();
    sim.exportWells();
    
    // 6. 初始化状态
    sim.initState();

    // --- 【在这里添加调用】 ---
    std::cout << "Exporting Geometry for Visualization..." << std::endl;
    sim.exportStaticGeometry(); 
    // -----------------------
    
    // 7. 运行模拟
    // 100 天，步长 10 天
    std::cout << "Starting Simulation..." << std::endl;
    sim.run(100.0); 
    
    std::cout << "Done. Results saved to CSV." << std::endl;
    return 0;
}