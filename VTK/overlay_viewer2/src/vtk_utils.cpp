// vtk_utils.cpp
#include "vtk_utils.h"

#include <vtkCell.h>
#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>

#include <unordered_map>

void CopyCellsAsLines(
  vtkPolyData* src,
  long long cellBegin,
  long long cellEnd,
  vtkPolyData* dst)
{
  if (!src || !dst) return;

  auto outPts = vtkSmartPointer<vtkPoints>::New();
  auto outLines = vtkSmartPointer<vtkCellArray>::New();

  std::unordered_map<long long, long long> idMap;
  idMap.reserve(1024);

  auto mapPoint = [&](long long pid) -> long long {
    auto it = idMap.find(pid);
    if (it != idMap.end()) return it->second;
    double p[3];
    src->GetPoint(pid, p);
    long long nid = outPts->InsertNextPoint(p);
    idMap.emplace(pid, nid);
    return nid;
  };

  for (long long cid = cellBegin; cid < cellEnd; ++cid) {
    vtkCell* c = src->GetCell(cid);
    if (!c) continue;
    long long n = c->GetNumberOfPoints();
    if (n < 2) continue;

    for (long long i = 0; i + 1 < n; ++i) {
      long long p0 = mapPoint(c->GetPointId(i));
      long long p1 = mapPoint(c->GetPointId(i + 1));
      long long ids[2]{p0, p1};
      outLines->InsertNextCell(2, ids);
    }
  }

  dst->SetPoints(outPts);
  dst->SetLines(outLines);
  dst->Modified();
}
