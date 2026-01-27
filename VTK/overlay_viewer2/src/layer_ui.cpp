#include "layer_ui.h"
#include "ui_layer_button.h"
#include "fracture_select_effect.h"

#include <vtkCommand.h>
#include <vtkPropCollection.h>
#include <vtkProp3D.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>

#include <functional>

namespace {

// 统一收集：fracture vs other（只收 3D，避免 HUD/UI 被隐藏）
static void CollectLayers(
  LayerUIHandle& h,
  vtkRenderer* renderer,
  vtkActor* fractureActor
) {
  h.fractureProps.clear();
  h.otherProps.clear();

  if (fractureActor) h.fractureProps.push_back(fractureActor);

  auto props = renderer->GetViewProps();
  props->InitTraversal();
  for (vtkIdType i = 0; i < props->GetNumberOfItems(); ++i) {
    vtkProp* p = props->GetNextProp();
    if (!p) continue;

    if (!vtkProp3D::SafeDownCast(p)) continue; // 只处理3D
    if (p == fractureActor) continue;

    h.otherProps.push_back(p);
  }
}

static void ApplyVisibility(
  LayerUIHandle& h,
  vtkRenderWindowInteractor* iren,
  FractureSelectEffect* effect
) {
  // Fracture layer
  for (auto* p : h.fractureProps) {
    if (!p) continue;
    p->SetVisibility(h.showFracture ? 1 : 0);

    // 关键：隐藏时不可拾取，显示时可拾取
    p->SetPickable(h.showFracture ? 1 : 0);
  }

  // Other layer
  for (auto* p : h.otherProps) {
    if (!p) continue;
    p->SetVisibility(h.showOther ? 1 : 0);
    // other 是否允许拾取看你需求；不要求就不动
    // p->SetPickable(h.showOther ? 1 : 0);
  }

  if (!h.showFracture && effect) {
    effect->Clear(iren);
  }
}

class UiLayerClickCallback : public vtkCommand {
public:
  static UiLayerClickCallback* New() { return new UiLayerClickCallback(); }

  vtkRenderWindowInteractor* Iren = nullptr;

  UiLayerButton* BtnFracture = nullptr;
  UiLayerButton* BtnOther = nullptr;

  LayerUIHandle* Handle = nullptr;
  FractureSelectEffect* Effect = nullptr;

  std::function<void()> Apply;

  void Execute(vtkObject* caller, unsigned long eventId, void*) override {
    if (eventId != vtkCommand::LeftButtonPressEvent) return;

    auto* iren = vtkRenderWindowInteractor::SafeDownCast(caller);
    if (!iren || !Handle || !BtnFracture || !BtnOther) return;

    int x = 0, y = 0;
    iren->GetEventPosition(x, y);

    if (BtnFracture->Hit(x, y)) {
      Handle->showFracture = !Handle->showFracture;
      BtnFracture->SetChecked(Handle->showFracture);
      if (!Handle->showFracture && Effect) Effect->Clear(iren);
      if (Apply) Apply();
      this->AbortFlagOn();
      if (iren->GetRenderWindow()) iren->GetRenderWindow()->Render();
      return;
    }

    if (BtnOther->Hit(x, y)) {
      Handle->showOther = !Handle->showOther;
      BtnOther->SetChecked(Handle->showOther);
      if (Apply) Apply();
      this->AbortFlagOn();
      if (iren->GetRenderWindow()) iren->GetRenderWindow()->Render();
      return;
    }
  }
};

} // namespace

std::shared_ptr<LayerUIHandle> InstallLayerUI(
  vtkRenderWindowInteractor* iren,
  vtkRenderer* renderer,
  vtkActor* fractureActor,
  FractureSelectEffect* effect
) {
  auto h = std::make_shared<LayerUIHandle>();

  // 1) 分桶
  CollectLayers(*h, renderer, fractureActor);
  ApplyVisibility(*h, iren, effect);

  // 2) 创建按钮（必须让按钮对象活着：放 static 或 new）
  //   这里用 new，让它们跟进程同寿命（简单可靠）。
  auto* btnFracture = new UiLayerButton();
  btnFracture->X = 20; btnFracture->Y = 20; btnFracture->W = 220; btnFracture->H = 36;
  btnFracture->Label = "Fracture";
  btnFracture->Checked = h->showFracture;
  btnFracture->Build(renderer);

  auto* btnOther = new UiLayerButton();
  btnOther->X = 20; btnOther->Y = 62; btnOther->W = 220; btnOther->H = 36;
  btnOther->Label = "Other";
  btnOther->Checked = h->showOther;
  btnOther->Build(renderer);

  // 3) 注册点击回调（最高优先级，吃事件）
  auto cb = vtkSmartPointer<UiLayerClickCallback>::New();
  cb->Iren = iren;
  cb->BtnFracture = btnFracture;
  cb->BtnOther = btnOther;
  cb->Handle = h.get();
  cb->Effect = effect;
  cb->Apply = [h, iren, effect]() {
    ApplyVisibility(*h, iren, effect);
  };

  h->clickCb = cb;
  iren->AddObserver(vtkCommand::LeftButtonPressEvent, cb, 20.0);

  return h;
}
