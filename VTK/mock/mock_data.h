#pragma once
#include <vector>
#include <cstddef>   // size_t

namespace MockData {

// ----------------------
// 基础数据结构（与你贴的一致）
// ----------------------
struct Point3 {
    double x, y, z;
};

struct CellData {
    int id;
    Point3 center;
    double pressure;
    double Sw;
    double Sg;
};

struct FractureData {
    int id;
    std::vector<Point3> vertices;
    double aperture;
    double perm;
};

struct WellData {
    int id;
    Point3 position;
    double bhp;
    double rate;
};

struct GridData {
    int Nx, Ny, Nz;
    double Lx, Ly, Lz;
    double dx, dy, dz;
};

// ----------------------
// 一次性打包输出：最方便传给渲染入口
// ----------------------
struct RawDataset {
    GridData grid;
    std::vector<CellData> cells;
    std::vector<FractureData> fractures;
    std::vector<WellData> wells;
};

// ----------------------
// 生成参数（可选：不传就用默认）
// ----------------------
struct GenerateParams {
    int Nx = 10, Ny = 10, Nz = 5;   // 网格维度
    double Lx = 100.0, Ly = 100.0, Lz = 50.0; // 物理尺寸（会自动算 dx/dy/dz）

    // 压力场：简单线性梯度，保证有颜色变化
    double p0 = 1000.0;     // 基础压力
    double gx = 5.0;        // 沿 x 的梯度系数
    double gy = 3.0;        // 沿 y 的梯度系数
    double gz = 20.0;       // 沿 z 的梯度系数

    // 饱和度（随便给，渲染压力用不到也没关系）
    double Sw0 = 0.6;
    double Sg0 = 0.4;

    // fracture（给一个矩形，放在中间某个 z 层）
    bool enable_fracture = true;
    double fracture_aperture = 1e-3;
    double fracture_perm = 1e-12;

    // well（给一口井，放在中心）
    bool enable_well = true;
    double well_bhp = 200.0;
    double well_rate = 100.0;
};

// ----------------------
// 生成一份“肯定自洽”的 mock 数据
// - center 使用规则格心：(i+0.5)*dx
// - cells 顺序默认 (k, j, i) 展平
// ----------------------
RawDataset Generate(const GenerateParams& params = GenerateParams{});

// 一些小工具：计算 index（如果你后面要 debug）
inline std::size_t FlattenIndex(int i, int j, int k, int Nx, int Ny) {
    return static_cast<std::size_t>(k) * static_cast<std::size_t>(Nx) * static_cast<std::size_t>(Ny)
         + static_cast<std::size_t>(j) * static_cast<std::size_t>(Nx)
         + static_cast<std::size_t>(i);
}

} // namespace MockData
