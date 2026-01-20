#pragma once

#include <string>
#include <vector>
#include <limits>
#include <algorithm>

#include <vtkSmartPointer.h>
#include <vtkPNGReader.h>
#include <vtkImageActor.h>
#include <vtkRenderWindow.h>

extern std::string gPng2D;
extern std::string gPng3D;

extern vtkSmartPointer<vtkPNGReader> gPngReader;
extern vtkSmartPointer<vtkImageActor> gImageActor;
extern vtkRenderWindow* gRenderWindow;

struct SegmentRow {
  long long global_id{};
  int parent_id{};
  int grid_id{};
  double length{};
  double p1x{}, p1y{}, p2x{}, p2y{};
  double midx{}, midy{};
  double aperture{};
};

// fracture_geometry.csv: id x0 y0 z0 x1 y1 z1 x2 y2 z2 x3 y3 z3
struct FractureQuadRow {
  int id{};
  double x0{}, y0{}, z0{};
  double x1{}, y1{}, z1{};
  double x2{}, y2{}, z2{};
  double x3{}, y3{}, z3{};
};

struct IntersectionRow {
  int id{};
  int frac1_id{}, frac2_id{};
  long long seg1_id{}, seg2_id{};
  int cell_id{};
  double x{}, y{};
};

struct PressureRow {
  double x{}, y{}, p{};
};

// 师兄 final_field.csv: x,y,z,pressure
struct FinalFieldRow {
  double x{}, y{}, z{}, p{};
};

struct Bounds2D {
  double xmin = std::numeric_limits<double>::infinity();
  double xmax = -std::numeric_limits<double>::infinity();
  double ymin = std::numeric_limits<double>::infinity();
  double ymax = -std::numeric_limits<double>::infinity();

  void add(double x, double y) {
    xmin = std::min(xmin, x); xmax = std::max(xmax, x);
    ymin = std::min(ymin, y); ymax = std::max(ymax, y);
  }
};

// CSV loaders
bool load_segments(const std::string& path, std::vector<SegmentRow>& segs, Bounds2D& b);
bool load_intersections(const std::string& path, std::vector<IntersectionRow>& ints);
bool load_pressure(const std::string& path, std::vector<PressureRow>& ps, Bounds2D& b);
bool load_final_field(const std::string& path, std::vector<FinalFieldRow>& ps, Bounds2D& b);
bool load_fracture_geometry(const std::string& path, std::vector<FractureQuadRow>& fracs, Bounds2D& b);
bool load_fracture_geometry(const std::string& path,
                            std::vector<FractureQuadRow>& fracs,
                            Bounds2D& b);

// ===== View Mode =====
enum class ViewMode {
    Mode2D,
    Mode3D
};

extern ViewMode gViewMode;

// ===== Background PNG paths =====
extern std::string gPng2D;
extern std::string gPng3D;

