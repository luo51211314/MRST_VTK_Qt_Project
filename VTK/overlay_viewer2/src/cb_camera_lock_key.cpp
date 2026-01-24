#include "cb_camera_lock_key.h"
#include "style_lockable.h"

#include <vtkObjectFactory.h>          // ★必须有（提供 vtkStandardNewMacro）
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderWindow.h>
#include <vtkTextActor.h>

#include <cctype>
#include <iostream>

vtkStandardNewMacro(CameraLockKeyCallback);


void CameraLockKeyCallback::Execute(vtkObject* caller, unsigned long eventId, void*) {
  if (eventId != vtkCommand::KeyPressEvent) return;

  auto* iren = vtkRenderWindowInteractor::SafeDownCast(caller);
  if (!iren || !Style) return;

  char code = iren->GetKeyCode();
  code = (char)std::tolower((unsigned char)code);

  if (code != ToggleKeyLower) return;

  bool locked = !Style->GetCameraLocked();
  Style->SetCameraLocked(locked);

  // cb_camera_lock_key.cpp 里
if (LockHud) {
  LockHud->SetInput(locked ? "CAM: LOCKED (hold B to box-select)"
                           : "CAM: UNLOCKED (press L to lock)");
  LockHud->SetVisibility(1);
}


  std::cout << "[Cam] Locked=" << (locked ? "YES" : "NO") << "\n";
  iren->GetRenderWindow()->Render();

  this->AbortFlagOn(); // 防止别的 KeyPress callback 也吃到 F
}
