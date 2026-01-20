// =============================================================================
// File: edfm_3d_water_oil_lgr.cpp
// Desc: 3D (W/O/G) fully-implicit EDFM + Static one-level LGR around fractures
// Build: g++ -O2 -std=c++17 edfm_3d_water_oil_lgr.cpp -o edfm_lgr -I /path/to/eigen
// =============================================================================
//
// "First runnable" LGR implementation
// -----------------------------------
// - Parent structured grid is built first; then parents are marked refined if:
//     (a) fracture intersects parent cell (clip area > eps_area), OR
//     (b) volume-averaged distance d_bar < d_threshold.
// - Each refined parent is uniformly subdivided into Nrx*Nry*Nrz leaf cells.
// - Only leaf cells participate in the matrix unknown vector.
// - EDFM fracture segments are generated at leaf scale (clip quad ∩ leaf AABB).
// - Matrix-Matrix (MM) connections are rebuilt, including coarse-fine interfaces.
// - Matrix-Fracture (MF) and Fracture-Fracture (FF) connections are built with
//   leaf bucketing + neighbor search to avoid O(m^2).
//
// Important practical defaults
// ----------------------------
// - Your original grid (50x20x5) plus default LGR (10x10x5) can explode in
//   unknown count and make SparseLU / numerical Jacobian infeasible.
// - Therefore this file sets a SAFE default grid and refinement ratio so the
//   code can run end-to-end on a laptop.
// - You can switch back to your original Nx/Ny/Nz and 10x10x5 after verifying
//   correctness, and then you will very likely need: (i) analytic Jacobian,
//   (ii) iterative linear solver + preconditioner, and (iii) stricter pruning.
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
#include <unordered_set>
#include <unordered_map>
#include <cstdint>

#include <Eigen/Sparse>
#include <Eigen/Dense>
#include <Eigen/SparseLU>

using namespace std;
using namespace Eigen;

// =============================================================================
// 1. Constants & basic geometry
// =============================================================================

static constexpr double EPS = 1e-12;
static constexpr double PI  = 3.14159265358979323846;

struct Point3 {
    double x{}, y{}, z{};
    Point3 operator+(const Point3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Point3 operator-(const Point3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Point3 operator*(double s) const { return {x*s, y*s, z*s}; }
    double dot(const Point3& o) const { return x*o.x + y*o.y + z*o.z; }
    Point3 cross(const Point3& o) const {
        return {y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x};
    }
    double norm() const { return std::sqrt(x*x + y*y + z*z); }
};

static inline double clamp01(double v) { return std::max(0.0, std::min(1.0, v)); }
static inline double clampd(double v, double a, double b) { return std::max(a, std::min(b, v)); }

struct AABB {
    Point3 mn, mx;
};

static inline AABB makeAABBFromCenterSize(const Point3& c, double dx, double dy, double dz) {
    return {{c.x - dx*0.5, c.y - dy*0.5, c.z - dz*0.5}, {c.x + dx*0.5, c.y + dy*0.5, c.z + dz*0.5}};
}

static inline AABB expandAABB(const AABB& b, double r) {
    return {{b.mn.x - r, b.mn.y - r, b.mn.z - r}, {b.mx.x + r, b.mx.y + r, b.mx.z + r}};
}

static inline bool aabbIntersect(const AABB& a, const AABB& b) {
    return !(a.mx.x < b.mn.x || a.mn.x > b.mx.x ||
             a.mx.y < b.mn.y || a.mn.y > b.mx.y ||
             a.mx.z < b.mn.z || a.mn.z > b.mx.z);
}

static inline double distAABB_AABB(const AABB& a, const AABB& b) {
    // minimum Euclidean distance between two AABBs (0 if intersect)
    double dx = 0.0;
    if (a.mx.x < b.mn.x) dx = b.mn.x - a.mx.x;
    else if (b.mx.x < a.mn.x) dx = a.mn.x - b.mx.x;
    double dy = 0.0;
    if (a.mx.y < b.mn.y) dy = b.mn.y - a.mx.y;
    else if (b.mx.y < a.mn.y) dy = a.mn.y - b.mx.y;
    double dz = 0.0;
    if (a.mx.z < b.mn.z) dz = b.mn.z - a.mx.z;
    else if (b.mx.z < a.mn.z) dz = a.mn.z - b.mx.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

// =============================================================================
// 2. Fracture & grid entities
// =============================================================================

struct Fracture {
    int id{};
    Point3 vertices[4];
    double aperture{0.001};
    double perm{10000.0};
};

struct ParentCell {
    int parent_id{};
    int ix{}, iy{}, iz{};
    Point3 center{};
    double dx{}, dy{}, dz{};
    double vol{};
    double phi{};
    double K[3]{};
    double depth{};
    AABB box{};
    bool refined{false};
    int leaf_base{-1};
    uint16_t Nrx{1}, Nry{1}, Nrz{1};
};

struct LeafCell {
    int leaf_id{};
    int parent_id{};
    uint16_t lix{0}, liy{0}, liz{0};
    Point3 center{};
    double dx{}, dy{}, dz{};
    double vol{};
    double phi{};
    double K[3]{};
    double depth{};
    AABB box{};
};

struct Segment {
    int id{};
    int frac_id{};
    int matrix_leaf_id{}; // IMPORTANT: leaf id
    int parent_id{-1};    // optional debug
    double area{0.0};
    Point3 center{};
    Point3 normal{};
    double aperture{0.001};
    double perm{10000.0};
    double T_mf{0.0};
};

struct Connection {
    int u{}, v{}; // global node index (leaf or segment)
    double T{0.0};
    int type{0};  // 0=MM,1=MF,2=FF
};

// =============================================================================
// 3. Polygon clipping: quad ∩ AABB (Sutherland-Hodgman)
// =============================================================================

static inline bool isInsidePlane(const Point3& p, const Point3& n, double d) {
    return (n.dot(p) + d) >= -1e-10;
}

static inline Point3 intersectPlaneSeg(const Point3& p1, const Point3& p2, const Point3& n, double d) {
    double d1 = n.dot(p1) + d;
    double d2 = n.dot(p2) + d;
    double t = d1 / (d1 - d2);
    return p1 + (p2 - p1) * t;
}

static std::vector<Point3> clipPolyPlane(const std::vector<Point3>& in, const Point3& n, double d) {
    std::vector<Point3> out;
    if (in.empty()) return out;
    for (size_t i = 0; i < in.size(); ++i) {
        const Point3& cur = in[i];
        const Point3& prev = in[(i + in.size() - 1) % in.size()];
        bool curIn  = isInsidePlane(cur,  n, d);
        bool prevIn = isInsidePlane(prev, n, d);
        if (curIn) {
            if (!prevIn) out.push_back(intersectPlaneSeg(prev, cur, n, d));
            out.push_back(cur);
        } else if (prevIn) {
            out.push_back(intersectPlaneSeg(prev, cur, n, d));
        }
    }
    return out;
}

static double polygonArea(const std::vector<Point3>& poly) {
    if (poly.size() < 3) return 0.0;
    Point3 total{0,0,0};
    Point3 v0 = poly[0];
    for (size_t i = 1; i + 1 < poly.size(); ++i) {
        total = total + (poly[i] - v0).cross(poly[i+1] - v0);
    }
    return 0.5 * total.norm();
}

static Point3 polygonCenter(const std::vector<Point3>& poly) {
    Point3 c{0,0,0};
    if (poly.empty()) return c;
    for (const auto& p : poly) c = c + p;
    return c * (1.0 / (double)poly.size());
}

static std::vector<Point3> clipFractureBox(const Fracture& frac, const AABB& box) {
    std::vector<Point3> poly;
    poly.reserve(4);
    for (int i = 0; i < 4; ++i) poly.push_back(frac.vertices[i]);
    const double x_min = box.mn.x, x_max = box.mx.x;
    const double y_min = box.mn.y, y_max = box.mx.y;
    const double z_min = box.mn.z, z_max = box.mx.z;
    // 6 planes with inward normals
    poly = clipPolyPlane(poly, { 1, 0, 0}, -x_min);
    poly = clipPolyPlane(poly, {-1, 0, 0},  x_max);
    poly = clipPolyPlane(poly, { 0, 1, 0}, -y_min);
    poly = clipPolyPlane(poly, { 0,-1, 0},  y_max);
    poly = clipPolyPlane(poly, { 0, 0, 1}, -z_min);
    poly = clipPolyPlane(poly, { 0, 0,-1},  z_max);
    return poly;
}

// =============================================================================
// 4. Distance: point-to-triangle (Ericson) and quad distance
// =============================================================================

static inline double distPointSegment(const Point3& p, const Point3& a, const Point3& b) {
    Point3 ab = b - a;
    double t = (p - a).dot(ab) / std::max(EPS, ab.dot(ab));
    t = clampd(t, 0.0, 1.0);
    Point3 q = a + ab * t;
    return (p - q).norm();
}

static double distPointTriangle(const Point3& p, const Point3& a, const Point3& b, const Point3& c) {
    // Real-Time Collision Detection (Christer Ericson)
    Point3 ab = b - a;
    Point3 ac = c - a;
    Point3 ap = p - a;

    double d1 = ab.dot(ap);
    double d2 = ac.dot(ap);
    if (d1 <= 0.0 && d2 <= 0.0) return (p - a).norm();

    Point3 bp = p - b;
    double d3 = ab.dot(bp);
    double d4 = ac.dot(bp);
    if (d3 >= 0.0 && d4 <= d3) return (p - b).norm();

    double vc = d1*d4 - d3*d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
        double v = d1 / std::max(EPS, (d1 - d3));
        Point3 q = a + ab * v;
        return (p - q).norm();
    }

    Point3 cp = p - c;
    double d5 = ab.dot(cp);
    double d6 = ac.dot(cp);
    if (d6 >= 0.0 && d5 <= d6) return (p - c).norm();

    double vb = d5*d2 - d1*d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
        double w = d2 / std::max(EPS, (d2 - d6));
        Point3 q = a + ac * w;
        return (p - q).norm();
    }

    double va = d3*d6 - d5*d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
        double w = (d4 - d3) / std::max(EPS, ((d4 - d3) + (d5 - d6)));
        Point3 q = b + (c - b) * w;
        return (p - q).norm();
    }

    // inside face region
    Point3 n = ab.cross(ac);
    double nlen = std::max(EPS, n.norm());
    n = n * (1.0 / nlen);
    double dist = std::abs((p - a).dot(n));
    return dist;
}

static inline double distPointQuad(const Point3& p, const Fracture& f) {
    const Point3& v0 = f.vertices[0];
    const Point3& v1 = f.vertices[1];
    const Point3& v2 = f.vertices[2];
    const Point3& v3 = f.vertices[3];
    double d1 = distPointTriangle(p, v0, v1, v2);
    double d2 = distPointTriangle(p, v0, v2, v3);
    return std::min(d1, d2);
}

static double dbarCellQuad(const AABB& cell, const Fracture& f, int nsx, int nsy, int nsz) {
    // uniform sampling of cell volume
    double sx = (cell.mx.x - cell.mn.x) / nsx;
    double sy = (cell.mx.y - cell.mn.y) / nsy;
    double sz = (cell.mx.z - cell.mn.z) / nsz;
    double sum = 0.0;
    int cnt = 0;
    for (int k = 0; k < nsz; ++k) {
        for (int j = 0; j < nsy; ++j) {
            for (int i = 0; i < nsx; ++i) {
                Point3 p{cell.mn.x + (i + 0.5)*sx,
                         cell.mn.y + (j + 0.5)*sy,
                         cell.mn.z + (k + 0.5)*sz};
                sum += distPointQuad(p, f);
                cnt++;
            }
        }
    }
    return (cnt > 0) ? (sum / cnt) : 1e30;
}

// =============================================================================
// 5. Fluid props (same style as your original code)
// =============================================================================

struct FluidProps {
    double mu_w = 1.0;
    double mu_o = 5.0;
    double mu_g = 0.2;
    double cw = 1e-8;
    double co = 1e-5;
    double cg = 1e-3;
    double P_ref = 100.0;
    double Swi = 0.2;
    double Sor = 0.2;
    double Sgc = 0.05;
};

static FluidProps g_props;

static void calcPVT(double P, double& Bw, double& Bo, double& Bg) {
    double dP = P - g_props.P_ref;
    Bw = std::exp(-g_props.cw * dP);
    Bo = std::exp(-g_props.co * dP);
    Bg = std::exp(-g_props.cg * dP);
}

static void calcRelPerm(double Sw, double Sg, double& krw, double& kro, double& krg) {
    double Sw_norm = (Sw - g_props.Swi) / (1.0 - g_props.Swi - g_props.Sor);
    double Sg_norm = (Sg - g_props.Sgc) / (1.0 - g_props.Sgc - g_props.Swi - g_props.Sor);
    Sw_norm = clamp01(Sw_norm);
    Sg_norm = clamp01(Sg_norm);
    krw = std::pow(Sw_norm, 2.0);
    krg = std::pow(Sg_norm, 2.0);
    double So_norm = clamp01(1.0 - Sw_norm - Sg_norm);
    kro = std::pow(So_norm, 2.0);
}

struct State {
    double P{200.0};
    double Sw{0.2};
    double Sg{0.05};
};

// =============================================================================
// 6. Simulator with LGR
// =============================================================================

class SimulatorLGR {
public:
    // Domain
    int Nx{12}, Ny{6}, Nz{2};           // SAFE default for "runnable" LGR
    double Lx{1000}, Ly{500}, Lz{50};
    double dx{}, dy{}, dz{};

    // LGR config
    bool enable_lgr{true};
    double d_threshold{30.0};
    uint16_t lgr_Nrx{4}, lgr_Nry{4}, lgr_Nrz{2}; // SAFE default; switch to 10/10/5 later
    int dbar_nsx{4}, dbar_nsy{4}, dbar_nsz{4};
    double eps_area_factor{1e-8}; // eps_area = eps_area_factor * min_face_area
    double d_avg_factor{0.5};     // d_avg = d_avg_factor * min(dx,dy,dz)

    // Geometry storage
    std::vector<ParentCell> parents;
    std::vector<LeafCell> leaves;
    std::vector<Fracture> fractures;
    std::vector<Segment> segments;

    // Maps / helpers
    std::vector<std::vector<int>> parent_face_leaves[6]; // [face][pid] -> list of leaf ids
    std::vector<std::vector<int>> leaf_neighbors;        // MM neighbors
    std::vector<std::vector<int>> leaf_to_segs;          // bucket

    // Graph
    std::vector<Connection> connections;
    struct Neighbor { int v; double T; int type; };
    std::vector<std::vector<Neighbor>> adj; // for residual

    // unknown counts
    int n_leaf{0};
    int n_seg{0};
    int n_total{0};

    // states
    std::vector<State> states, states_prev;

    // Well
    struct Well { int target_node_idx; double WI; double P_bhp; };
    std::vector<Well> wells;
    std::unordered_map<int,int> well_map;

    SimulatorLGR() {
        dx = Lx / Nx; dy = Ly / Ny; dz = Lz / Nz;
    }

    // ---------------------------------------------------------------------
    // 6.1 Build parent grid
    // ---------------------------------------------------------------------
    void buildParentGrid() {
        dx = Lx / Nx; dy = Ly / Ny; dz = Lz / Nz;
        int n_parent = Nx*Ny*Nz;
        parents.resize(n_parent);
        for (int k = 0; k < Nz; ++k) {
            for (int j = 0; j < Ny; ++j) {
                for (int i = 0; i < Nx; ++i) {
                    int pid = k*Nx*Ny + j*Nx + i;
                    ParentCell pc;
                    pc.parent_id = pid;
                    pc.ix=i; pc.iy=j; pc.iz=k;
                    pc.dx=dx; pc.dy=dy; pc.dz=dz;
                    pc.center = {(i+0.5)*dx, (j+0.5)*dy, (k+0.5)*dz};
                    pc.vol = dx*dy*dz;
                    pc.phi = 0.2;
                    pc.K[0]=0.1; pc.K[1]=0.1; pc.K[2]=0.01;
                    pc.depth = pc.center.z;
                    pc.box = makeAABBFromCenterSize(pc.center, dx, dy, dz);
                    pc.refined = false;
                    pc.Nrx = lgr_Nrx; pc.Nry = lgr_Nry; pc.Nrz = lgr_Nrz;
                    parents[pid] = pc;
                }
            }
        }
    }

    // ---------------------------------------------------------------------
    // 6.2 Generate fractures (same as your original implementation)
    // ---------------------------------------------------------------------
    void generateFractures() {
        fractures.clear();
        std::mt19937 rng(42);
        std::uniform_real_distribution<double> distX(0, Lx);
        std::uniform_real_distribution<double> distY(0, Ly);
        std::uniform_real_distribution<double> distZ(0, Lz);
        std::uniform_real_distribution<double> distAngle(0, PI);
        std::uniform_real_distribution<double> distDip(0, PI/3.0);
        std::uniform_real_distribution<double> distL(30, 80);

        for(int i=0; i<80; ++i) { // SAFE default: fewer natural fractures
            Fracture f; f.id = i; f.aperture = 0.001; f.perm = 10000.0;
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
            fractures.push_back(f);
        }

        double xc=Lx/2.0, yc=Ly/2.0, zc=Lz/2.0;
        double offsets[3] = {-100.0, 0.0, 100.0};
        double hf_len=200.0, hf_height=100.0;
        for(int k=0; k<3; ++k) {
            Fracture f; f.id = 100 + k; f.aperture = 0.01; f.perm = 100000.0;
            double x_curr = xc + offsets[k];
            f.vertices[0] = {x_curr, yc - hf_len/2, zc - hf_height/2};
            f.vertices[1] = {x_curr, yc + hf_len/2, zc - hf_height/2};
            f.vertices[2] = {x_curr, yc + hf_len/2, zc + hf_height/2};
            f.vertices[3] = {x_curr, yc - hf_len/2, zc + hf_height/2};
            fractures.push_back(f);
        }
    }

    // ---------------------------------------------------------------------
    // 6.3 Mark refinement
    // ---------------------------------------------------------------------
    static AABB fractureAABB(const Fracture& f) {
        AABB b;
        b.mn = { 1e30,  1e30,  1e30};
        b.mx = {-1e30, -1e30, -1e30};
        for (int i=0;i<4;++i) {
            b.mn.x = std::min(b.mn.x, f.vertices[i].x);
            b.mn.y = std::min(b.mn.y, f.vertices[i].y);
            b.mn.z = std::min(b.mn.z, f.vertices[i].z);
            b.mx.x = std::max(b.mx.x, f.vertices[i].x);
            b.mx.y = std::max(b.mx.y, f.vertices[i].y);
            b.mx.z = std::max(b.mx.z, f.vertices[i].z);
        }
        return b;
    }

    double epsAreaForBox(double dx_, double dy_, double dz_) const {
        double a1 = dx_*dy_, a2 = dx_*dz_, a3 = dy_*dz_;
        double minA = std::min(a1, std::min(a2, a3));
        return eps_area_factor * minA;
    }

    void markRefinement() {
        if (!enable_lgr) {
            for (auto& p : parents) p.refined = false;
            return;
        }
        // reset
        for (auto& p : parents) p.refined = false;

        // For each fracture, scan candidate parent range based on expanded AABB
        for (const auto& f : fractures) {
            AABB fb = fractureAABB(f);
            AABB fb_exp = expandAABB(fb, d_threshold);

            int i0 = std::max(0, (int)std::floor(fb_exp.mn.x / dx));
            int i1 = std::min(Nx-1, (int)std::floor(fb_exp.mx.x / dx));
            int j0 = std::max(0, (int)std::floor(fb_exp.mn.y / dy));
            int j1 = std::min(Ny-1, (int)std::floor(fb_exp.mx.y / dy));
            int k0 = std::max(0, (int)std::floor(fb_exp.mn.z / dz));
            int k1 = std::min(Nz-1, (int)std::floor(fb_exp.mx.z / dz));

            for (int k=k0; k<=k1; ++k) {
                for (int j=j0; j<=j1; ++j) {
                    for (int i=i0; i<=i1; ++i) {
                        int pid = k*Nx*Ny + j*Nx + i;
                        ParentCell& pc = parents[pid];
                        if (pc.refined) continue; // already refined by other fracture

                        // L0.5: AABB distance pruning
                        if (distAABB_AABB(pc.box, fb) > d_threshold) continue;

                        // L1: intersection via clipping
                        double epsA = epsAreaForBox(pc.dx, pc.dy, pc.dz);
                        auto poly = clipFractureBox(f, pc.box);
                        double area = polygonArea(poly);
                        if (area > epsA) {
                            pc.refined = true;
                            continue;
                        }

                        // L2: d_bar sampling
                        double dbar = dbarCellQuad(pc.box, f, dbar_nsx, dbar_nsy, dbar_nsz);
                        if (dbar < d_threshold) {
                            pc.refined = true;
                        }
                    }
                }
            }
        }

        int cnt = 0;
        for (const auto& p : parents) if (p.refined) cnt++;
        std::cout << "Refinement marked: refined parents = " << cnt << " / " << (int)parents.size() << std::endl;
    }

    // ---------------------------------------------------------------------
    // 6.4 Build leaf grid + mapping
    // ---------------------------------------------------------------------
    void buildLeafGrid() {
        // leaf count
        n_leaf = 0;
        for (auto& p : parents) {
            p.leaf_base = n_leaf;
            if (p.refined) n_leaf += (int)p.Nrx * (int)p.Nry * (int)p.Nrz;
            else n_leaf += 1;
        }
        leaves.clear();
        leaves.resize(n_leaf);

        for (const auto& p : parents) {
            if (!p.refined) {
                int lid = p.leaf_base;
                LeafCell lc;
                lc.leaf_id = lid;
                lc.parent_id = p.parent_id;
                lc.lix = lc.liy = lc.liz = 0;
                lc.dx = p.dx; lc.dy = p.dy; lc.dz = p.dz;
                lc.center = p.center;
                lc.vol = p.vol;
                lc.phi = p.phi;
                lc.K[0]=p.K[0]; lc.K[1]=p.K[1]; lc.K[2]=p.K[2];
                lc.depth = p.depth;
                lc.box = p.box;
                leaves[lid] = lc;
            } else {
                double dxs = p.dx / p.Nrx;
                double dys = p.dy / p.Nry;
                double dzs = p.dz / p.Nrz;
                for (uint16_t lk=0; lk<p.Nrz; ++lk) {
                    for (uint16_t lj=0; lj<p.Nry; ++lj) {
                        for (uint16_t li=0; li<p.Nrx; ++li) {
                            int lid = p.leaf_base + ((int)lk*(int)p.Nry + (int)lj)*(int)p.Nrx + (int)li;
                            Point3 c{
                                p.box.mn.x + (li + 0.5)*dxs,
                                p.box.mn.y + (lj + 0.5)*dys,
                                p.box.mn.z + (lk + 0.5)*dzs
                            };
                            LeafCell lc;
                            lc.leaf_id = lid;
                            lc.parent_id = p.parent_id;
                            lc.lix = li; lc.liy = lj; lc.liz = lk;
                            lc.dx = dxs; lc.dy = dys; lc.dz = dzs;
                            lc.center = c;
                            lc.vol = dxs*dys*dzs;
                            lc.phi = p.phi;
                            lc.K[0]=p.K[0]; lc.K[1]=p.K[1]; lc.K[2]=p.K[2];
                            lc.depth = c.z;
                            lc.box = makeAABBFromCenterSize(c, dxs, dys, dzs);
                            leaves[lid] = lc;
                        }
                    }
                }
            }
        }

        std::cout << "Leaf grid built: n_leaf = " << n_leaf << std::endl;
    }

    // ---------------------------------------------------------------------
    // 6.5 Build parent face leaf cover (nonconforming interface table)
    // ---------------------------------------------------------------------
    void buildParentFaceLeaves() {
        int n_parent = (int)parents.size();
        for (int f=0; f<6; ++f) {
            parent_face_leaves[f].clear();
            parent_face_leaves[f].resize(n_parent);
        }
        for (const auto& p : parents) {
            int pid = p.parent_id;
            if (!p.refined) {
                for (int f=0; f<6; ++f) parent_face_leaves[f][pid] = {p.leaf_base};
                continue;
            }
            // ordering: lk outer, lj inner (consistent across neighbors)
            // -X (li=0) / +X (li=Nrx-1)
            {
                std::vector<int> negx, posx;
                negx.reserve((int)p.Nry*(int)p.Nrz);
                posx.reserve((int)p.Nry*(int)p.Nrz);
                for (uint16_t lk=0; lk<p.Nrz; ++lk) {
                    for (uint16_t lj=0; lj<p.Nry; ++lj) {
                        int lid0 = p.leaf_base + ((int)lk*(int)p.Nry + (int)lj)*(int)p.Nrx + 0;
                        int lid1 = p.leaf_base + ((int)lk*(int)p.Nry + (int)lj)*(int)p.Nrx + ((int)p.Nrx - 1);
                        negx.push_back(lid0);
                        posx.push_back(lid1);
                    }
                }
                parent_face_leaves[0][pid] = std::move(negx);
                parent_face_leaves[1][pid] = std::move(posx);
            }
            // -Y / +Y
            {
                std::vector<int> negy, posy;
                negy.reserve((int)p.Nrx*(int)p.Nrz);
                posy.reserve((int)p.Nrx*(int)p.Nrz);
                for (uint16_t lk=0; lk<p.Nrz; ++lk) {
                    for (uint16_t li=0; li<p.Nrx; ++li) {
                        int lid0 = p.leaf_base + ((int)lk*(int)p.Nry + 0)*(int)p.Nrx + (int)li;
                        int lid1 = p.leaf_base + ((int)lk*(int)p.Nry + ((int)p.Nry - 1))*(int)p.Nrx + (int)li;
                        negy.push_back(lid0);
                        posy.push_back(lid1);
                    }
                }
                parent_face_leaves[2][pid] = std::move(negy);
                parent_face_leaves[3][pid] = std::move(posy);
            }
            // -Z / +Z
            {
                std::vector<int> negz, posz;
                negz.reserve((int)p.Nrx*(int)p.Nry);
                posz.reserve((int)p.Nrx*(int)p.Nry);
                for (uint16_t lj=0; lj<p.Nry; ++lj) {
                    for (uint16_t li=0; li<p.Nrx; ++li) {
                        int lid0 = p.leaf_base + (0*(int)p.Nry + (int)lj)*(int)p.Nrx + (int)li;
                        int lid1 = p.leaf_base + (((int)p.Nrz - 1)*(int)p.Nry + (int)lj)*(int)p.Nrx + (int)li;
                        negz.push_back(lid0);
                        posz.push_back(lid1);
                    }
                }
                parent_face_leaves[4][pid] = std::move(negz);
                parent_face_leaves[5][pid] = std::move(posz);
            }
        }
    }

    // ---------------------------------------------------------------------
    // 6.6 Build MM connections (leaf graph only) + leaf_neighbors
    // ---------------------------------------------------------------------
    static inline double halfResT(double area, double dL, double kL, double dR, double kR) {
        // T = A / (dL/kL + dR/kR)
        return area / (dL / std::max(EPS, kL) + dR / std::max(EPS, kR));
    }

    void buildMMConnections(std::vector<Connection>& mm_out) {
        mm_out.clear();
        mm_out.reserve(n_leaf * 4);
        leaf_neighbors.clear();
        leaf_neighbors.resize(n_leaf);

        auto add_mm = [&](int a, int b, double T) {
            if (a == b) return;
            int u = std::min(a,b);
            int v = std::max(a,b);
            mm_out.push_back({u,v,T,0});
            leaf_neighbors[u].push_back(v);
            leaf_neighbors[v].push_back(u);
        };

        // (A) intra refined parent: subgrid 6-neigh
        for (const auto& p : parents) {
            if (!p.refined) continue;
            int base = p.leaf_base;
            int Nrx = p.Nrx, Nry=p.Nry, Nrz=p.Nrz;
            for (int lk=0; lk<Nrz; ++lk) {
                for (int lj=0; lj<Nry; ++lj) {
                    for (int li=0; li<Nrx; ++li) {
                        int lid = base + (lk*Nry + lj)*Nrx + li;
                        const LeafCell& c = leaves[lid];
                        if (li + 1 < Nrx) {
                            int rid = lid + 1;
                            const LeafCell& r = leaves[rid];
                            double A = c.dy * c.dz;
                            double T = halfResT(A, c.dx*0.5, c.K[0], r.dx*0.5, r.K[0]);
                            add_mm(lid, rid, T);
                        }
                        if (lj + 1 < Nry) {
                            int uid = base + (lk*Nry + (lj+1))*Nrx + li;
                            const LeafCell& ucell = leaves[uid];
                            double A = c.dx * c.dz;
                            double T = halfResT(A, c.dy*0.5, c.K[1], ucell.dy*0.5, ucell.K[1]);
                            add_mm(lid, uid, T);
                        }
                        if (lk + 1 < Nrz) {
                            int wid = base + ((lk+1)*Nry + lj)*Nrx + li;
                            const LeafCell& w = leaves[wid];
                            double A = c.dx * c.dy;
                            double T = halfResT(A, c.dz*0.5, c.K[2], w.dz*0.5, w.K[2]);
                            add_mm(lid, wid, T);
                        }
                    }
                }
            }
        }

        // (B) cross parent faces: traverse in +x,+y,+z in parent grid
        auto parentAt = [&](int i,int j,int k)->int { return k*Nx*Ny + j*Nx + i; };
        for (int k=0; k<Nz; ++k) {
            for (int j=0; j<Ny; ++j) {
                for (int i=0; i<Nx; ++i) {
                    int A = parentAt(i,j,k);
                    const ParentCell& pA = parents[A];
                    // +X
                    if (i + 1 < Nx) {
                        int B = parentAt(i+1,j,k);
                        const ParentCell& pB = parents[B];
                        const auto& LA = parent_face_leaves[1][A]; // +X
                        const auto& LB = parent_face_leaves[0][B]; // -X
                        connectParentFaces_MM(LA, pA, LB, pB, /*dir=*/0, add_mm);
                    }
                    // +Y
                    if (j + 1 < Ny) {
                        int B = parentAt(i,j+1,k);
                        const ParentCell& pB = parents[B];
                        const auto& LA = parent_face_leaves[3][A]; // +Y
                        const auto& LB = parent_face_leaves[2][B]; // -Y
                        connectParentFaces_MM(LA, pA, LB, pB, /*dir=*/1, add_mm);
                    }
                    // +Z
                    if (k + 1 < Nz) {
                        int B = parentAt(i,j,k+1);
                        const ParentCell& pB = parents[B];
                        const auto& LA = parent_face_leaves[5][A]; // +Z
                        const auto& LB = parent_face_leaves[4][B]; // -Z
                        connectParentFaces_MM(LA, pA, LB, pB, /*dir=*/2, add_mm);
                    }
                }
            }
        }

        // dedup leaf_neighbors
        for (auto& nb : leaf_neighbors) {
            std::sort(nb.begin(), nb.end());
            nb.erase(std::unique(nb.begin(), nb.end()), nb.end());
        }
    }

    // dir: 0=x,1=y,2=z
    template<class AddMM>
    void connectParentFaces_MM(
        const std::vector<int>& LA, const ParentCell& pA,
        const std::vector<int>& LB, const ParentCell& pB,
        int dir,
        AddMM add_mm)
    {
        // coarse-coarse
        if (LA.size() == 1 && LB.size() == 1) {
            int a = LA[0], b = LB[0];
            const LeafCell& ca = leaves[a];
            const LeafCell& cb = leaves[b];
            double A = (dir==0)? (ca.dy*ca.dz) : (dir==1? (ca.dx*ca.dz) : (ca.dx*ca.dy));
            double dL = (dir==0)? (ca.dx*0.5) : (dir==1? (ca.dy*0.5) : (ca.dz*0.5));
            double dR = (dir==0)? (cb.dx*0.5) : (dir==1? (cb.dy*0.5) : (cb.dz*0.5));
            double kL = ca.K[dir];
            double kR = cb.K[dir];
            add_mm(a, b, halfResT(A, dL, kL, dR, kR));
            return;
        }

        // coarse-fine (A coarse)
        if (LA.size() == 1 && LB.size() > 1) {
            int a = LA[0];
            const LeafCell& ca = leaves[a];
            for (int b : LB) {
                const LeafCell& cb = leaves[b];
                double A = (dir==0)? (cb.dy*cb.dz) : (dir==1? (cb.dx*cb.dz) : (cb.dx*cb.dy));
                double dL = (dir==0)? (ca.dx*0.5) : (dir==1? (ca.dy*0.5) : (ca.dz*0.5));
                double dR = (dir==0)? (cb.dx*0.5) : (dir==1? (cb.dy*0.5) : (cb.dz*0.5));
                add_mm(a, b, halfResT(A, dL, ca.K[dir], dR, cb.K[dir]));
            }
            return;
        }

        // coarse-fine (B coarse)
        if (LA.size() > 1 && LB.size() == 1) {
            int b = LB[0];
            const LeafCell& cb = leaves[b];
            for (int a : LA) {
                const LeafCell& ca = leaves[a];
                double A = (dir==0)? (ca.dy*ca.dz) : (dir==1? (ca.dx*ca.dz) : (ca.dx*ca.dy));
                double dL = (dir==0)? (ca.dx*0.5) : (dir==1? (ca.dy*0.5) : (ca.dz*0.5));
                double dR = (dir==0)? (cb.dx*0.5) : (dir==1? (cb.dy*0.5) : (cb.dz*0.5));
                add_mm(a, b, halfResT(A, dL, ca.K[dir], dR, cb.K[dir]));
            }
            return;
        }

        // fine-fine: assume same subdivision & aligned, do 1-1 by list order
        if (LA.size() == LB.size()) {
            for (size_t idx=0; idx<LA.size(); ++idx) {
                int a = LA[idx], b = LB[idx];
                const LeafCell& ca = leaves[a];
                const LeafCell& cb = leaves[b];
                double A = (dir==0)? (ca.dy*ca.dz) : (dir==1? (ca.dx*ca.dz) : (ca.dx*ca.dy));
                double dL = (dir==0)? (ca.dx*0.5) : (dir==1? (ca.dy*0.5) : (ca.dz*0.5));
                double dR = (dir==0)? (cb.dx*0.5) : (dir==1? (cb.dy*0.5) : (cb.dz*0.5));
                add_mm(a, b, halfResT(A, dL, ca.K[dir], dR, cb.K[dir]));
            }
            return;
        }

        // fallback (different refinement ratio) - not supported in this first version
        // (do nothing)
    }

    // ---------------------------------------------------------------------
    // 6.7 Build segments (leaf-scale clipping) + MF bucket
    // ---------------------------------------------------------------------
    void buildSegmentsAndMF(std::vector<Connection>& mf_out) {
        segments.clear();
        mf_out.clear();
        leaf_to_segs.clear();
        leaf_to_segs.resize(n_leaf);

        int seg_id_counter = 0;
        for (const auto& frac : fractures) {
            AABB fb = fractureAABB(frac);
            // parent candidate range based on fracture AABB (no threshold needed; intersection only)
            int i0 = std::max(0, (int)std::floor(fb.mn.x / dx));
            int i1 = std::min(Nx-1, (int)std::floor(fb.mx.x / dx));
            int j0 = std::max(0, (int)std::floor(fb.mn.y / dy));
            int j1 = std::min(Ny-1, (int)std::floor(fb.mx.y / dy));
            int k0 = std::max(0, (int)std::floor(fb.mn.z / dz));
            int k1 = std::min(Nz-1, (int)std::floor(fb.mx.z / dz));

            // normal
            Point3 vec1 = frac.vertices[1] - frac.vertices[0];
            Point3 vec2 = frac.vertices[3] - frac.vertices[0];
            Point3 normal = vec1.cross(vec2);
            double nlen = std::max(EPS, normal.norm());
            normal = normal * (1.0 / nlen);

            for (int k=k0; k<=k1; ++k) {
                for (int j=j0; j<=j1; ++j) {
                    for (int i=i0; i<=i1; ++i) {
                        int pid = k*Nx*Ny + j*Nx + i;
                        const ParentCell& p = parents[pid];

                        if (!p.refined) {
                            double epsA = epsAreaForBox(p.dx, p.dy, p.dz);
                            auto poly = clipFractureBox(frac, p.box);
                            double area = polygonArea(poly);
                            if (area > epsA) {
                                Segment seg;
                                seg.id = seg_id_counter++;
                                seg.frac_id = frac.id;
                                seg.matrix_leaf_id = p.leaf_base;
                                seg.parent_id = pid;
                                seg.area = area;
                                seg.center = polygonCenter(poly);
                                seg.normal = normal;
                                seg.aperture = frac.aperture;
                                seg.perm = frac.perm;
                                // Tmf
                                const LeafCell& lc = leaves[seg.matrix_leaf_id];
                                double minh = std::min(lc.dx, std::min(lc.dy, lc.dz));
                                double d_avg = d_avg_factor * minh;
                                double Kn = seg.normal.x*seg.normal.x*lc.K[0] +
                                            seg.normal.y*seg.normal.y*lc.K[1] +
                                            seg.normal.z*seg.normal.z*lc.K[2];
                                seg.T_mf = area * (Kn / std::max(EPS, d_avg));
                                segments.push_back(seg);
                                leaf_to_segs[seg.matrix_leaf_id].push_back(seg.id);
                            }
                        } else {
                            // refined: visit only subcells overlapping fracture AABB
                            int Nrx=p.Nrx, Nry=p.Nry, Nrz=p.Nrz;
                            double dxs = p.dx/Nrx, dys=p.dy/Nry, dzs=p.dz/Nrz;

                            int li0 = (int)std::floor((fb.mn.x - p.box.mn.x) / dxs);
                            int li1 = (int)std::floor((fb.mx.x - p.box.mn.x) / dxs);
                            int lj0 = (int)std::floor((fb.mn.y - p.box.mn.y) / dys);
                            int lj1 = (int)std::floor((fb.mx.y - p.box.mn.y) / dys);
                            int lk0 = (int)std::floor((fb.mn.z - p.box.mn.z) / dzs);
                            int lk1 = (int)std::floor((fb.mx.z - p.box.mn.z) / dzs);
                            li0 = std::max(0, std::min(Nrx-1, li0));
                            li1 = std::max(0, std::min(Nrx-1, li1));
                            lj0 = std::max(0, std::min(Nry-1, lj0));
                            lj1 = std::max(0, std::min(Nry-1, lj1));
                            lk0 = std::max(0, std::min(Nrz-1, lk0));
                            lk1 = std::max(0, std::min(Nrz-1, lk1));

                            for (int lk=lk0; lk<=lk1; ++lk) {
                                for (int lj=lj0; lj<=lj1; ++lj) {
                                    for (int li=li0; li<=li1; ++li) {
                                        int lid = p.leaf_base + (lk*Nry + lj)*Nrx + li;
                                        const LeafCell& lc = leaves[lid];
                                        double epsA = epsAreaForBox(lc.dx, lc.dy, lc.dz);
                                        auto poly = clipFractureBox(frac, lc.box);
                                        double area = polygonArea(poly);
                                        if (area > epsA) {
                                            Segment seg;
                                            seg.id = seg_id_counter++;
                                            seg.frac_id = frac.id;
                                            seg.matrix_leaf_id = lid;
                                            seg.parent_id = pid;
                                            seg.area = area;
                                            seg.center = polygonCenter(poly);
                                            seg.normal = normal;
                                            seg.aperture = frac.aperture;
                                            seg.perm = frac.perm;
                                            double minh = std::min(lc.dx, std::min(lc.dy, lc.dz));
                                            double d_avg = d_avg_factor * minh;
                                            double Kn = seg.normal.x*seg.normal.x*lc.K[0] +
                                                        seg.normal.y*seg.normal.y*lc.K[1] +
                                                        seg.normal.z*seg.normal.z*lc.K[2];
                                            seg.T_mf = area * (Kn / std::max(EPS, d_avg));
                                            segments.push_back(seg);
                                            leaf_to_segs[seg.matrix_leaf_id].push_back(seg.id);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        n_seg = (int)segments.size();
        std::cout << "Segments built: n_seg = " << n_seg << std::endl;

        // MF edges: leaf -> segnode
        mf_out.reserve(n_seg);
        for (int s=0; s<n_seg; ++s) {
            int u = segments[s].matrix_leaf_id;
            int v = n_leaf + s;
            mf_out.push_back({std::min(u,v), std::max(u,v), segments[s].T_mf, 1});
        }
    }

    // ---------------------------------------------------------------------
    // 6.8 Build FF connections (bucket + neighbor search)
    // ---------------------------------------------------------------------
    void buildFFConnections(std::vector<Connection>& ff_out) {
        ff_out.clear();
        // (A) intra-frac near neighbors within leaf neighborhood
        // distance threshold scaled with local cell size
        for (int s=0; s<n_seg; ++s) {
            const Segment& seg = segments[s];
            int leaf = seg.matrix_leaf_id;

            // candidate leaves: self + MM neighbors (1-hop)
            std::vector<int> cand_leaves;
            cand_leaves.reserve(1 + leaf_neighbors[leaf].size());
            cand_leaves.push_back(leaf);
            for (int nb : leaf_neighbors[leaf]) cand_leaves.push_back(nb);

            // build candidate segments list
            for (int cl : cand_leaves) {
                for (int t : leaf_to_segs[cl]) {
                    if (t <= s) continue; // avoid duplicates
                    if (segments[t].frac_id != seg.frac_id) continue;
                    double dist = (seg.center - segments[t].center).norm();
                    const LeafCell& lc = leaves[leaf];
                    double h = std::sqrt(lc.dx*lc.dx + lc.dy*lc.dy + lc.dz*lc.dz);
                    double r = 2.5 * h;
                    if (dist < std::max(1e-6, r)) {
                        double width = std::sqrt(std::max(EPS, std::min(seg.area, segments[t].area)));
                        double T = seg.perm * seg.aperture * width / std::max(1e-6, dist);
                        int u = n_leaf + s;
                        int v = n_leaf + t;
                        ff_out.push_back({std::min(u,v), std::max(u,v), T, 2});
                    }
                }
            }
        }

        // (B) inter-frac: within same leaf, connect different frac groups
        const double T_inter_const = 1000.0;
        for (int leaf=0; leaf<n_leaf; ++leaf) {
            const auto& segs = leaf_to_segs[leaf];
            if (segs.size() < 2) continue;
            for (size_t i=0; i<segs.size(); ++i) {
                for (size_t j=i+1; j<segs.size(); ++j) {
                    int s1 = segs[i], s2 = segs[j];
                    if (segments[s1].frac_id == segments[s2].frac_id) continue;
                    int u = n_leaf + s1;
                    int v = n_leaf + s2;
                    ff_out.push_back({std::min(u,v), std::max(u,v), T_inter_const, 2});
                }
            }
        }

        std::cout << "FF edges built (raw): " << ff_out.size() << std::endl;
    }

    // ---------------------------------------------------------------------
    // 6.9 Merge edges, dedup, build adjacency
    // ---------------------------------------------------------------------
    struct ConnKey {
        int type;
        int u;
        int v;
        bool operator==(const ConnKey& o) const { return type==o.type && u==o.u && v==o.v; }
    };
    struct ConnKeyHash {
        size_t operator()(const ConnKey& k) const {
            // 64-bit mix
            uint64_t x = (uint64_t)(k.type & 0xFF);
            x = (x << 28) ^ (uint64_t)k.u;
            x = (x << 28) ^ (uint64_t)k.v;
            // splitmix64-like
            x += 0x9e3779b97f4a7c15ULL;
            x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
            x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
            x = x ^ (x >> 31);
            return (size_t)x;
        }
    };

    void buildAllConnections(const std::vector<Connection>& mm,
                             const std::vector<Connection>& mf,
                             const std::vector<Connection>& ff)
    {
        n_total = n_leaf + n_seg;
        connections.clear();
        connections.reserve(mm.size() + mf.size() + ff.size());

        std::unordered_set<ConnKey, ConnKeyHash> seen;
        seen.reserve((mm.size() + mf.size() + ff.size()) * 2 + 16);

        auto push_unique = [&](const Connection& c) {
            int u = std::min(c.u, c.v);
            int v = std::max(c.u, c.v);
            ConnKey key{c.type, u, v};
            if (seen.insert(key).second) {
                connections.push_back({u, v, c.T, c.type});
            }
        };
        for (const auto& c : mm) push_unique(c);
        for (const auto& c : mf) push_unique(c);
        for (const auto& c : ff) push_unique(c);

        // adjacency
        adj.assign(n_total, {});
        for (const auto& c : connections) {
            adj[c.u].push_back({c.v, c.T, c.type});
            adj[c.v].push_back({c.u, c.T, c.type});
        }
        std::cout << "Connections built (unique): " << connections.size() << std::endl;
    }

    // ---------------------------------------------------------------------
    // 6.10 Wells (reuse your logic: producer on each HF near mid-z)
    // ---------------------------------------------------------------------
    void setupWells() {
        wells.clear();
        well_map.clear();
        std::vector<int> target_fracs = {100,101,102};
        for (int fid : target_fracs) {
            int best_s = -1;
            double best = 1e30;
            for (int s=0; s<n_seg; ++s) {
                if (segments[s].frac_id != fid) continue;
                double dzv = std::abs(segments[s].center.z - Lz*0.5);
                if (dzv < best) { best = dzv; best_s = s; }
            }
            if (best_s >= 0) {
                Well w;
                w.target_node_idx = n_leaf + best_s;
                w.WI = 100.0;
                w.P_bhp = 50.0;
                int idx = (int)wells.size();
                wells.push_back(w);
                well_map[w.target_node_idx] = idx;
            }
        }
        std::cout << "Setup wells = " << wells.size() << std::endl;
    }

    // ---------------------------------------------------------------------
    // 6.11 States init
    // ---------------------------------------------------------------------
    void initState() {
        states.assign(n_total, {});
        states_prev = states;
        for (int i=0; i<n_total; ++i) {
            states[i].P = 200.0;
            states[i].Sw = 0.2;
            states[i].Sg = 0.05;
        }
        states_prev = states;
    }

    // ---------------------------------------------------------------------
    // 6.12 Residual (same structure as your original, but leaf-based)
    // ---------------------------------------------------------------------
    struct Properties {
        double Bw, Bo, Bg;
        double krw, kro, krg;
        double lw, lo, lg;
    };

    Properties getProps(const State& s) const {
        Properties p;
        calcPVT(s.P, p.Bw, p.Bo, p.Bg);
        calcRelPerm(s.Sw, s.Sg, p.krw, p.kro, p.krg);
        p.lw = p.krw / (g_props.mu_w * p.Bw);
        p.lo = p.kro / (g_props.mu_o * p.Bo);
        p.lg = p.krg / (g_props.mu_g * p.Bg);
        return p;
    }

    Vector3d computeNodeResidual(int u, double dt,
                                 const std::vector<State>& curr,
                                 const std::vector<Properties>& props) const
    {
        Vector3d R = Vector3d::Zero();

        double vol = 0.0;
        double phi = 1.0;
        if (u < n_leaf) {
            vol = leaves[u].vol;
            phi = leaves[u].phi;
        } else {
            int s = u - n_leaf;
            vol = segments[s].area * segments[s].aperture;
            phi = 1.0;
        }
        double accum = vol * phi / dt;

        const State& s_new = curr[u];
        const State& s_old = states_prev[u];
        const Properties& p_new = props[u];
        Properties p_old = getProps(s_old);

        // Water
        R(0) = accum * (s_new.Sw / p_new.Bw - s_old.Sw / p_old.Bw);
        // Oil
        double So_new = 1.0 - s_new.Sw - s_new.Sg;
        double So_old = 1.0 - s_old.Sw - s_old.Sg;
        R(1) = accum * (So_new / p_new.Bo - So_old / p_old.Bo);
        // Gas
        R(2) = accum * (s_new.Sg / p_new.Bg - s_old.Sg / p_old.Bg);

        // Flux
        for (const auto& nb : adj[u]) {
            int v = nb.v;
            double T = nb.T;
            double dP = curr[u].P - curr[v].P;
            const Properties& pup = (dP >= 0.0) ? props[u] : props[v];
            R(0) += T * pup.lw * dP;
            R(1) += T * pup.lo * dP;
            R(2) += T * pup.lg * dP;
        }

        // Wells (producer only)
        auto it = well_map.find(u);
        if (it != well_map.end()) {
            const Well& w = wells[it->second];
            double dP = curr[u].P - w.P_bhp;
            if (dP > 0.0) {
                const Properties& pu = props[u];
                R(0) += w.WI * pu.lw * dP;
                R(1) += w.WI * pu.lo * dP;
                R(2) += w.WI * pu.lg * dP;
            }
        }
        return R;
    }

    // ---------------------------------------------------------------------
    // 6.13 Newton step (numerical Jacobian) - kept for compatibility
    //      Warning: this is the main scalability bottleneck after LGR.
    // ---------------------------------------------------------------------
    bool solveStep(double dt, double& step_oil, double& step_water, double& step_gas) {
        const int max_iter = 12;
        const double tol = 1e-3;

        std::vector<State> backup = states;
        std::vector<Properties> props_cache(n_total);

        for (int iter=0; iter<max_iter; ++iter) {
            for (int i=0; i<n_total; ++i) props_cache[i] = getProps(states[i]);

            VectorXd Rg(3*n_total);
            double max_res = 0.0;
            for (int i=0; i<n_total; ++i) {
                Vector3d r = computeNodeResidual(i, dt, states, props_cache);
                Rg.segment<3>(3*i) = r;
                max_res = std::max(max_res, r.lpNorm<Infinity>());
            }
            if (max_res < tol) {
                // production
                for (const auto& w : wells) {
                    int u = w.target_node_idx;
                    double dP = states[u].P - w.P_bhp;
                    if (dP > 0.0) {
                        const auto& pu = props_cache[u];
                        step_water += w.WI * pu.lw * dP * dt;
                        step_oil   += w.WI * pu.lo * dP * dt;
                        step_gas   += w.WI * pu.lg * dP * dt;
                    }
                }
                return true;
            }

            // Jacobian by finite difference (only local affected nodes)
            std::vector<Triplet<double>> trips;
            trips.reserve((size_t)n_total * 60);

            const double epsP = 1e-6;
            const double epsS = 1e-6;

            for (int u=0; u<n_total; ++u) {
                State orig = states[u];
                Properties prop_orig = props_cache[u];

                std::vector<int> affected;
                affected.reserve(adj[u].size() + 1);
                affected.push_back(u);
                for (const auto& nb : adj[u]) affected.push_back(nb.v);
                std::sort(affected.begin(), affected.end());
                affected.erase(std::unique(affected.begin(), affected.end()), affected.end());

                std::vector<Vector3d> base(affected.size());
                for (size_t k=0;k<affected.size();++k) {
                    base[k] = Rg.segment<3>(3*affected[k]);
                }

                for (int var=0; var<3; ++var) {
                    double eps = (var==0) ? epsP : epsS;
                    if (var==0) states[u].P += eps;
                    else if (var==1) states[u].Sw += eps;
                    else states[u].Sg += eps;

                    props_cache[u] = getProps(states[u]);
                    for (size_t k=0;k<affected.size();++k) {
                        int row = affected[k];
                        Vector3d rnew = computeNodeResidual(row, dt, states, props_cache);
                        Vector3d diff = (rnew - base[k]) / eps;
                        for (int eq=0; eq<3; ++eq) {
                            double v = diff(eq);
                            if (std::abs(v) > 1e-20) trips.emplace_back(3*row + eq, 3*u + var, v);
                        }
                    }

                    states[u] = orig;
                    props_cache[u] = prop_orig;
                }
            }

            SparseMatrix<double> J(3*n_total, 3*n_total);
            J.setFromTriplets(trips.begin(), trips.end());

            SparseLU<SparseMatrix<double>> solver;
            solver.analyzePattern(J);
            solver.factorize(J);
            if (solver.info() != Success) {
                states = backup;
                return false;
            }
            VectorXd delta = solver.solve(-Rg);

            // damping
            double damping = 1.0;
            double maxDP=0.0, maxDS=0.0;
            for (int i=0;i<n_total;++i) {
                maxDP = std::max(maxDP, std::abs(delta(3*i+0)));
                maxDS = std::max(maxDS, std::abs(delta(3*i+1)));
                maxDS = std::max(maxDS, std::abs(delta(3*i+2)));
            }
            if (maxDP > 20.0) damping = std::min(damping, 20.0 / maxDP);
            if (maxDS > 0.1)  damping = std::min(damping, 0.1 / maxDS);

            for (int i=0;i<n_total;++i) {
                states[i].P  += delta(3*i+0) * damping;
                states[i].Sw += delta(3*i+1) * damping;
                states[i].Sg += delta(3*i+2) * damping;
                states[i].P = std::max(1.0, states[i].P);
                states[i].Sw = clamp01(states[i].Sw);
                states[i].Sg = clamp01(states[i].Sg);
                if (states[i].Sw + states[i].Sg > 1.0) {
                    double ssum = states[i].Sw + states[i].Sg;
                    states[i].Sw /= ssum;
                    states[i].Sg /= ssum;
                }
            }
        }

        states = backup;
        return false;
    }

    // ---------------------------------------------------------------------
    // 6.14 Run with adaptive dt (same pattern)
    // ---------------------------------------------------------------------
    void run(double total_days) {
        std::ofstream file("output_sim_lgr.csv");
        file << "Time,CumOil,CumWater,CumGas,AvgPressure,DT,nLeaf,nSeg\n";

        double t = 0.0;
        double dt = 0.001;
        double dt_min = 1e-6;
        double dt_max = 5.0;
        double tot_o=0, tot_w=0, tot_g=0;

        int step = 0;
        while (t < total_days - 1e-12) {
            step++;
            if (t + dt > total_days) dt = total_days - t;
            double so=0, sw=0, sg=0;
            bool ok = false;
            while (!ok) {
                if (dt < dt_min) {
                    std::cerr << "dt too small, abort." << std::endl;
                    return;
                }
                std::cout << "Step "<<step<<" t="<<t<<" dt="<<dt<<" ... "<<std::flush;
                ok = solveStep(dt, so, sw, sg);
                if (!ok) {
                    std::cout << "fail -> dt*=0.5" << std::endl;
                    dt *= 0.5;
                }
            }
            std::cout << "ok" << std::endl;
            t += dt;
            states_prev = states;
            tot_o += so; tot_w += sw; tot_g += sg;

            // avg P on leaf cells
            double avgP = 0.0;
            for (int i=0;i<n_leaf;++i) avgP += states[i].P;
            avgP /= std::max(1, n_leaf);

            file << t << "," << tot_o << "," << tot_w << "," << tot_g << "," << avgP << "," << dt
                 << "," << n_leaf << "," << n_seg << "\n";
            file.flush();

            dt = std::min(dt_max, dt * 2.0);
        }
        file.close();

        // export leaf final field
        std::ofstream field("final_field_lgr.csv");
        field << "leaf_id,parent_id,x,y,z,P,Sw,Sg\n";
        for (int i=0;i<n_leaf;++i) {
            field << i << "," << leaves[i].parent_id << ","
                  << leaves[i].center.x << "," << leaves[i].center.y << "," << leaves[i].center.z << ","
                  << states[i].P << "," << states[i].Sw << "," << states[i].Sg << "\n";
        }
        field.close();

        // export segments
        std::ofstream segf("segments_lgr.csv");
        segf << "seg_id,frac_id,leaf_id,parent_id,area,cx,cy,cz,Tmf\n";
        for (const auto& s : segments) {
            segf << s.id << "," << s.frac_id << "," << s.matrix_leaf_id << "," << s.parent_id << ","
                 << s.area << "," << s.center.x << "," << s.center.y << "," << s.center.z << "," << s.T_mf << "\n";
        }
        segf.close();
    }

    // ---------------------------------------------------------------------
    // 6.15 One-click pipeline
    // ---------------------------------------------------------------------
    void preprocess() {
        buildParentGrid();
        generateFractures();
        markRefinement();
        buildLeafGrid();
        buildParentFaceLeaves();

        std::vector<Connection> mm, mf, ff;
        buildMMConnections(mm);
        buildSegmentsAndMF(mf);
        buildFFConnections(ff);
        buildAllConnections(mm, mf, ff);

        setupWells();
        initState();
    }
};

// =============================================================================
// main
// =============================================================================

int main() {
    SimulatorLGR sim;

    std::cout << "Preprocessing (Parent->Leaf LGR + EDFM)..." << std::endl;
    sim.preprocess();

    std::cout << "Running simulation..." << std::endl;
    sim.run(10.0); // SAFE default: 10 days

    std::cout << "Done. Outputs: output_sim_lgr.csv, final_field_lgr.csv, segments_lgr.csv" << std::endl;
    return 0;
}
