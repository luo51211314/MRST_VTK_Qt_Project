#pragma once

struct Mapper {
  double xmin{}, xmax{}, ymin{}, ymax{};
  int W{}, H{};
  bool flipY = false;
  bool mirrorX = false;

  void map(double x, double y, double& u, double& v) const {
  double nx = (x - xmin) / (xmax - xmin);
  double ny = (y - ymin) / (ymax - ymin);
  if (mirrorX) nx = 1.0 - nx;
  u = nx * (W - 1);
  v = ny * (H - 1);
  if (flipY) v = (H - 1) - v;
}


  // image(u,v) -> world(x,y)
  void inv_map(double u, double v, double& x, double& y) const {
    double nx = u / (W - 1);
    double ny = v / (H - 1);
    if (flipY) ny = 1.0 - ny;
    if (mirrorX) nx = 1.0 - nx;
    x = xmin + nx * (xmax - xmin);
    y = ymin + ny * (ymax - ymin);
  }
};

