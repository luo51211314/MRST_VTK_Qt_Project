// style_lockable.h
#pragma once
#include <vtkInteractorStyleTrackballCamera.h>

class LockableTrackballStyle : public vtkInteractorStyleTrackballCamera {
public:
  static LockableTrackballStyle* New();
  vtkTypeMacro(LockableTrackballStyle, vtkInteractorStyleTrackballCamera);

  void SetCameraLocked(bool v) { CameraLocked = v; }
  bool GetCameraLocked() const { return CameraLocked; }

  void OnLeftButtonDown() override;
  void OnLeftButtonUp() override;
  void OnMouseMove() override;

private:
  bool CameraLocked = true; // 默认锁定
};
