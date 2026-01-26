// =============================================================================
// 文件名: edfm_3d_blackoil_interface.cpp
// 描述: 集成了 AlgoToVTK 和 QtUItoAlgo 接口的 3D 黑油模型求解器
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
#include <string>
#include <thread>
#include <chrono>

#include <Eigen/Sparse>
#include <Eigen/Dense>
#include <Eigen/SparseLU>

// =============================================================================
// 接口定义
// =============================================================================

namespace AlgoToVTK {
    struct Point3 { double x, y, z; };
    struct CellData { int id; Point3 center; double pressure; double Sw; double Sg; };
    struct FractureData { int id; std::vector<Point3> vertices; double aperture; double perm; };
    struct WellData { int id; Point3 position; double bhp; double rate; };
    struct GridData { int Nx, Ny, Nz; double Lx, Ly, Lz; double dx, dy, dz; };

    class IVisualizationData {
    public:
        virtual ~IVisualizationData() = default;
        virtual GridData getGridData() = 0;
        virtual std::vector<CellData> getCellData() = 0;
        virtual std::vector<FractureData> getFractureData() = 0;
        virtual std::vector<WellData> getWellData() = 0;
        virtual std::map<std::string, std::vector<double>> getProductionData() = 0;
        virtual std::vector<double> getTimeSteps() = 0;
    };
}

namespace QtUItoAlgo {
    struct Point3 { double x, y, z; };
    struct FractureInput { int id; Point3 vertices[4]; double aperture; double perm; };
    struct GridParameters { int Nx, Ny, Nz; double Lx, Ly, Lz; };
    struct FluidProperties {
        double mu_w = 1.0; double mu_o = 5.0; double mu_g = 0.2;
        double cw = 1e-8; double co = 1e-5; double cg = 1e-3;
        double P_ref = 100.0;
        double Swi = 0.2; double Sor = 0.2; double Sgc = 0.05;
    };
    struct WellParameters { int target_fracture_id; double WI; double P_bhp; };
    struct SimulationParameters { double total_time_days; double initial_time_step; double min_time_step; double max_time_step; };

    class ISimulatorController {
    public:
        virtual ~ISimulatorController() = default;
        virtual bool initGrid(const GridParameters& params) = 0;
        virtual bool addFractures(const std::vector<FractureInput>& fractures) = 0;
        virtual bool setFluidProperties(const FluidProperties& props) = 0;
        virtual bool addWells(const std::vector<WellParameters>& wells) = 0;
        virtual bool setSimulationParameters(const SimulationParameters& params) = 0;
        virtual bool runSimulation() = 0;
        virtual bool pauseSimulation() = 0;
        virtual bool resumeSimulation() = 0;
        virtual bool stopSimulation() = 0;
        virtual std::string getSimulationStatus() = 0;
        virtual double getCurrentTime() = 0;
    };

    class IDataTransfer {
    public:
        virtual ~IDataTransfer() = default;
        virtual bool exportResults(const std::string& output_dir) = 0;
        virtual bool exportGeometry(const std::string& output_path) = 0;
        virtual std::map<std::string, std::vector<double>> getProductionData() = 0;
        virtual std::vector<std::tuple<double, double, double, double>> getPressureField() = 0;
    };

    class ISimulationCallback {
    public:
        virtual ~ISimulationCallback() = default;
        virtual void onProgressUpdate(double progress, double current_time) = 0;
        virtual void onSimulationCompleted() = 0;
        virtual void onSimulationFailed(const std::string& error_message) = 0;
        virtual void onTimeStepChanged(double new_time_step) = 0;
    };
}

// =============================================================================
// 内部算法命名空间
// =============================================================================
namespace Internal {
    using namespace std;
    using namespace Eigen;

    const double EPSILON = 1e-8;
    const double PI = 3.14159265358979323846;

    struct Vec3 {
        double x, y, z;
        Vec3 operator+(const Vec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
        Vec3 operator-(const Vec3& other) const { return {x - other.x, y - other.y, z - other.z}; }
        Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
        double dot(const Vec3& other) const { return x * other.x + y * other.y + z * other.z; }
        Vec3 cross(const Vec3& other) const { return {y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x}; }
        double norm() const { return std::sqrt(x*x + y*y + z*z); }
    };

    struct Fracture {
        int id;
        Vec3 vertices[4];
        double aperture;
        double perm;
    };

    struct Cell {
        int id;
        int ix, iy, iz;
        Vec3 center;
        double dx, dy, dz;
        double vol;
        double phi;
        double K[3];
        double depth;
    };

    struct Segment {
        int id; int frac_id; int cell_id; double area;
        Vec3 center; Vec3 normal;
        double aperture; double perm; double T_mf;
    };

    struct Connection {
        int u; int v; double T; int type;
    };

    struct FluidProps {
        double mu_w = 1.0; double mu_o = 5.0; double mu_g = 0.2;
        double cw = 1e-8; double co = 1e-5; double cg = 1e-3;
        double P_ref = 100.0;
        double Swi = 0.2; double Sor = 0.2; double Sgc = 0.05;
    };

    struct State {
        double P; double Sw; double Sg;
    };

    double polygonArea(const std::vector<Vec3>& poly) {
        if (poly.size() < 3) return 0.0;
        Vec3 total = {0, 0, 0};
        Vec3 v0 = poly[0];
        for (size_t i = 1; i < poly.size() - 1; ++i) {
            Vec3 v1 = poly[i]; Vec3 v2 = poly[i+1];
            total = total + (v1 - v0).cross(v2 - v0);
        }
        return 0.5 * total.norm();
    }

    Vec3 polygonCenter(const std::vector<Vec3>& poly) {
        Vec3 c = {0,0,0};
        if (poly.empty()) return c;
        for (const auto& p : poly) c = c + p;
        return c * (1.0 / poly.size());
    }

    bool isInside(const Vec3& p, const Vec3& planeNormal, double planeD) {
        return (planeNormal.dot(p) + planeD) >= -1e-9;
    }

    Vec3 intersectPlane(const Vec3& p1, const Vec3& p2, const Vec3& planeNormal, double planeD) {
        double d1 = planeNormal.dot(p1) + planeD;
        double d2 = planeNormal.dot(p2) + planeD;
        double t = d1 / (d1 - d2);
        return p1 + (p2 - p1) * t;
    }

    std::vector<Vec3> clipPolygonCurrentPlane(const std::vector<Vec3>& inputPoly, const Vec3& normal, double d) {
        std::vector<Vec3> outputPoly;
        if (inputPoly.empty()) return outputPoly;
        for (size_t i = 0; i < inputPoly.size(); ++i) {
            Vec3 cur = inputPoly[i];
            Vec3 prev = inputPoly[(i + inputPoly.size() - 1) % inputPoly.size()];
            bool curIn = isInside(cur, normal, d);
            bool prevIn = isInside(prev, normal, d);
            if (curIn) {
                if (!prevIn) outputPoly.push_back(intersectPlane(prev, cur, normal, d));
                outputPoly.push_back(cur);
            } else if (prevIn) {
                outputPoly.push_back(intersectPlane(prev, cur, normal, d));
            }
        }
        return outputPoly;
    }

    std::vector<Vec3> clipFractureBox(const Fracture& frac, const Cell& cell) {
        std::vector<Vec3> poly;
        for (int i=0; i<4; ++i) poly.push_back(frac.vertices[i]);
        double x_min = cell.center.x - cell.dx/2; double x_max = cell.center.x + cell.dx/2;
        double y_min = cell.center.y - cell.dy/2; double y_max = cell.center.y + cell.dy/2;
        double z_min = cell.center.z - cell.dz/2; double z_max = cell.center.z + cell.dz/2;
        poly = clipPolygonCurrentPlane(poly, {1,0,0}, -x_min);
        poly = clipPolygonCurrentPlane(poly, {-1,0,0}, x_max);
        poly = clipPolygonCurrentPlane(poly, {0,1,0}, -y_min);
        poly = clipPolygonCurrentPlane(poly, {0,-1,0}, y_max);
        poly = clipPolygonCurrentPlane(poly, {0,0,1}, -z_min);
        poly = clipPolygonCurrentPlane(poly, {0,0,-1}, z_max);
        return poly;
    }

    void calcPVT(const FluidProps& props, double P, double& Bw, double& Bo, double& Bg, double& dBw_dP, double& dBo_dP, double& dBg_dP) {
        double dP = P - props.P_ref;
        Bw = std::exp(-props.cw * dP);
        Bo = std::exp(-props.co * dP);
        Bg = std::exp(-props.cg * dP);
    }

    void calcRelPerm(const FluidProps& props, double Sw, double Sg, double& krw, double& kro, double& krg) {
        double So = 1.0 - Sw - Sg;
        double Sw_norm = (Sw - props.Swi) / (1.0 - props.Swi - props.Sor);
        double Sg_norm = (Sg - props.Sgc) / (1.0 - props.Sgc - props.Swi - props.Sor);
        Sw_norm = std::max(0.0, std::min(1.0, Sw_norm));
        Sg_norm = std::max(0.0, std::min(1.0, Sg_norm));
        krw = std::pow(Sw_norm, 2.0);
        krg = std::pow(Sg_norm, 2.0);
        double So_norm = 1.0 - Sw_norm - Sg_norm;
        So_norm = std::max(0.0, std::min(1.0, So_norm));
        kro = std::pow(So_norm, 2.0);
    }
}

// =============================================================================
// 模拟器类实现
// =============================================================================

class Simulator : public QtUItoAlgo::ISimulatorController, 
                  public QtUItoAlgo::IDataTransfer,
                  public AlgoToVTK::IVisualizationData 
{
private:
    Internal::FluidProps g_props;
    int Nx, Ny, Nz;
    double Lx, Ly, Lz;
    double dx, dy, dz;
    std::vector<Internal::Cell> cells;
    std::vector<Internal::Fracture> fractures;
    std::vector<Internal::Segment> segments;
    std::vector<Internal::Connection> connections;
    
    struct Neighbor { int v; double T; int conn_idx; };
    std::vector<std::vector<Neighbor>> adj;

    int n_matrix;
    int n_frac_nodes;
    int n_total;

    std::vector<Internal::State> states; 
    std::vector<Internal::State> states_prev; 

    struct Well { int target_node_idx; double WI; double P_bhp; };
    std::vector<Well> wells;
    std::map<int, int> well_map; 
    
    QtUItoAlgo::SimulationParameters sim_params;
    QtUItoAlgo::ISimulationCallback* callback = nullptr;
    bool is_running = false;
    bool is_paused = false;
    bool stop_requested = false;
    double current_sim_time = 0.0;
    
    std::vector<double> time_history;
    std::vector<double> oil_prod_history;
    std::vector<double> water_prod_history;
    std::vector<double> gas_prod_history;

public:
    Simulator() {
        sim_params = {100.0, 0.001, 1e-6, 10.0};
    }

    void setCallback(QtUItoAlgo::ISimulationCallback* cb) {
        callback = cb;
    }

    bool initGrid(const QtUItoAlgo::GridParameters& params) override {
        std::cout << "Initializing Grid..." << std::endl;
        Nx = params.Nx; Ny = params.Ny; Nz = params.Nz;
        Lx = params.Lx; Ly = params.Ly; Lz = params.Lz;
        dx = Lx/Nx; dy = Ly/Ny; dz = Lz/Nz;
        
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
                    cells[id].K[0] = 0.1; cells[id].K[1] = 0.1; cells[id].K[2] = 0.01; 
                    cells[id].depth = cells[id].center.z;
                }
            }
        }
        return true;
    }

    bool addFractures(const std::vector<QtUItoAlgo::FractureInput>& input_fracs) override {
        std::cout << "Generating Fractures..." << std::endl;
        fractures.clear();
        for(const auto& in_f : input_fracs) {
            Internal::Fracture f;
            f.id = in_f.id;
            f.aperture = in_f.aperture;
            f.perm = in_f.perm;
            for(int i=0; i<4; ++i) {
                f.vertices[i] = {in_f.vertices[i].x, in_f.vertices[i].y, in_f.vertices[i].z};
            }
            fractures.push_back(f);
        }
        processGeometry();
        buildConnections();
        return true;
    }

    bool setFluidProperties(const QtUItoAlgo::FluidProperties& props) override {
        g_props.mu_w = props.mu_w; g_props.mu_o = props.mu_o; g_props.mu_g = props.mu_g;
        g_props.cw = props.cw; g_props.co = props.co; g_props.cg = props.cg;
        g_props.P_ref = props.P_ref;
        g_props.Swi = props.Swi; g_props.Sor = props.Sor; g_props.Sgc = props.Sgc;
        return true;
    }

    bool addWells(const std::vector<QtUItoAlgo::WellParameters>& input_wells) override {
        std::cout << "Setting up Wells..." << std::endl;
        wells.clear();
        well_map.clear();
        for(const auto& w_param : input_wells) {
            std::vector<int> cands;
            for(int s=0; s<n_frac_nodes; ++s) if(segments[s].frac_id == w_param.target_fracture_id) cands.push_back(s);
            int best_s = -1; double min_dz = 1e9;
            for(int s : cands) {
                double dz_val = std::abs(segments[s].center.z - Lz/2.0);
                if(dz_val < min_dz) { min_dz = dz_val; best_s = s; }
            }
            if(best_s != -1) {
                Well w; w.target_node_idx = n_matrix + best_s; w.WI = w_param.WI; w.P_bhp = w_param.P_bhp;
                wells.push_back(w);
                well_map[w.target_node_idx] = (int)wells.size() - 1;
            }
        }
        std::cout << "Setup " << wells.size() << " well connections." << std::endl;
        return true;
    }

    bool setSimulationParameters(const QtUItoAlgo::SimulationParameters& params) override {
        sim_params = params;
        return true;
    }

    bool runSimulation() override {
        if(is_running) return false;
        is_running = true;
        is_paused = false;
        stop_requested = false;
        
        initState();
        
        std::ofstream file("output_sim.csv");
        file << "Time,CumOil,CumWater,CumGas,AvgPressure,DT\n";

        current_sim_time = 0.0;
        double dt = sim_params.initial_time_step;
        
        double tot_oil=0, tot_water=0, tot_gas=0;
        int step_count = 0;
        
        time_history.clear(); oil_prod_history.clear(); water_prod_history.clear(); gas_prod_history.clear();

        std::cout << std::fixed << std::setprecision(4);

        std::cout << "Starting Simulation..." << std::endl;

        while(current_sim_time < sim_params.total_time_days && !stop_requested) {
            if(is_paused) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            step_count++;
            double step_oil=0, step_water=0, step_gas=0;
            bool success = false;
            
            if(current_sim_time + dt > sim_params.total_time_days) dt = sim_params.total_time_days - current_sim_time;

            while(!success && !stop_requested) {
                if(dt < sim_params.min_time_step) {
                    if(callback) callback->onSimulationFailed("Time step too small.");
                    is_running = false;
                    return false;
                }
                
                if(callback) callback->onTimeStepChanged(dt);

                std::cout << "Step " << step_count << " @ T=" << current_sim_time << " trying dt=" << dt << " ... " << std::flush;
                success = solveStep(dt, step_oil, step_water, step_gas);

                if(success) {
                    std::cout << "Converged." << std::endl;
                    current_sim_time += dt;
                    states_prev = states;
                    
                    tot_oil += step_oil; tot_water += step_water; tot_gas += step_gas;
                    
                    time_history.push_back(current_sim_time);
                    oil_prod_history.push_back(tot_oil);
                    water_prod_history.push_back(tot_water);
                    gas_prod_history.push_back(tot_gas);

                    double avgP = 0; for(int i=0; i<n_matrix; ++i) avgP += states[i].P; avgP /= n_matrix;
                    file << current_sim_time << "," << tot_oil << "," << tot_water << "," << tot_gas << "," << avgP << "," << dt << "\n";
                    file.flush();

                    dt = std::min(sim_params.max_time_step, dt * 2.0);
                    
                    if(callback) callback->onProgressUpdate(current_sim_time / sim_params.total_time_days, current_sim_time);

                } else {
                    std::cout << "Failed. Cutting timestep." << std::endl;
                    dt *= 0.5;
                }
            }
        }
        
        file.close();
        is_running = false;
        if(callback) callback->onSimulationCompleted();
        return true;
    }

    bool pauseSimulation() override { is_paused = true; return true; }
    bool resumeSimulation() override { is_paused = false; return true; }
    bool stopSimulation() override { stop_requested = true; return true; }
    std::string getSimulationStatus() override {
        if(!is_running) return "Stopped";
        if(is_paused) return "Paused";
        return "Running";
    }
    double getCurrentTime() override { return current_sim_time; }

    bool exportResults(const std::string& output_dir) override {
        std::string path = output_dir + "/final_field.csv";
        std::ofstream field(path);
        field << "x,y,z,P,Sw,Sg\n";
        for(int i=0; i<n_matrix; ++i) {
            field << cells[i].center.x << "," << cells[i].center.y << "," << cells[i].center.z << ","
                  << states[i].P << "," << states[i].Sw << "," << states[i].Sg << "\n";
        }
        return true;
    }

    bool exportGeometry(const std::string& output_path) override {
        std::string p_grid = output_path + "/grid_info.csv";
        std::ofstream gridFile(p_grid);
        gridFile << Nx << "," << Ny << "," << Nz << "," << Lx << "," << Ly << "," << Lz << "," << dx << "," << dy << "," << dz << "\n";
        gridFile.close();

        std::string p_frac = output_path + "/fracture_geometry.csv";
        std::ofstream fracFile(p_frac);
        fracFile << "id,x0,y0,z0,x1,y1,z1,x2,y2,z2,x3,y3,z3\n";
        for (const auto& f : fractures) {
            fracFile << f.id;
            for (int i = 0; i < 4; ++i) fracFile << "," << f.vertices[i].x << "," << f.vertices[i].y << "," << f.vertices[i].z;
            fracFile << "\n";
        }
        fracFile.close();
        
        std::string p_well = output_path + "/well_info.csv";
        std::ofstream wellFile(p_well);
        wellFile << "well_id,node_idx,type,x,y,z,WI,P_bhp\n";
        for (size_t i = 0; i < wells.size(); ++i) {
            int u = wells[i].target_node_idx;
            std::string type = (u < n_matrix) ? "Matrix" : "Fracture";
            Internal::Vec3 c;
            if (u < n_matrix) c = cells[u].center;
            else c = segments[u - n_matrix].center;
            wellFile << i << "," << u << "," << type << "," 
                     << c.x << "," << c.y << "," << c.z << ","
                     << wells[i].WI << "," << wells[i].P_bhp << "\n";
        }
        wellFile.close();

        std::cout << "Geometry exported: grid_info.csv, fracture_geometry.csv and well_info.csv" << std::endl;
        return true;
    }

    std::map<std::string, std::vector<double>> getProductionData() override {
        std::map<std::string, std::vector<double>> data;
        data["Time"] = time_history;
        data["Oil"] = oil_prod_history;
        data["Water"] = water_prod_history;
        data["Gas"] = gas_prod_history;
        return data;
    }

    std::vector<std::tuple<double, double, double, double>> getPressureField() override {
        std::vector<std::tuple<double, double, double, double>> field;
        for(int i=0; i<n_matrix; ++i) {
            field.emplace_back(cells[i].center.x, cells[i].center.y, cells[i].center.z, states[i].P);
        }
        return field;
    }

    AlgoToVTK::GridData getGridData() override {
        return {Nx, Ny, Nz, Lx, Ly, Lz, dx, dy, dz};
    }

    std::vector<AlgoToVTK::CellData> getCellData() override {
        std::vector<AlgoToVTK::CellData> data;
        data.reserve(n_matrix);
        for(int i=0; i<n_matrix; ++i) {
            AlgoToVTK::CellData c;
            c.id = cells[i].id;
            c.center = {cells[i].center.x, cells[i].center.y, cells[i].center.z};
            c.pressure = states[i].P;
            c.Sw = states[i].Sw;
            c.Sg = states[i].Sg;
            data.push_back(c);
        }
        return data;
    }

    std::vector<AlgoToVTK::FractureData> getFractureData() override {
        std::vector<AlgoToVTK::FractureData> data;
        for(const auto& f : fractures) {
            AlgoToVTK::FractureData fd;
            fd.id = f.id;
            fd.aperture = f.aperture;
            fd.perm = f.perm;
            for(int i=0; i<4; ++i) fd.vertices.push_back({f.vertices[i].x, f.vertices[i].y, f.vertices[i].z});
            data.push_back(fd);
        }
        return data;
    }

    std::vector<AlgoToVTK::WellData> getWellData() override {
        std::vector<AlgoToVTK::WellData> data;
        for(size_t i=0; i<wells.size(); ++i) {
            AlgoToVTK::WellData wd;
            wd.id = (int)i;
            int node = wells[i].target_node_idx;
            Internal::Vec3 pos;
            if(node < n_matrix) pos = cells[node].center;
            else pos = segments[node-n_matrix].center;
            wd.position = {pos.x, pos.y, pos.z};
            wd.bhp = wells[i].P_bhp;
            wd.rate = 0; 
            data.push_back(wd);
        }
        return data;
    }

    std::vector<double> getTimeSteps() override {
        return time_history;
    }

private:
    void processGeometry() {
        std::cout << "Processing Geometry..." << std::endl;
        segments.clear();
        int seg_id_counter = 0;
        for(const auto& frac : fractures) {
            double fmin_x = 1e9, fmax_x = -1e9, fmin_y = 1e9, fmax_y = -1e9, fmin_z = 1e9, fmax_z = -1e9;
            for(const auto& v : frac.vertices) {
                fmin_x = std::min(fmin_x, v.x); fmax_x = std::max(fmax_x, v.x);
                fmin_y = std::min(fmin_y, v.y); fmax_y = std::max(fmax_y, v.y);
                fmin_z = std::min(fmin_z, v.z); fmax_z = std::max(fmax_z, v.z);
            }
            int i_start = std::max(0, (int)(fmin_x / dx)); int i_end   = std::min(Nx-1, (int)(fmax_x / dx));
            int j_start = std::max(0, (int)(fmin_y / dy)); int j_end   = std::min(Ny-1, (int)(fmax_y / dy));
            int k_start = std::max(0, (int)(fmin_z / dz)); int k_end   = std::min(Nz-1, (int)(fmax_z / dz));
            Internal::Vec3 vec1 = frac.vertices[1] - frac.vertices[0];
            Internal::Vec3 vec2 = frac.vertices[3] - frac.vertices[0];
            Internal::Vec3 normal = vec1.cross(vec2); normal = normal * (1.0 / normal.norm());
            for(int k=k_start; k<=k_end; ++k) {
                for(int j=j_start; j<=j_end; ++j) {
                    for(int i=i_start; i<=i_end; ++i) {
                        int cell_idx = k*Nx*Ny + j*Nx + i;
                        std::vector<Internal::Vec3> poly = Internal::clipFractureBox(frac, cells[cell_idx]);
                        double area = Internal::polygonArea(poly);
                        if(area > 1e-6) { 
                            Internal::Segment seg; seg.id = seg_id_counter++; seg.frac_id = frac.id;
                            seg.cell_id = cell_idx; seg.area = area; seg.center = Internal::polygonCenter(poly);
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

    void buildConnections() {
        std::cout << "Building Connections..." << std::endl;
        connections.clear();
        adj.assign(n_total, std::vector<Neighbor>());

        auto add_conn = [&](int u, int v, double T, int type) {
            Internal::Connection c; c.u = u; c.v = v; c.T = T; c.type = type;
            connections.push_back(c);
            int conn_idx = (int)connections.size() - 1;
            adj[u].push_back({v, T, conn_idx});
            adj[v].push_back({u, T, conn_idx});
        };

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

        for(int s=0; s<n_frac_nodes; ++s) {
            int u = segments[s].cell_id;
            int v = n_matrix + s;
            add_conn(u, v, segments[s].T_mf, 1);
        }

        std::map<int, std::vector<int>> frac_seg_map;
        for(int s=0; s<n_frac_nodes; ++s) frac_seg_map[segments[s].frac_id].push_back(s);

        for(auto& entry : frac_seg_map) {
            const std::vector<int>& segs = entry.second;
            for(size_t i=0; i<segs.size(); ++i) {
                for(size_t j=i+1; j<segs.size(); ++j) {
                    int s1 = segs[i]; int s2 = segs[j];
                    double dist = (segments[s1].center - segments[s2].center).norm();
                    if(dist < (dx + dy + dz)/2.0 * 1.5) {
                         double width = std::sqrt(segments[s1].area); 
                         double T = segments[s1].perm * segments[s1].aperture * width / dist;
                         add_conn(n_matrix + s1, n_matrix + s2, T, 2);
                    }
                }
            }
        }

        std::map<int, std::vector<int>> cell_seg_map;
        for(int s=0; s<n_frac_nodes; ++s) cell_seg_map[segments[s].cell_id].push_back(s);
        for(auto& entry : cell_seg_map) {
            const std::vector<int>& segs = entry.second;
            for(size_t i=0; i<segs.size(); ++i) {
                for(size_t j=i+1; j<segs.size(); ++j) {
                    int s1 = segs[i]; int s2 = segs[j];
                    if(segments[s1].frac_id != segments[s2].frac_id) {
                        double T = 1000.0;
                        add_conn(n_matrix+s1, n_matrix+s2, T, 2);
                    }
                }
            }
        }
        std::cout << "Built " << connections.size() << " connections." << std::endl;
    }

    void initState() {
        states.resize(n_total); states_prev.resize(n_total);
        for(int i=0; i<n_total; ++i) {
            states[i].P = 200.0; states[i].Sw = 0.2; states[i].Sg = 0.05;
            states_prev[i] = states[i];
        }
    }

    struct Props {
        double Bw, Bo, Bg, krw, kro, krg, lw, lo, lg; 
    };

    Props getProps(const Internal::State& s) {
        Props p; double dummy;
        Internal::calcPVT(g_props, s.P, p.Bw, p.Bo, p.Bg, dummy, dummy, dummy);
        Internal::calcRelPerm(g_props, s.Sw, s.Sg, p.krw, p.kro, p.krg);
        p.lw = p.krw / (g_props.mu_w * p.Bw);
        p.lo = p.kro / (g_props.mu_o * p.Bo);
        p.lg = p.krg / (g_props.mu_g * p.Bg);
        return p;
    }

    Eigen::Vector3d computeNodeResidual(int u, double dt, const std::vector<Internal::State>& curr_states, const std::vector<Props>& curr_props) {
        Eigen::Vector3d R_node = Eigen::Vector3d::Zero();
        double vol = (u < n_matrix) ? cells[u].vol : (segments[u-n_matrix].area * segments[u-n_matrix].aperture);
        double phi = (u < n_matrix) ? cells[u].phi : 1.0;
        double accum_coeff = vol * phi / dt;
        
        const auto& s_new = curr_states[u];
        const auto& s_old = states_prev[u];
        const auto& p_new = curr_props[u];
        Props p_old = getProps(s_old);

        R_node(0) = accum_coeff * (s_new.Sw/p_new.Bw - s_old.Sw/p_old.Bw);
        double So_new = 1.0 - s_new.Sw - s_new.Sg;
        double So_old = 1.0 - s_old.Sw - s_old.Sg;
        R_node(1) = accum_coeff * (So_new/p_new.Bo - So_old/p_old.Bo);
        R_node(2) = accum_coeff * (s_new.Sg/p_new.Bg - s_old.Sg/p_old.Bg);

        for(const auto& nb : adj[u]) {
            int v = nb.v; double T = nb.T;
            double dPhi = curr_states[u].P - curr_states[v].P;
            const Props& p_up = (dPhi >= 0) ? curr_props[u] : curr_props[v];
            
            R_node(0) += T * p_up.lw * dPhi;
            R_node(1) += T * p_up.lo * dPhi;
            R_node(2) += T * p_up.lg * dPhi;
        }

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

    bool solveStep(double dt, double& step_oil, double& step_water, double& step_gas) {
        int max_iter = 15; double tol = 1e-3;
        std::vector<Internal::State> states_backup = states;
        std::vector<Props> props_cache(n_total);

        for(int iter=0; iter<max_iter; ++iter) {
            for(int i=0; i<n_total; ++i) props_cache[i] = getProps(states[i]);

            Eigen::VectorXd R_global(3*n_total);
            double max_resid = 0.0;
            
            for(int i=0; i<n_total; ++i) {
                Eigen::Vector3d r_i = computeNodeResidual(i, dt, states, props_cache);
                R_global.segment<3>(3*i) = r_i;
                double local_norm = r_i.lpNorm<Eigen::Infinity>();
                if(local_norm > max_resid) max_resid = local_norm;
            }
            
            if(max_resid < tol) {
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
                return true; 
            }

            Eigen::SparseMatrix<double> J(3*n_total, 3*n_total);
            std::vector<Eigen::Triplet<double>> tripletList;
            double eps_P = 1e-6; double eps_S = 1e-6;

            for(int u=0; u<n_total; ++u) {
                Internal::State s_orig = states[u];
                Props p_orig = props_cache[u];
                std::vector<int> affected_nodes; affected_nodes.reserve(adj[u].size() + 1);
                affected_nodes.push_back(u);
                for(const auto& nb : adj[u]) affected_nodes.push_back(nb.v);

                std::vector<Eigen::Vector3d> base_resids(affected_nodes.size());
                for(size_t k=0; k<affected_nodes.size(); ++k) base_resids[k] = R_global.segment<3>(3*affected_nodes[k]);

                for(int var=0; var<3; ++var) {
                    double eps = (var==0) ? eps_P : eps_S;
                    if(var==0) states[u].P += eps;
                    else if(var==1) states[u].Sw += eps;
                    else states[u].Sg += eps;
                    props_cache[u] = getProps(states[u]);

                    for(size_t k=0; k<affected_nodes.size(); ++k) {
                        int row_node = affected_nodes[k];
                        Eigen::Vector3d r_new = computeNodeResidual(row_node, dt, states, props_cache);
                        Eigen::Vector3d diff = (r_new - base_resids[k]) / eps;
                        for(int eq=0; eq<3; ++eq) {
                            if(std::abs(diff(eq)) > 1e-20) tripletList.emplace_back(3*row_node + eq, 3*u + var, diff(eq));
                        }
                    }
                    if(var==0) states[u].P = s_orig.P;
                    else if(var==1) states[u].Sw = s_orig.Sw;
                    else states[u].Sg = s_orig.Sg;
                }
                props_cache[u] = p_orig; 
            }

            J.setFromTriplets(tripletList.begin(), tripletList.end());
            Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;
            solver.analyzePattern(J); solver.factorize(J);
            if(solver.info() != Eigen::Success) { states = states_backup; return false; }
            Eigen::VectorXd delta = solver.solve(-R_global);
            
            double damping = 1.0; 
            double max_delta_P = 0; double max_delta_S = 0;
            for(int i=0; i<n_total; ++i) {
                max_delta_P = std::max(max_delta_P, std::abs(delta(3*i+0)));
                max_delta_S = std::max(max_delta_S, std::abs(delta(3*i+1)));
                max_delta_S = std::max(max_delta_S, std::abs(delta(3*i+2)));
            }
            if(max_delta_P > 20.0) damping = std::min(damping, 20.0 / max_delta_P);
            if(max_delta_S > 0.1)  damping = std::min(damping, 0.1 / max_delta_S);

            for(int i=0; i<n_total; ++i) {
                states[i].P += delta(3*i+0) * damping;
                states[i].Sw += delta(3*i+1) * damping;
                states[i].Sg += delta(3*i+2) * damping;
                states[i].P = std::max(1.0, states[i].P);
                states[i].Sw = std::max(0.0, std::min(1.0, states[i].Sw));
                states[i].Sg = std::max(0.0, std::min(1.0, states[i].Sg));
                if(states[i].Sw + states[i].Sg > 1.0) {
                    double sum = states[i].Sw + states[i].Sg;
                    states[i].Sw /= sum; states[i].Sg /= sum;
                }
            }
        }
        states = states_backup;
        return false;
    }
};

// =============================================================================
// Main 函数
// =============================================================================

// int main() {
//     Simulator sim;
    
//     QtUItoAlgo::GridParameters gridParams;
//     gridParams.Lx = 1000; gridParams.Ly = 500; gridParams.Lz = 20;
//     gridParams.Nx = 20; gridParams.Ny = 10; gridParams.Nz = 2;
//     sim.initGrid(gridParams);

//     std::vector<QtUItoAlgo::FractureInput> fracs;
//     std::mt19937 rng(42);
//     std::uniform_real_distribution<double> distX(0, 1000), distY(0, 500), distZ(0, 20);
//     std::uniform_real_distribution<double> distAngle(0, Internal::PI), distDip(0, Internal::PI/3.0), distL(30, 80);

//     for(int i=0; i<100; ++i) {
//         QtUItoAlgo::FractureInput f; f.id = i; f.aperture = 0.001; f.perm = 10000.0;
//         Internal::Vec3 center = {distX(rng), distY(rng), distZ(rng)};
//         double len = distL(rng); double height = distL(rng) * 0.5;
//         double strike = distAngle(rng); double dip = distDip(rng);
//         Internal::Vec3 u = {cos(strike), sin(strike), 0}; 
//         Internal::Vec3 n_horiz = {-sin(strike), cos(strike), 0};
//         Internal::Vec3 v = {n_horiz.x * cos(dip), n_horiz.y * cos(dip), -sin(dip)}; 
        
//         Internal::Vec3 v0 = center - u*(len/2) - v*(height/2);
//         Internal::Vec3 v1 = center + u*(len/2) - v*(height/2);
//         Internal::Vec3 v2 = center + u*(len/2) + v*(height/2);
//         Internal::Vec3 v3 = center - u*(len/2) + v*(height/2);
        
//         f.vertices[0] = {v0.x, v0.y, v0.z}; f.vertices[1] = {v1.x, v1.y, v1.z};
//         f.vertices[2] = {v2.x, v2.y, v2.z}; f.vertices[3] = {v3.x, v3.y, v3.z};
//         fracs.push_back(f);
//     }
//     double xc = 500.0, yc = 250.0, zc = 10.0;
//     double offsets[3] = {-100.0, 0.0, 100.0};
//     for(int k=0; k<3; ++k) {
//         QtUItoAlgo::FractureInput f; f.id = 100 + k; f.aperture = 0.01; f.perm = 100000.0;
//         double x_curr = xc + offsets[k];
//         f.vertices[0] = {x_curr, yc - 100, zc - 50}; f.vertices[1] = {x_curr, yc + 100, zc - 50};
//         f.vertices[2] = {x_curr, yc + 100, zc + 50}; f.vertices[3] = {x_curr, yc - 100, zc + 50};
//         fracs.push_back(f);
//     }
//     sim.addFractures(fracs);

//     std::vector<QtUItoAlgo::WellParameters> wells;
//     wells.push_back({100, 100.0, 50.0});
//     wells.push_back({101, 100.0, 50.0});
//     wells.push_back({102, 100.0, 50.0});
//     sim.addWells(wells);

//     std::cout << "Exporting Geometry for Visualization..." << std::endl;
//     sim.exportGeometry(".");

//     QtUItoAlgo::SimulationParameters simParams;
//     simParams.total_time_days = 100.0;
//     simParams.initial_time_step = 0.001;
//     simParams.min_time_step = 1e-6;
//     simParams.max_time_step = 10.0;
//     sim.setSimulationParameters(simParams);
    
//     sim.runSimulation();

//     sim.exportResults(".");
    
//     std::cout << "Done. Results saved to CSV." << std::endl;
//     return 0;
// }