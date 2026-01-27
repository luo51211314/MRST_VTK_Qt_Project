#include "ui_layer_button.h"

#include <vtkActor2D.h>
#include <vtkCellArray.h>
#include <vtkCoordinate.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkPoints.h>
#include <vtkProperty2D.h>
#include <vtkRenderer.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>

void UiLayerButton::Build(vtkRenderer* renderer) {
  // ---- rectangle poly (display coords) ----
  auto pts = vtkSmartPointer<vtkPoints>::New();
  pts->SetNumberOfPoints(4);
  pts->SetPoint(0, X,     Y,     0);
  pts->SetPoint(1, X + W, Y,     0);
  pts->SetPoint(2, X + W, Y + H, 0);
  pts->SetPoint(3, X,     Y + H, 0);

  auto poly = vtkSmartPointer<vtkPolyData>::New();
  poly->SetPoints(pts);

  auto cells = vtkSmartPointer<vtkCellArray>::New();
  vtkIdType ids[4] = {0, 1, 2, 3};
  cells->InsertNextCell(4, ids);
  poly->SetPolys(cells);

  auto coord = vtkSmartPointer<vtkCoordinate>::New();
  coord->SetCoordinateSystemToDisplay();

  auto mapper2d = vtkSmartPointer<vtkPolyDataMapper2D>::New();
  mapper2d->SetInputData(poly);
  mapper2d->SetTransformCoordinate(coord);

  RectActor = vtkSmartPointer<vtkActor2D>::New();
  RectActor->SetMapper(mapper2d);
  RectActor->GetProperty()->SetOpacity(0.65);

  // ---- text ----
  TextActor = vtkSmartPointer<vtkTextActor>::New();
  TextActor->SetPosition(X + 10, Y + 8);
  auto* tp = TextActor->GetTextProperty();
  tp->SetFontSize(16);
  tp->SetBold(1);
  tp->SetColor(1.0, 1.0, 1.0);

  UpdateText();
  SetChecked(Checked);

  renderer->AddActor2D(RectActor);
  renderer->AddActor2D(TextActor);
}

bool UiLayerButton::Hit(int mx, int my) const {
  return (mx >= X && mx <= X + W && my >= Y && my <= Y + H);
}

void UiLayerButton::SetChecked(bool checked) {
  Checked = checked;
  // simple color scheme: greenish when ON, gray when OFF
  if (RectActor) {
    if (Checked) RectActor->GetProperty()->SetColor(0.2, 0.6, 0.2);
    else         RectActor->GetProperty()->SetColor(0.35, 0.35, 0.35);
  }
  UpdateText();
}

void UiLayerButton::Toggle() {
  SetChecked(!Checked);
}

void UiLayerButton::UpdateText() {
  if (!TextActor) return;
  std::string s = Label + ": " + (Checked ? "ON" : "OFF");
  TextActor->SetInput(s.c_str());
}
