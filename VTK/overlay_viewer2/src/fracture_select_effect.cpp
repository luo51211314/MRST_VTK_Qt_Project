#include "fracture_select_effect.h"
#include "vtk_utils.h"   // CopyCellsAsLines(...)

#include <vtkActor.h>
#include <vtkAppendPolyData.h>
#include <vtkCell.h>
#include <vtkCellPicker.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkWindow.h>

#include <algorithm>
#include <cfloat>
#include <iomanip>
#include <sstream>

void FractureSelectEffect::EnsureHighlightObjects() {
  if (HighlightActor && HighlightPoly) return;

  HighlightPoly = vtkSmartPointer<vtkPolyData>::New();

  auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  mapper->SetInputData(HighlightPoly);

  HighlightActor = vtkSmartPointer<vtkActor>::New();
  HighlightActor->SetMapper(mapper);
  HighlightActor->GetProperty()->SetColor(1.0, 1.0, 0.0);
  HighlightActor->GetProperty()->SetLineWidth(6.0);
  HighlightActor->GetProperty()->SetOpacity(1.0);
  HighlightActor->PickableOff();
  HighlightActor->SetVisibility(0);

  if (Renderer) Renderer->AddActor(HighlightActor);
}

void FractureSelectEffect::ShowHudTopLeft(vtkRenderWindowInteractor* iren, const std::string& text) {
  if (!HudText || !iren) return;

  // 强制可见：黄字黑底
  auto* tp = HudText->GetTextProperty();
  tp->SetFontSize(22);
  tp->SetColor(1.0, 1.0, 0.0);
  tp->SetBold(1);
  tp->SetBackgroundColor(0.0, 0.0, 0.0);
  tp->SetBackgroundOpacity(0.65);
  tp->SetJustificationToLeft();
  tp->SetVerticalJustificationToTop();

  HudText->SetInput(text.c_str());
  HudText->SetVisibility(1);

  int* sz = iren->GetRenderWindow()->GetSize();
  int h = sz ? sz[1] : 900;
  int margin = 10;

  // 顶对齐：y 用 h - margin，永远在屏幕内
  HudText->SetDisplayPosition(margin, h - margin);
  HudText->Modified();

  iren->GetRenderWindow()->Render();
}

void FractureSelectEffect::HideHudTopLeft() {
  if (!HudText) return;
  HudText->SetInput("");
  HudText->SetVisibility(0);
}

void FractureSelectEffect::ShowTooltip(vtkRenderWindowInteractor* iren, int x, int y, const std::string& text) {
  if (!TooltipText || !iren) return;

  TooltipText->SetInput(text.c_str());
  TooltipText->SetVisibility(1);

  int ox = 18;
  int oy = -18;
  TooltipText->SetDisplayPosition(x + ox, y + oy);

  iren->GetRenderWindow()->Render();
}

void FractureSelectEffect::HideTooltip() {
  if (!TooltipText) return;
  TooltipText->SetInput("");
  TooltipText->SetVisibility(0);
}

void FractureSelectEffect::Clear(vtkRenderWindowInteractor* iren) {
  EnsureHighlightObjects();

  SelectedFractureIds.clear();

  if (HighlightPoly) {
    HighlightPoly->Initialize();
    HighlightPoly->Modified();
  }
  if (HighlightActor) HighlightActor->SetVisibility(0);

  HideHudTopLeft();
  HideTooltip();

  if (iren) iren->GetRenderWindow()->Render();
}

bool FractureSelectEffect::GetFractureBounds(int fractureIndex, double bounds6[6]) const {
  if (!FracturePoly || fractureIndex < 0) return false;

  long long nCells = FracturePoly->GetNumberOfCells();
  long long begin = (long long)fractureIndex * (long long)CellsPerFracture;
  long long end = std::min(begin + (long long)CellsPerFracture, nCells);
  if (begin < 0 || begin >= nCells || begin >= end) return false;

  double xmin =  DBL_MAX, ymin =  DBL_MAX, zmin =  DBL_MAX;
  double xmax = -DBL_MAX, ymax = -DBL_MAX, zmax = -DBL_MAX;

  for (long long cid = begin; cid < end; ++cid) {
    vtkCell* cell = FracturePoly->GetCell((vtkIdType)cid);
    if (!cell) continue;

    vtkPoints* pts = cell->GetPoints();
    if (!pts) continue;

    vtkIdType n = pts->GetNumberOfPoints();
    for (vtkIdType i = 0; i < n; ++i) {
      double p[3];
      pts->GetPoint(i, p);
      xmin = std::min(xmin, p[0]); ymin = std::min(ymin, p[1]); zmin = std::min(zmin, p[2]);
      xmax = std::max(xmax, p[0]); ymax = std::max(ymax, p[1]); zmax = std::max(zmax, p[2]);
    }
  }

  if (xmin > xmax) return false;

  bounds6[0] = xmin; bounds6[1] = xmax;
  bounds6[2] = ymin; bounds6[3] = ymax;
  bounds6[4] = zmin; bounds6[5] = zmax;
  return true;
}

std::string FractureSelectEffect::MakePickInfoText(int fractureIndex, long long cellId) const {
  std::ostringstream ss;
  ss << "SELECT (PICK)\n";
  ss << "fractureIndex: " << fractureIndex << "\n";
  ss << "cellId: " << cellId << "\n";

  double b[6];
  if (GetFractureBounds(fractureIndex, b)) {
    ss << std::fixed << std::setprecision(3);
    ss << "BBox X: [" << b[0] << ", " << b[1] << "]\n";
    ss << "BBox Y: [" << b[2] << ", " << b[3] << "]\n";
    ss << "BBox Z: [" << b[4] << ", " << b[5] << "]\n";
  } else {
    ss << "BBox: (unavailable)\n";
  }
  return ss.str();
}

std::string FractureSelectEffect::MakeBoxInfoText(const std::set<int>& fractureIndices,
                                                  int xmin, int ymin, int xmax, int ymax) const {
  std::ostringstream ss;
  ss << "SELECT (BOX)\n";
  ss << "fractures: " << (int)fractureIndices.size() << "\n";
  ss << "screen rect: (" << xmin << "," << ymin << ") - (" << xmax << "," << ymax << ")\n";

  double xminW =  DBL_MAX, yminW =  DBL_MAX, zminW =  DBL_MAX;
  double xmaxW = -DBL_MAX, ymaxW = -DBL_MAX, zmaxW = -DBL_MAX;

  int valid = 0;
  for (int fid : fractureIndices) {
    double b[6];
    if (!GetFractureBounds(fid, b)) continue;
    valid++;
    xminW = std::min(xminW, b[0]); xmaxW = std::max(xmaxW, b[1]);
    yminW = std::min(yminW, b[2]); ymaxW = std::max(ymaxW, b[3]);
    zminW = std::min(zminW, b[4]); zmaxW = std::max(zmaxW, b[5]);
  }

  if (valid > 0) {
    ss << std::fixed << std::setprecision(3);
    ss << "WorldBBox X: [" << xminW << ", " << xmaxW << "]\n";
    ss << "WorldBBox Y: [" << yminW << ", " << ymaxW << "]\n";
    ss << "WorldBBox Z: [" << zminW << ", " << zmaxW << "]\n";
  } else {
    ss << "WorldBBox: (unavailable)\n";
  }

  ss << "IDs (first 10): ";
  int c = 0;
  for (int fid : fractureIndices) {
    ss << fid << " ";
    if (++c >= 10) break;
  }
  ss << "\n";
  return ss.str();
}

std::string FractureSelectEffect::MakeHoverInfoText(int fractureIndex) const {
  std::ostringstream ss;
  ss << "Fracture: " << fractureIndex << "\n";
  double b[6];
  if (GetFractureBounds(fractureIndex, b)) {
    ss << std::fixed << std::setprecision(3);
    ss << "X:[" << b[0] << "," << b[1] << "]\n";
    ss << "Y:[" << b[2] << "," << b[3] << "]\n";
    ss << "Z:[" << b[4] << "," << b[5] << "]";
  } else {
    ss << "(no bounds)";
  }
  return ss.str();
}

void FractureSelectEffect::BuildHighlightSingle(int fractureIndex) {
  EnsureHighlightObjects();
  if (!FracturePoly || fractureIndex < 0) return;

  long long nCells = FracturePoly->GetNumberOfCells();
  long long begin = (long long)fractureIndex * (long long)CellsPerFracture;
  long long end = std::min(begin + (long long)CellsPerFracture, nCells);
  if (begin < 0 || begin >= nCells || begin >= end) return;

  CopyCellsAsLines(FracturePoly, begin, end, HighlightPoly);
  HighlightActor->SetVisibility(1);
}

void FractureSelectEffect::BuildHighlightMulti(const std::set<int>& fractureIndices) {
  EnsureHighlightObjects();
  if (!FracturePoly || fractureIndices.empty()) return;

  auto append = vtkSmartPointer<vtkAppendPolyData>::New();

  long long nCells = FracturePoly->GetNumberOfCells();
  for (int fid : fractureIndices) {
    if (fid < 0) continue;

    long long begin = (long long)fid * (long long)CellsPerFracture;
    long long end = std::min(begin + (long long)CellsPerFracture, nCells);
    if (begin < 0 || begin >= nCells || begin >= end) continue;

    auto tmp = vtkSmartPointer<vtkPolyData>::New();
    CopyCellsAsLines(FracturePoly, begin, end, tmp);
    append->AddInputData(tmp);
  }

  append->Update();
  HighlightPoly->ShallowCopy(append->GetOutput());
  HighlightPoly->Modified();
  HighlightActor->SetVisibility(1);
}

void FractureSelectEffect::ApplyPick(vtkRenderWindowInteractor* iren, int fractureIndex, long long cellId) {
  if (!iren) return;
  if (!Renderer || !FracturePoly) { Clear(iren); return; }

  Clear(nullptr);

  SelectedFractureIds.clear();
  if (fractureIndex >= 0) SelectedFractureIds.insert(fractureIndex);

  BuildHighlightSingle(fractureIndex);
  ShowHudTopLeft(iren, MakePickInfoText(fractureIndex, cellId));
}

void FractureSelectEffect::ApplyBox(vtkRenderWindowInteractor* iren,
                                   const std::set<int>& fractureIndices,
                                   int xmin, int ymin, int xmax, int ymax) {
  if (!iren) return;
  if (!Renderer || !FracturePoly) { Clear(iren); return; }

  Clear(nullptr);

  if (fractureIndices.empty()) {
    Clear(iren);
    return;
  }

  SelectedFractureIds = fractureIndices;
  BuildHighlightMulti(fractureIndices);
  ShowHudTopLeft(iren, MakeBoxInfoText(fractureIndices, xmin, ymin, xmax, ymax));
}

void FractureSelectEffect::UpdateHoverTooltip(vtkRenderWindowInteractor* iren,
                                             vtkActor* fractureActor,
                                             int mouseX, int mouseY) {
  if (!iren || !Renderer || !FracturePoly || !fractureActor || !TooltipText) return;

  if (SelectedFractureIds.empty()) {
    HideTooltip();
    return;
  }

  auto picker = vtkSmartPointer<vtkCellPicker>::New();
  picker->SetTolerance(0.005);
  picker->PickFromListOn();
  picker->InitializePickList();
  picker->AddPickList(fractureActor);

  int ok = picker->Pick(mouseX, mouseY, 0, Renderer) ? 1 : 0;
  if (!ok) { HideTooltip(); return; }

  vtkIdType cellId = picker->GetCellId();
  if (cellId < 0) { HideTooltip(); return; }

  int fractureIndex = (int)((long long)cellId / (long long)CellsPerFracture);

  if (SelectedFractureIds.find(fractureIndex) == SelectedFractureIds.end()) {
    HideTooltip();
    return;
  }

  ShowTooltip(iren, mouseX, mouseY, MakeHoverInfoText(fractureIndex));
}

