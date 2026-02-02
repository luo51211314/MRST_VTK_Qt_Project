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
#include "render_runtime.h"

#include <filesystem>
#include <tuple>

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
#include <vector>

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
      break;
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

  FractureSelectEffect* FractureEffect = nullptr;

  void Execute(vtkObject* caller, unsigned long eventId, void*) override {
    if (eventId != vtkCommand::LeftButtonPressEvent) return;
    if (!Picker || !Renderer || !WellActor || !WellPoly || !Wells || !HudText) return;

    auto* iren = vtkRenderWindowInteractor::SafeDownCast(caller);
    if (!iren) return;

    int x = 0, y = 0;
    iren->GetEventPosition(x, y);

    if (WellActor->GetPickable() == 0 || WellActor->GetVisibility() == 0) {
      HudText->SetInput("");
      HudText->SetVisibility(0);
      return;
    }

    Picker->PickFromListOn();
    Picker->InitializePickList();
    Picker->AddPickList(WellActor);

    int ok = Picker->Pick(x, y, 0.0, Renderer);
    if (!ok) {
      HudText->SetInput("");
      HudText->SetVisibility(0);
      return;
    }

    vtkActor* picked = vtkActor::SafeDownCast(Picker->GetViewProp());
    if (picked != WellActor) return;

    vtkIdType cellId = Picker->GetCellId();
    if (cellId < 0) return;

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

    this->AbortFlagOn();
    if (iren->GetRenderWindow()) iren->GetRenderWindow()->Render();
  }
};

// ============================
// Mode 切换：收集 props + 按钮回调
// ============================

enum class RenderMode {
  MODE1_FRAC_WELL = 0,   // 裂缝+井
  MODE2_GRID_FIELD = 1,  // 严格网格压力（叠加裂缝井）
  MODE3_SMOOTH_FIELD = 2 // 平滑占满压力（叠加裂缝井）
};


class ModeClickCallback : public vtkCommand {
public:
  static ModeClickCallback* New() { return new ModeClickCallback(); }

  vtkRenderWindowInteractor* Iren = nullptr;
  vtkRenderer* Renderer = nullptr;

  UiLayerButton* Btn1 = nullptr;
  UiLayerButton* Btn2 = nullptr;
  UiLayerButton* Btn3 = nullptr;

  SceneModes* Modes = nullptr;  // NEW

  RenderMode Current = RenderMode::MODE1_FRAC_WELL;

  void ApplyMode(RenderMode m) {
    Current = m;

    if (!Renderer || !Modes) return;

    // mode1（裂缝+井）始终可见
    SetPropsVisible(Modes->mode1.props, true);

    // 二选一：压力场只显示一个
    SetPropsVisible(Modes->mode2.props, (m == RenderMode::MODE2_GRID_FIELD));
    SetPropsVisible(Modes->mode3.props, (m == RenderMode::MODE3_SMOOTH_FIELD));

    // 按钮状态
    if (Btn1) Btn1->SetChecked(m == RenderMode::MODE1_FRAC_WELL);
    if (Btn2) Btn2->SetChecked(m == RenderMode::MODE2_GRID_FIELD);
    if (Btn3) Btn3->SetChecked(m == RenderMode::MODE3_SMOOTH_FIELD);

    // 相机：mode2 是 stacked（Z 拉伸），其余 normal
    if (m == RenderMode::MODE2_GRID_FIELD) {
      setupCamera(Renderer, Modes->cx, Modes->cy, Modes->cz * 10.0, Modes->maxDim, true);
    } else {
      setupCamera(Renderer, Modes->cx, Modes->cy, Modes->cz, Modes->maxDim, false);
    }

    if (Iren && Iren->GetRenderWindow()) Iren->GetRenderWindow()->Render();
  }

  void Execute(vtkObject* caller, unsigned long eventId, void*) override {
    if (eventId != vtkCommand::LeftButtonPressEvent) return;

    auto* iren = vtkRenderWindowInteractor::SafeDownCast(caller);
    if (!iren || !Btn1 || !Btn2 || !Btn3) return;

    int x = 0, y = 0;
    iren->GetEventPosition(x, y);

    if (Btn1->Hit(x, y)) { ApplyMode(RenderMode::MODE1_FRAC_WELL); this->AbortFlagOn(); return; }
    if (Btn2->Hit(x, y)) { ApplyMode(RenderMode::MODE2_GRID_FIELD); this->AbortFlagOn(); return; }
    if (Btn3->Hit(x, y)) { ApplyMode(RenderMode::MODE3_SMOOTH_FIELD); this->AbortFlagOn(); return; }
  }
};


int main(int argc, char** argv) {
  try {
    std::string configFile = "./config.json";
    if (argc > 1) configFile = argv[1];

    std::string gridFile, fieldFile, fractureFile, wellFile;

// ---- debug: print cwd + config path existence ----
namespace fs = std::filesystem;
std::cout << "[CWD] " << fs::current_path() << std::endl;
std::cout << "[ARG configFile] " << configFile << std::endl;

fs::path cfgPath = fs::path(configFile);
std::error_code ec;
std::cout << "[CFG exists] " << fs::exists(cfgPath, ec)
          << "  is_regular=" << fs::is_regular_file(cfgPath, ec)
          << "  abs=" << fs::absolute(cfgPath, ec)
          << std::endl;

if (!readConfig(configFile, gridFile, fieldFile, fractureFile, wellFile)) {
  std::cerr << "[readConfig] FAILED.\n";
  std::cerr << "  configFile=" << configFile << "\n";
  std::cerr << "  abs=" << fs::absolute(cfgPath, ec) << "\n";
  return 1;
}

std::cout << "[readConfig] OK\n"
          << "  gridFile=" << gridFile << "\n"
          << "  fieldFile=" << fieldFile << "\n"
          << "  fractureFile=" << fractureFile << "\n"
          << "  wellFile=" << wellFile << "\n";

// ===== fix: resolve relative csv paths by config directory (NOT by CWD) =====
auto resolveByConfigDir = [&](const std::string& p) -> std::string {
  fs::path pp(p);
  if (pp.empty()) return p;
  if (pp.is_absolute()) return pp.lexically_normal().string();

  fs::path cfgAbs = fs::absolute(fs::path(configFile), ec);
  fs::path cfgDir = cfgAbs.parent_path();
  return (cfgDir / pp).lexically_normal().string();
};

gridFile     = resolveByConfigDir(gridFile);
fieldFile    = resolveByConfigDir(fieldFile);
fractureFile = resolveByConfigDir(fractureFile);
wellFile     = resolveByConfigDir(wellFile);

std::cout << "[resolved paths]\n"
          << "  gridFile=" << gridFile << "\n"
          << "  fieldFile=" << fieldFile << "\n"
          << "  fractureFile=" << fractureFile << "\n"
          << "  wellFile=" << wellFile << "\n";

// ---- debug: check the parsed files exist (as given) ----
auto printExists = [&](const char* name, const std::string& p) {
  fs::path pp = fs::path(p);
  std::cout << "[" << name << " exists] " << fs::exists(pp, ec)
            << "  is_regular=" << fs::is_regular_file(pp, ec)
            << "  abs=" << fs::absolute(pp, ec)
            << std::endl;
};
printExists("gridFile", gridFile);
printExists("fieldFile", fieldFile);
printExists("fractureFile", fractureFile);
printExists("wellFile", wellFile);


    GridInfo gridInfo = readGridInfo(gridFile);
    FieldData fieldData = readFieldData(fieldFile);
    auto fractures = readFractures(fractureFile);
    auto wells = readWells(wellFile);

    auto renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->SetBackground(0.1, 0.1, 0.1);

// ============================
// NEW: 一次性构建 3 个模式（mode1 + mode2 + mode3）
// 注意：Build3ModesFromConfig 内部会 RemoveAllViewProps，所以一定要在 HUD 之前调用
// ============================
SceneModes modes = Build3ModesFromConfig(configFile, renderer);

// 相机先给 mode1（普通，不拉伸）
setupCamera(renderer, modes.cx, modes.cy, modes.cz, modes.maxDim, false);

    auto renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->AddRenderer(renderer);
    renderWindow->SetSize(1200, 900);

    auto iren = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    iren->SetRenderWindow(renderWindow);

    // 固定 3D 交互风格
    auto style3D = vtkSmartPointer<LockableTrackballStyle>::New();
    style3D->SetCameraLocked(false);
    iren->SetInteractorStyle(style3D);

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

    // Layer UI（Fracture/Other）
    auto layerUI = InstallLayerUI(
      iren,
      renderer,
      fractureActor,
      effect.get()
    );

    // Well pick callback
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
    boxCb->FractureActor = fractureActor;

    iren->AddObserver(vtkCommand::KeyPressEvent, boxCb, 5.0);
    iren->AddObserver(vtkCommand::KeyReleaseEvent, boxCb, 5.0);
    iren->AddObserver(vtkCommand::LeftButtonPressEvent, boxCb, 5.0);
    iren->AddObserver(vtkCommand::MouseMoveEvent, boxCb, 5.0);
    iren->AddObserver(vtkCommand::LeftButtonReleaseEvent, boxCb, 5.0);

    iren->AddObserver(vtkCommand::LeftButtonPressEvent, pickCb, 1.0);

    // MouseMove -> 坐标显示
    auto movePicker = vtkSmartPointer<vtkCellPicker>::New();
    movePicker->SetTolerance(0.005);

    auto moveCb = vtkSmartPointer<MouseMoveCoordCallback>::New();
    moveCb->Picker = movePicker;
    moveCb->Renderer = renderer;
    moveCb->CoordText = coordHud;
    iren->AddObserver(vtkCommand::MouseMoveEvent, moveCb);

    // Hover tooltip
    auto hoverCb = vtkSmartPointer<FractureHoverTooltipCallback>::New();
    hoverCb->Renderer = renderer;
    hoverCb->FractureActor = fractureActor;
    hoverCb->Effect = effect.get();
    iren->AddObserver(vtkCommand::MouseMoveEvent, hoverCb, 2.0);

    // ============================
    // Mode Buttons（3 选 1）
    // ============================
    // 放在左上角按钮的右侧，避免和 Layer UI 重叠
    auto* btnM1 = new UiLayerButton();
    btnM1->X = 260; btnM1->Y = 20; btnM1->W = 260; btnM1->H = 36;
    btnM1->Label = "Mode1: Fracture+Well";
    btnM1->Checked = true;
    btnM1->Build(renderer);

    auto* btnM2 = new UiLayerButton();
    btnM2->X = 260; btnM2->Y = 62; btnM2->W = 260; btnM2->H = 36;
    btnM2->Label = "Mode2: GridPressure";
    btnM2->Checked = false;
    btnM2->Build(renderer);

    auto* btnM3 = new UiLayerButton();
    btnM3->X = 260; btnM3->Y = 104; btnM3->W = 260; btnM3->H = 36;
    btnM3->Label = "Mode3: SmoothPressure";
    btnM3->Checked = false;
    btnM3->Build(renderer);

    auto modeCb = vtkSmartPointer<ModeClickCallback>::New();
    modeCb->Iren = iren;
    modeCb->Renderer = renderer;
    modeCb->Btn1 = btnM1;
    modeCb->Btn2 = btnM2;
    modeCb->Btn3 = btnM3;
    modeCb->Modes = &modes;

    // 比 layer_ui 的 20.0 再高一点，确保点到模式按钮时能吃掉事件
    iren->AddObserver(vtkCommand::LeftButtonPressEvent, modeCb, 21.0);

    // ===== 打印当前工作目录 =====
    std::cout << "[CWD] " << std::filesystem::current_path().string() << std::endl;

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

