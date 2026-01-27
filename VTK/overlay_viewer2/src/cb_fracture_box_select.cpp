// cb_fracture_box_select.cpp
#include "cb_fracture_box_select.h"
#include "fracture_select_effect.h"

#include <vtkActor2D.h>
#include <vtkAreaPicker.h>
#include <vtkCell.h>
#include <vtkCellArray.h>
#include <vtkCoordinate.h>
#include <vtkPlanes.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkProperty2D.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkTextActor.h>

#include <algorithm>
#include <iostream>
#include <set>
#include <vector>

vtkStandardNewMacro(FractureBoxSelectCallback);

static inline bool InFrustum(vtkPlanes* frustum, const double p[3]) {
  double q[3] = {p[0], p[1], p[2]};
  return frustum && frustum->EvaluateFunction(q) <= 0.0;
}

void FractureBoxSelectCallback::EnsureBox() {
  if (boxActor) return;

  boxPts   = vtkSmartPointer<vtkPoints>::New();
  boxLines = vtkSmartPointer<vtkCellArray>::New();
  boxPoly  = vtkSmartPointer<vtkPolyData>::New();

  boxPts->SetNumberOfPoints(4);
  for (int i = 0; i < 4; ++i) boxPts->SetPoint(i, 0.0, 0.0, 0.0);

  auto addEdge = [&](vtkIdType a, vtkIdType b) {
    vtkIdType ids[2] = {a, b};
    boxLines->InsertNextCell(2, ids);
  };
  addEdge(0, 1);
  addEdge(1, 2);
  addEdge(2, 3);
  addEdge(3, 0);

  boxPoly->SetPoints(boxPts);
  boxPoly->SetLines(boxLines);

  auto mapper2d = vtkSmartPointer<vtkPolyDataMapper2D>::New();
  mapper2d->SetInputData(boxPoly);

  auto coord = vtkSmartPointer<vtkCoordinate>::New();
  coord->SetCoordinateSystemToDisplay();
  mapper2d->SetTransformCoordinate(coord);

  boxActor = vtkSmartPointer<vtkActor2D>::New();
  boxActor->SetMapper(mapper2d);
  boxActor->GetProperty()->SetColor(0.2, 1.0, 0.2);
  boxActor->GetProperty()->SetLineWidth(2.0);
  boxActor->SetVisibility(0);

  if (Renderer) Renderer->AddActor2D(boxActor);
}

void FractureBoxSelectCallback::UpdateBox(int ax0, int ay0, int ax1, int ay1) {
  EnsureBox();

  int xmin = std::min(ax0, ax1);
  int xmax = std::max(ax0, ax1);
  int ymin = std::min(ay0, ay1);
  int ymax = std::max(ay0, ay1);

  boxPts->SetPoint(0, xmin, ymin, 0);
  boxPts->SetPoint(1, xmax, ymin, 0);
  boxPts->SetPoint(2, xmax, ymax, 0);
  boxPts->SetPoint(3, xmin, ymax, 0);

  boxPts->Modified();
  boxPoly->Modified();
}

void FractureBoxSelectCallback::ShowBox(bool on) {
  EnsureBox();
  boxActor->SetVisibility(on ? 1 : 0);
}

void FractureBoxSelectCallback::BuildSelectionFromRect(int ax0, int ay0, int ax1, int ay1) {
  if (!Renderer || !FracturePoly || FracturePoly->GetNumberOfCells() <= 0) {
    if (Effect) {
      auto* iren = vtkRenderWindowInteractor::SafeDownCast(Renderer->GetRenderWindow()->GetInteractor());
      Effect->Clear(iren);
    }
    return;
  }

  int xmin = std::min(ax0, ax1);
  int xmax = std::max(ax0, ax1);
  int ymin = std::min(ay0, ay1);
  int ymax = std::max(ay0, ay1);

  auto areaPicker = vtkSmartPointer<vtkAreaPicker>::New();
  areaPicker->AreaPick(xmin, ymin, xmax, ymax, Renderer);

  vtkPlanes* frustum = areaPicker->GetFrustum();
  if (!frustum) {
    if (Effect) {
      auto* iren = vtkRenderWindowInteractor::SafeDownCast(Renderer->GetRenderWindow()->GetInteractor());
      Effect->Clear(iren);
    }
    return;
  }

  const vtkIdType nCells = FracturePoly->GetNumberOfCells();
  const int cpf = std::max(1, CellsPerFracture);
  const vtkIdType nFractures = nCells / cpf;

  std::set<int> selectedFractureIds;

  for (vtkIdType fid = 0; fid < nFractures; ++fid) {
    bool hitThisFrac = false;

    for (int k = 0; k < cpf; ++k) {
      vtkIdType cid = fid * cpf + k;
      if (cid < 0 || cid >= nCells) continue;

      vtkCell* cell = FracturePoly->GetCell(cid);
      if (!cell) continue;

      vtkIdList* ptIds = cell->GetPointIds();
      for (vtkIdType j = 0; j < ptIds->GetNumberOfIds(); ++j) {
        double p[3];
        FracturePoly->GetPoint(ptIds->GetId(j), p);
        if (InFrustum(frustum, p)) {
          hitThisFrac = true;
          break;
        }
      }
      if (hitThisFrac) break;
    }

    if (hitThisFrac) {
      selectedFractureIds.insert((int)fid);
    }
  }

  if (!Effect) {
    std::cout << "[Box] Effect is null" << std::endl;
    return;
  }

  auto* iren = vtkRenderWindowInteractor::SafeDownCast(Renderer->GetRenderWindow()->GetInteractor());

  Effect->Renderer = Renderer;
  Effect->FracturePoly = FracturePoly;
  Effect->CellsPerFracture = cpf;
  Effect->HudText = HudText; // 左上角HUD

  if (selectedFractureIds.empty()) {
    Effect->Clear(iren);
  } else {
    Effect->ApplyBox(iren, selectedFractureIds, xmin, ymin, xmax, ymax);
  }

  std::cout << "[Box] selection done" << std::endl;
}

void FractureBoxSelectCallback::Execute(vtkObject* caller, unsigned long eventId, void*) {
auto* iren = vtkRenderWindowInteractor::SafeDownCast(caller);
if (!iren || !Renderer) return;

// ✅ Fracture Layer OFF：Box Select 完全禁用
if (!FractureActor || FractureActor->GetPickable() == 0 || FractureActor->GetVisibility() == 0) {
  // 关掉绿框 + 复位状态，避免残留
  ShowBox(false);
  bDown = false;
  dragging = false;
  boxLatched = false;

  if (Effect) Effect->Clear(iren); // 清高亮/HUD/tooltip（兜底）
  return;
}

  if (!iren) return;

  if (eventId == vtkCommand::KeyPressEvent || eventId == vtkCommand::KeyReleaseEvent) {
    const char* key = iren->GetKeySym();
    if (key && (key[0] == 'b' || key[0] == 'B') && key[1] == '\0') {
      bDown = (eventId == vtkCommand::KeyPressEvent);
      std::cout << "[Box] B key, bDown=" << bDown
                << " dragging=" << dragging
                << " latched=" << boxLatched << std::endl;
    }
    return;
  }

  if (eventId == vtkCommand::LeftButtonPressEvent) {
    if (!bDown) return;

    boxLatched = true;
    dragging = true;

    int* pos = iren->GetEventPosition();
    x0 = x1 = pos[0];
    y0 = y1 = pos[1];

    UpdateBox(x0, y0, x1, y1);
    ShowBox(true);

    std::cout << "[Box] LPress start (" << x0 << "," << y0 << ")" << std::endl;

    this->AbortFlagOn();
    iren->Render();
    return;
  }

  if (eventId == vtkCommand::MouseMoveEvent) {
    if (!dragging || !boxLatched) return;

    int* pos = iren->GetEventPosition();
    x1 = pos[0];
    y1 = pos[1];

    UpdateBox(x0, y0, x1, y1);

    this->AbortFlagOn();
    iren->Render();
    return;
  }

  if (eventId == vtkCommand::LeftButtonReleaseEvent) {
    if (!dragging || !boxLatched) return;

    int* pos = iren->GetEventPosition();
    x1 = pos[0];
    y1 = pos[1];

    std::cout << "[Box] LRelease end (" << x1 << "," << y1 << ")" << std::endl;

    dragging = false;
    boxLatched = false;

    ShowBox(false);
    BuildSelectionFromRect(x0, y0, x1, y1);

    this->AbortFlagOn();
    iren->Render();
    return;
  }
}

