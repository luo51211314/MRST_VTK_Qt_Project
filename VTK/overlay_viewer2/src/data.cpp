#include "data.h"
#include <fstream>
#include <sstream>

std::vector<std::string> split_csv_line(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  bool in_quotes = false;

  auto flush = [&]() {
    if (!cur.empty()) out.push_back(cur);
    else out.push_back(""); // 保持列对齐
    cur.clear();
  };

  for (char c : s) {
    if (c == '"') { in_quotes = !in_quotes; continue; }

    // 兼容逗号、TAB、空格分隔（不在引号内才分割）
    if (!in_quotes && (c == ',' || c == '\t' || c == ' ')) {
      // 处理连续空格/连续tab：只在 cur 非空时才 flush，避免产生大量空列
      if (!cur.empty()) flush();
      continue;
    }

    cur.push_back(c);
  }

  if (!cur.empty() || !out.empty()) flush();
  return out;
}


bool is_header_line(const std::string& line) {
  for (char c : line)
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) return true;
  return false;
}

bool load_segments(const std::string& path, std::vector<SegmentRow>& segs, Bounds2D& b) {
  std::ifstream fin(path);
  if (!fin) return false;

  std::string line;
  bool first = true;
  while (std::getline(fin, line)) {
    if (line.empty()) continue;
    if (first && is_header_line(line)) { first = false; continue; }
    first = false;

    auto cols = split_csv_line(line);
    if (cols.size() < 11) continue;

    SegmentRow r;
    r.global_id = std::stoll(cols[0]);
    r.parent_id = std::stoi(cols[1]);
    r.grid_id   = std::stoi(cols[2]);
    r.length    = std::stod(cols[3]);
    r.p1x = std::stod(cols[4]); r.p1y = std::stod(cols[5]);
    r.p2x = std::stod(cols[6]); r.p2y = std::stod(cols[7]);
    r.midx = std::stod(cols[8]); r.midy = std::stod(cols[9]);
    r.aperture = std::stod(cols[10]);

    segs.push_back(r);
    b.add(r.p1x, r.p1y); b.add(r.p2x, r.p2y);
  }
  return !segs.empty();
}

bool load_fracture_geometry(const std::string& path,
                            std::vector<FractureQuadRow>& fracs,
                            Bounds2D& b) {
  std::ifstream fin(path);
  if (!fin) return false;

  std::string line;
  bool first = true;
  while (std::getline(fin, line)) {
    if (line.empty()) continue;
    if (first && is_header_line(line)) { first = false; continue; }
    first = false;

    auto cols = split_csv_line(line);
    // id x0 y0 z0 x1 y1 z1 x2 y2 z2 x3 y3 z3
    if (cols.size() < 13) continue;

    FractureQuadRow r;
    r.id = std::stoi(cols[0]);

    r.x0 = std::stod(cols[1]);  r.y0 = std::stod(cols[2]);  r.z0 = std::stod(cols[3]);
    r.x1 = std::stod(cols[4]);  r.y1 = std::stod(cols[5]);  r.z1 = std::stod(cols[6]);
    r.x2 = std::stod(cols[7]);  r.y2 = std::stod(cols[8]);  r.z2 = std::stod(cols[9]);
    r.x3 = std::stod(cols[10]); r.y3 = std::stod(cols[11]); r.z3 = std::stod(cols[12]);

    fracs.push_back(r);

    // 2D bounds（忽略 z）
    b.add(r.x0, r.y0);
    b.add(r.x1, r.y1);
    b.add(r.x2, r.y2);
    b.add(r.x3, r.y3);
  }

  return !fracs.empty();
}


bool load_intersections(const std::string& path, std::vector<IntersectionRow>& ints) {
  std::ifstream fin(path);
  if (!fin) return false;

  std::string line;
  bool first = true;
  while (std::getline(fin, line)) {
    if (line.empty()) continue;
    if (first && is_header_line(line)) { first = false; continue; }
    first = false;

    auto cols = split_csv_line(line);
    if (cols.size() < 9) continue;

    IntersectionRow r;
    r.id = std::stoi(cols[0]);
    r.frac1_id = std::stoi(cols[1]);
    r.frac2_id = std::stoi(cols[2]);
    r.seg1_id = std::stoll(cols[3]);
    r.seg2_id = std::stoll(cols[4]);
    r.cell_id = std::stoi(cols[5]);
    r.x = std::stod(cols[6]);
    r.y = std::stod(cols[7]);

    ints.push_back(r);
  }
  return !ints.empty();
}

bool load_pressure(const std::string& path, std::vector<PressureRow>& ps, Bounds2D& b) {
  std::ifstream fin(path);
  if (!fin) return false;

  std::string line;
  bool first = true;
  while (std::getline(fin, line)) {
    if (line.empty()) continue;
    if (first && is_header_line(line)) { first = false; continue; }
    first = false;

    auto cols = split_csv_line(line);
    if (cols.size() < 3) continue;

    PressureRow r;
    r.x = std::stod(cols[0]);
    r.y = std::stod(cols[1]);
    r.p = std::stod(cols[2]);
    ps.push_back(r);

    b.add(r.x, r.y);
  }
  return !ps.empty();
}

bool load_final_field(const std::string& path, std::vector<FinalFieldRow>& ps, Bounds2D& b) {
  std::ifstream fin(path);
  if (!fin) return false;

  std::string line;
  bool first = true;
  while (std::getline(fin, line)) {
    if (line.empty()) continue;
    if (first && is_header_line(line)) { first = false; continue; }
    first = false;

    auto cols = split_csv_line(line);
    // 允许列多，但至少要 4 列：x,y,z,pressure
    if (cols.size() < 4) continue;

    FinalFieldRow r;
    r.x = std::stod(cols[0]);
    r.y = std::stod(cols[1]);
    r.z = std::stod(cols[2]);
    r.p = std::stod(cols[3]);

    ps.push_back(r);
    b.add(r.x, r.y);
  }
  return !ps.empty();
}

ViewMode gViewMode = ViewMode::Mode2D;

// 先写死路径，后续可改为 argv / config
std::string gPng2D;
std::string gPng3D;

vtkSmartPointer<vtkPNGReader> gPngReader = nullptr;
vtkSmartPointer<vtkImageActor> gImageActor = nullptr;
vtkRenderWindow* gRenderWindow = nullptr;

