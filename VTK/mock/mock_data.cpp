#include "mock_data.h"
#include <algorithm> // std::clamp
#include <cmath>     // std::max

namespace MockData {

static double Clamp01(double v) {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

RawDataset Generate(const GenerateParams& p) {
    RawDataset out{};

    // -------- grid --------
    out.grid.Nx = p.Nx;
    out.grid.Ny = p.Ny;
    out.grid.Nz = p.Nz;

    out.grid.Lx = p.Lx;
    out.grid.Ly = p.Ly;
    out.grid.Lz = p.Lz;

    // 避免除 0
    out.grid.dx = (p.Nx > 0) ? (p.Lx / static_cast<double>(p.Nx)) : 1.0;
    out.grid.dy = (p.Ny > 0) ? (p.Ly / static_cast<double>(p.Ny)) : 1.0;
    out.grid.dz = (p.Nz > 0) ? (p.Lz / static_cast<double>(p.Nz)) : 1.0;

    const int Nx = out.grid.Nx, Ny = out.grid.Ny, Nz = out.grid.Nz;
    const double dx = out.grid.dx, dy = out.grid.dy, dz = out.grid.dz;

    // -------- cells --------
    out.cells.reserve(static_cast<std::size_t>(Nx) * static_cast<std::size_t>(Ny) * static_cast<std::size_t>(Nz));

    int id_counter = 0;
    for (int k = 0; k < Nz; ++k) {
        for (int j = 0; j < Ny; ++j) {
            for (int i = 0; i < Nx; ++i) {
                CellData c{};
                c.id = id_counter++;

                // 规则格心（最稳，后面落格子不会乱）
                c.center = Point3{
                    (static_cast<double>(i) + 0.5) * dx,
                    (static_cast<double>(j) + 0.5) * dy,
                    (static_cast<double>(k) + 0.5) * dz
                };

                // 简单压力梯度（保证 p_min/p_max 不一样）
                c.pressure = p.p0
                           + p.gx * static_cast<double>(i)
                           + p.gy * static_cast<double>(j)
                           + p.gz * static_cast<double>(k);

                // 饱和度随便给个自洽：Sw+Sg<=1（这里让它正好=1）
                c.Sw = Clamp01(p.Sw0);
                c.Sg = Clamp01(1.0 - c.Sw);

                out.cells.push_back(c);
            }
        }
    }

    // -------- fractures --------
    if (p.enable_fracture) {
        FractureData f{};
        f.id = 1;
        f.aperture = p.fracture_aperture;
        f.perm = p.fracture_perm;

        // 在中间 z 层放一个矩形（4 点），保证能看见
        const double z = (static_cast<double>(Nz) * 0.5) * dz;
        const double x0 = 0.30 * out.grid.Lx, x1 = 0.70 * out.grid.Lx;
        const double y0 = 0.45 * out.grid.Ly, y1 = 0.55 * out.grid.Ly;

        f.vertices = {
            {x0, y0, z},
            {x1, y0, z},
            {x1, y1, z},
            {x0, y1, z}
        };

        out.fractures.push_back(f);
    }

    // -------- wells --------
    if (p.enable_well) {
        WellData w{};
        w.id = 1;

        // 放在中心
        w.position = Point3{0.5 * out.grid.Lx, 0.5 * out.grid.Ly, 0.5 * out.grid.Lz};
        w.bhp = p.well_bhp;
        w.rate = p.well_rate;

        out.wells.push_back(w);
    }

    return out;
}

} // namespace MockData
