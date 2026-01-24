// cb_fracture_pick.cpp
#include "cb_fracture_pick.h"
#include "fracture_select_effect.h"

#include <vtkActor.h>
#include <vtkCellPicker.h>
#include <vtkPolyData.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkTextActor.h>

#include <sstream>

// ★关键：必须 include 这个，否则 vtkStandardNewMacro 可能会炸
#include <vtkObjectFactory.h>

vtkStandardNewMacro(FracturePickCallback);

void FracturePickCallback::ShowHud(vtkRenderWindowInteractor* iren, const std::string& text) {
  if (!HudText) return;
  HudText->SetInput(text.c_str());
  HudText->SetVisibility(1);
  if (iren) iren->GetRenderWindow()->Render();
}

void FracturePickCallback::ClearSelection(vtkRenderWindowInteractor* iren) {
  LastFractureIndex = -1;

  // 如果有统一效果，用它清空（高亮+HUD）
  if (Effect) {
    Effect->Renderer = Renderer;
    Effect->FracturePoly = FracturePoly;
    Effect->CellsPerFracture = CellsPerFracture;
    Effect->HudText = HudText;
    Effect->Clear(iren);
    return;
  }

  // 没有 Effect 的兜底
  if (HudText) {
    HudText->SetInput("");
    HudText->SetVisibility(0);
  }
  if (iren) iren->GetRenderWindow()->Render();
}

void FracturePickCallback::Execute(vtkObject* caller, unsigned long eventId, void*) {
  if (eventId != vtkCommand::LeftButtonPressEvent) return;

  auto* iren = vtkRenderWindowInteractor::SafeDownCast(caller);
  if (!iren || !Renderer || !Picker) return;

  if (OnlyWhen3D && Is3DPtr && !(*Is3DPtr)) return;

  if (!FractureActor || !FracturePoly) {
    ShowHud(iren, "Pick: FractureActor/Poly is NULL");
    return;
  }

  int* pos = iren->GetEventPosition();
  int x = pos[0], y = pos[1];

  Picker->InitializePickList();
  Picker->AddPickList(FractureActor);
  Picker->PickFromListOn();

  Picker->Pick(x, y, 0, Renderer);

  auto cellId = Picker->GetCellId();
  if (cellId < 0) {
    ClearSelection(iren);
    return;
  }

  int fractureIndex = (CellsPerFracture > 0) ? int(cellId / CellsPerFracture) : int(cellId);

  // ★统一效果：用同一套“黄线高亮 + 左上角HUD”
  if (Effect) {
    Effect->Renderer = Renderer;
    Effect->FracturePoly = FracturePoly;
    Effect->CellsPerFracture = CellsPerFracture;
    Effect->HudText = HudText;

    // 如果你想避免重复构建（点同一个裂缝不重刷），保留 LastFractureIndex
    if (fractureIndex != LastFractureIndex) {
      Effect->ApplyPick(iren, fractureIndex, (long long)cellId);
      LastFractureIndex = fractureIndex;
    }
    return;
  }

  // 没有 Effect 的兜底输出（一般不会走到）
  std::ostringstream ss;
  ss << "Pick OK\ncellId=" << cellId << "\nfractureIndex=" << fractureIndex;
  ShowHud(iren, ss.str());
}

