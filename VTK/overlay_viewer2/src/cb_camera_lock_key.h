// cb_camera_lock_key.h
#pragma once
#include <vtkCommand.h>

class vtkTextActor;
class LockableTrackballStyle;

class CameraLockKeyCallback : public vtkCommand {
public:
  static CameraLockKeyCallback* New();
  vtkTypeMacro(CameraLockKeyCallback, vtkCommand);

  LockableTrackballStyle* Style = nullptr;
  vtkTextActor* LockHud = nullptr;

  // 原来是 'f'，改成 'l'
  char ToggleKeyLower = 'l';

  void Execute(vtkObject* caller, unsigned long eventId, void* callData) override;
};

