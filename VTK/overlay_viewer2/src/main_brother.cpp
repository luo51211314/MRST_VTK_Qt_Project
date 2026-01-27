#include "shared.h"
#include "cb_fracture_pick.h"
#include "cb_fracture_box_select.h"
#include "style_lockable.h"
#include "cb_camera_lock_key.h"
#include "cb_mouse_move_coord.h"
#include "cb_fracture_hover_tooltip.h"
#include "ui_layer_button.h"
#include "layer_ui.h"
#include "fracture_select_effect.h"

#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkCommand.h>

#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkCellPicker.h>

#include <vtkActorCollection.h>
#include <vtkActor.h>
#include <vtkMapper.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>

#include <vtkPropCollection.h>
#include <vtkProp3D.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>

// ============================
// Actor 查找：fracture / well
// ============================

static bool PolyLooksLikeFracture(vtkPolyData* pd) {
  if (!pd) return false;
  vtkIdType nLines = (pd->GetLines() ? pd->GetLines()->GetNumberOfCells() : 0);
  vtkIdType nPolys = (pd->GetPolys() ? pd->GetPolys()->GetNumberOfCells() : 0);
  return (nLines > 0 && nLines >= nPolys);
}

static vtkActor* FindFractureActor(vtkRenderer* renderer, vtkPolyData*& outPoly) {
  outPoly = nullptr;
  if (!renderer) return nullptr;

  vtkActor* best = nullptr;
  vtkPolyData* bestPoly = nullptr;
  vtkIdType bestLineCells = -1;

  auto* actors = renderer->GetActors();
  actors->InitTraversal();

  for (vtkIdType i = 0; i < actors->GetNumberOfItems(); ++i) {
    vtkActor* a = actors->GetNextActor();
    if (!a || !a->GetMapper()) continue;

    vtkPolyData* pd = vtkPolyData::SafeDownCast(a->GetMapper()->GetInput());
    if (!pd) continue;
    if (!PolyLooksLikeFracture(pd)) continue;

    vtkIdType nLines = (pd->GetLines() ? pd->GetLines()->GetNumberOfCells() : 0);
    if (nLines > bestLineCells) {
      bestLineCells = nLines;
      best = a;
      bestPoly = pd;
    }
  }

  // fallback：拿 cells 最多的
  if (!best) {
    vtkActor* fallback = nullptr;
    vtkPolyData* fallbackPoly = nullptr;
    vtkIdType fallbackCells = -1;

    actors->InitTraversal();
    for (vtkIdType i = 0; i < actors->GetNumberOfItems(); ++i) {
      vtkActor* a = actors->GetNextActor();
      if (!a || !a->GetMapper()) continue;

      vtkPolyData* pd = vtkPolyData::SafeDownCast(a->GetMapper()->GetInput());
      if (!pd) continue;

      vtkIdType nCells = pd->GetNumberOfCells();
      if (nCells > fallbackCells) {
        fallbackCells = nCells;
        fallback = a;
        fallbackPoly = pd;
      }
    }
    best = fallback;
    bestPoly = fallbackPoly;
  }

  outPoly = bestPoly;
  return best;
}

// well actor：根据 shared::drawWells 的特征（红色、线宽更粗、polydata lines）
static vtkActor* FindWellActor(vtkRenderer* renderer, vtkPolyData*& outPoly) {
  outPoly = nullptr;
  if (!renderer) return nullptr;

  vtkActor* best = nullptr;
  vtkPolyData* bestPoly = nullptr;

  auto* actors = renderer->GetActors();
  actors->InitTraversal();

  for (vtkIdType i = 0; i < actors->GetNumberOfItems(); ++i) {
    vtkActor* a = actors->GetNextActor();
    if (!a || !a->GetMapper()) continue;

    vtkPolyData* pd = vtkPolyData::SafeDownCast(a->GetMapper()->GetInput());
    if (!pd) continue;

    vtkIdType nLines = (pd->GetLines() ? pd->GetLines()->GetNumberOfCells() : 0);
    if (nLines <= 0) continue;

    double c[3] = {0, 0, 0};
    a->GetProperty()->GetColor(c);
    double lw = a->GetProperty()->GetLineWidth();

    bool looksRed = (c[0] > 0.8 && c[1] < 0.3 && c[2] < 0.3);
    bool looksThick = (lw >= 2.5);

    if (looksRed && looksThick) {
      best = a;
      bestPoly = pd;
      break;  // 够确定
    }
  }

  outPoly = bestPoly;
  return best;
}

// ============================
// Well Pick Callback
// ============================
class WellPickCallback : public vtkCommand {
public:
  static WellPickCallback* New() { return new WellPickCallback(); }

  vtkSmartPointer<vtkCellPicker> Picker;
  vtkRenderer* Renderer = nullptr;
  vtkActor* WellActor = nullptr;
  vtkPolyData* WellPoly = nullptr;

  const std::vector<WellInfo>* Wells = nullptr;
  vtkTextActor* HudText = nullptr;

  FractureSelectEffect* FractureEffect = nullptr;  // 点井时清掉裂缝高亮（可选）

  void Execute(vtkObject* caller, unsigned long eventId, void* /*callData*/) override {
    if (eventId != vtkCommand::LeftButtonPressEvent) return;
    if (!Picker || !Renderer || !WellActor || !WellPoly || !Wells || !HudText) return;

    auto* iren = vtkRenderWindowInteractor::SafeDownCast(caller);
    if (!iren) return;

    int x = 0, y = 0;
    iren->GetEventPosition(x, y);

    // ✅ 关键：Other Layer OFF / Actor 不可拾取时，禁止井交互
    if (WellActor->GetPickable() == 0 || WellActor->GetVisibility() == 0) {
      // 可选：点到任何地方都不让井 HUD 残留
      HudText->SetInput("");
      HudText->SetVisibility(0);
      return;
    }

    // 只 pick well actor，避免误中 fracture/field
    Picker->PickFromListOn();
    Picker->InitializePickList();
    Picker->AddPickList(WellActor);

    int ok = Picker->Pick(x, y, 0.0, Renderer);
    if (!ok) {
      // ✅ 点空白：清 HUD（避免残留）
      HudText->SetInput("");
      HudText->SetVisibility(0);
      return;
    }

    vtkActor* picked = vtkActor::SafeDownCast(Picker->GetViewProp());
    if (picked != WellActor) return;

    vtkIdType cellId = Picker->GetCellId();
    if (cellId < 0) return;

    // shared::drawWells：每口井画 2 条线（水平+垂直）
    int wellIndex = static_cast<int>(cellId / 2);
    if (wellIndex < 0 || wellIndex >= static_cast<int>(Wells->size())) return;

    if (FractureEffect) {
      FractureEffect->Clear(iren);
    }

    const WellInfo& w = (*Wells)[wellIndex];

    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(3);
    oss << "WELL PICK\n"
        << "well_id=" << w.well_id << "  node_idx=" << w.node_idx << "\n"
        << "x=" << w.x << "  y=" << w.y << "  z=" << w.z << "\n"
        << "WI=" << w.WI << "  P_bhp=" << w.P_bhp;

    HudText->SetInput(oss.str().c_str());
    HudText->SetVisibility(1);

    // 吃掉事件，防止 fracture pick 再执行把 HUD 覆盖回 fracture
    this->AbortFlagOn();

    if (iren->GetRenderWindow()) iren->GetRenderWindow()->Render();
  }
};

int main(int argc, char** argv) {
  try {
    std::string configFile = "./config.json";
    if (argc > 1) configFile = argv[1];

    std::string gridFile, fieldFile, fractureFile, wellFile;
    if (!readConfig(configFile, gridFile, fieldFile, fractureFile, wellFile)) return 1;

    GridInfo gridInfo = readGridInfo(gridFile);
    FieldData fieldData = readFieldData(fieldFile);
    auto fractures = readFractures(fractureFile);
    auto wells = readWells(wellFile);

    auto renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->SetBackground(0.1, 0.1, 0.1);

    auto renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->AddRenderer(renderer);
    renderWindow->SetSize(1200, 900);

    auto iren = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    iren->SetRenderWindow(renderWindow);

    // 固定 3D 交互风格
    auto style3D = vtkSmartPointer<LockableTrackballStyle>::New();
    style3D->SetCameraLocked(false);
    iren->SetInteractorStyle(style3D);

    auto lut = createLookupTable(fieldData.p_min, fieldData.p_max);

    // shared 渲染：不动
    createSolidBlockScene(renderer, fieldData, lut);
    drawFractures(renderer, fractures, 1.0);
    drawWells(renderer, wells, 1.0);

    double cx = gridInfo.Lx / 2.0;
    double cy = gridInfo.Ly / 2.0;
    double cz = gridInfo.Lz / 2.0;
    double maxDim = std::max({gridInfo.Lx, gridInfo.Ly, gridInfo.Lz});
    setupCamera(renderer, cx, cy, cz, maxDim, false);

    // ===== Tooltip（鼠标 hover 到高亮裂缝时显示）=====
    auto tooltipHud = vtkSmartPointer<vtkTextActor>::New();
    tooltipHud->SetInput("");
    tooltipHud->GetTextProperty()->SetFontSize(16);
    tooltipHud->GetTextProperty()->SetColor(0.9, 0.9, 1.0);
    tooltipHud->SetVisibility(0);
    renderer->AddActor2D(tooltipHud);

    // ===== Camera lock HUD（右下角）=====
    auto lockHud = vtkSmartPointer<vtkTextActor>::New();
    lockHud->SetInput("CAM: UNLOCKED (press L to lock)");
    lockHud->GetTextProperty()->SetFontSize(16);
    lockHud->GetTextProperty()->SetColor(0.9, 0.7, 0.9);
    lockHud->SetVisibility(1);
    renderer->AddActor2D(lockHud);

    // ===== Select HUD（左上角：Pick/Box/Well 共用）=====
    auto selectHud = vtkSmartPointer<vtkTextActor>::New();
    selectHud->SetInput("");
    auto* stp = selectHud->GetTextProperty();
    stp->SetFontSize(22);
    stp->SetColor(1.0, 1.0, 0.0);
    stp->SetBold(1);
    stp->SetBackgroundColor(0.0, 0.0, 0.0);
    stp->SetBackgroundOpacity(0.65);
    stp->SetJustificationToLeft();
    stp->SetVerticalJustificationToTop();
    selectHud->SetVisibility(0);
    renderer->AddActor2D(selectHud);

    // ===== Coord HUD（鼠标移动显示世界坐标）=====
    auto coordHud = vtkSmartPointer<vtkTextActor>::New();
    coordHud->SetInput("Mouse World XYZ:\n(no hit)");
    coordHud->SetPosition(20, 130);
    coordHud->GetTextProperty()->SetFontSize(16);
    coordHud->GetTextProperty()->SetColor(0.6, 1.0, 0.6);
    coordHud->SetVisibility(1);
    renderer->AddActor2D(coordHud);

    // 相机锁定键
    auto camKeyCb = vtkSmartPointer<CameraLockKeyCallback>::New();
    camKeyCb->Style = style3D;
    camKeyCb->LockHud = lockHud;
    camKeyCb->ToggleKeyLower = 'l';
    iren->AddObserver(vtkCommand::KeyPressEvent, camKeyCb, 10.0);

    // 找 fracture actor/poly
    vtkPolyData* fracturePoly = nullptr;
    vtkActor* fractureActor = FindFractureActor(renderer, fracturePoly);
    std::cout << "[FindFractureActor] actor=" << (fractureActor ? "YES" : "NO")
              << " poly=" << (fracturePoly ? "YES" : "NO")
              << " cells=" << (fracturePoly ? fracturePoly->GetNumberOfCells() : 0)
              << std::endl;

    if (!fractureActor || !fracturePoly) {
      std::cerr << "ERROR: Cannot find fracture actor/poly.\n";
    }

    // 找 well actor/poly
    vtkPolyData* wellPoly = nullptr;
    vtkActor* wellActor = FindWellActor(renderer, wellPoly);
    std::cout << "[FindWellActor] actor=" << (wellActor ? "YES" : "NO")
              << " poly=" << (wellPoly ? "YES" : "NO")
              << " cells=" << (wellPoly ? wellPoly->GetNumberOfCells() : 0)
              << std::endl;

    if (!wellActor || !wellPoly) {
      std::cerr << "WARN: Cannot find well actor/poly (well pick will be disabled).\n";
    }

    // ★统一高亮+HUD效果对象（Pick/Box 共用）
    auto effect = std::make_shared<FractureSelectEffect>();
    effect->Renderer = renderer;
    effect->FracturePoly = fracturePoly;
    effect->CellsPerFracture = 4;
    effect->HudText = selectHud;
    effect->TooltipText = tooltipHud;

    // Layer UI（你已在 layer_ui 里处理 fracture/off -> clear + pickable）
    auto layerUI = InstallLayerUI(
      iren,
      renderer,
      fractureActor,
      effect.get()
    );

    // Well pick callback（优先于 fracture pick，低于 box）
    if (wellActor && wellPoly) {
      auto wellPicker = vtkSmartPointer<vtkCellPicker>::New();
      wellPicker->SetTolerance(0.01);

      auto wellPickCb = vtkSmartPointer<WellPickCallback>::New();
      wellPickCb->Picker = wellPicker;
      wellPickCb->Renderer = renderer;
      wellPickCb->WellActor = wellActor;
      wellPickCb->WellPoly = wellPoly;
      wellPickCb->Wells = &wells;
      wellPickCb->HudText = selectHud;
      wellPickCb->FractureEffect = effect.get();

      // 优先级：box(5.0) > well(3.0) > fracture pick(1.0)
      iren->AddObserver(vtkCommand::LeftButtonPressEvent, wellPickCb, 3.0);
    }

    // Pick callback（fracture）
    auto picker = vtkSmartPointer<vtkCellPicker>::New();
    picker->SetTolerance(0.005);

    auto pickCb = vtkSmartPointer<FracturePickCallback>::New();
    pickCb->Picker = picker;
    pickCb->Renderer = renderer;
    pickCb->FractureActor = fractureActor;
    pickCb->FracturePoly = fracturePoly;
    pickCb->CellsPerFracture = 4;
    pickCb->OnlyWhen3D = false;
    pickCb->Is3DPtr = nullptr;
    pickCb->Effect = effect.get();
    pickCb->HudText = selectHud;

    // Box callback（B + 左键拖拽）
    auto boxCb = vtkSmartPointer<FractureBoxSelectCallback>::New();
    boxCb->Renderer = renderer;
    boxCb->FracturePoly = fracturePoly;
    boxCb->CellsPerFracture = 4;
    boxCb->LockHintText = lockHud;
    boxCb->Effect = effect.get();
    boxCb->HudText = selectHud;

    // ✅ 你新增的：让 box select 能判断 fracture layer 是否开启
    boxCb->FractureActor = fractureActor;

    iren->AddObserver(vtkCommand::KeyPressEvent, boxCb, 5.0);
    iren->AddObserver(vtkCommand::KeyReleaseEvent, boxCb, 5.0);
    iren->AddObserver(vtkCommand::LeftButtonPressEvent, boxCb, 5.0);
    iren->AddObserver(vtkCommand::MouseMoveEvent, boxCb, 5.0);
    iren->AddObserver(vtkCommand::LeftButtonReleaseEvent, boxCb, 5.0);

    // Pick：优先级低，避免抢 box 的左键；同时会被 well pick Abort 掉
    iren->AddObserver(vtkCommand::LeftButtonPressEvent, pickCb, 1.0);

    // MouseMove -> 坐标显示
    auto movePicker = vtkSmartPointer<vtkCellPicker>::New();
    movePicker->SetTolerance(0.005);

    auto moveCb = vtkSmartPointer<MouseMoveCoordCallback>::New();
    moveCb->Picker = movePicker;
    moveCb->Renderer = renderer;
    moveCb->CoordText = coordHud;
    iren->AddObserver(vtkCommand::MouseMoveEvent, moveCb);

    // Hover tooltip（只对 fracture 高亮集合有效）
    auto hoverCb = vtkSmartPointer<FractureHoverTooltipCallback>::New();
    hoverCb->Renderer = renderer;
    hoverCb->FractureActor = fractureActor;
    hoverCb->Effect = effect.get();
    iren->AddObserver(vtkCommand::MouseMoveEvent, hoverCb, 2.0);

    renderWindow->Render();

    // 把 CAM HUD 放到右下角
    int* winSize = renderWindow->GetSize();
    lockHud->SetPosition(winSize[0] - 300, 20);

    iren->Start();
    return 0;

  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}

