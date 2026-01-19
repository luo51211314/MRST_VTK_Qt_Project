========================================
Overlay Viewer（3D 场景交互版）
裂缝 · 压力场 · Pick · HUD · 框选 · 相机锁定
========================================


========================================
一、项目定位（非常重要）
========================================

Overlay Viewer 是一个基于 VTK 的【交互层工程】，用于：

- 在【已完成并验证正确的 3D 渲染场景】之上
- 叠加人机交互能力（Pick / HUD / 框选 / 相机锁定等）

工程边界非常明确：

- ❌ 不负责任何数值计算
- ❌ 不修改 shared / main_visualizer 的渲染逻辑
- ✅ 只做“结果可视化 + 交互分析”


----------------------------------------
严格分工原则（不可破坏）
----------------------------------------

【shared / main_visualizer负责】
- 读取 CSV / config.json
- 构建统一的 3D 世界坐标
- 渲染压力场、裂缝、井
- 渲染逻辑不可修改（可信结果）

【overlay_viewer_brother（本工程）负责】
- 复用 shared 中创建好的 renderer / actor
- 在 renderer / interactor 上注册交互回调
- 实现 Pick / HUD / 框选 / 相机锁定等交互
- 不直接修改 shared 生成的几何数据（只叠加 overlay actor）


========================================
二、运行方式（已验证）
========================================

编译（推荐干净 build）：

cd ~/projects/overlay_viewer2
rm -rf build
mkdir build
cd build
cmake ..
make -j4 overlay_viewer_brother

运行：

./overlay_viewer_brother ../../combined_visualization/config.json


说明：
- config.json 完全由渲染工程定义
- overlay 不解析任何 CSV
- 所有渲染流程均来自 shared
- overlay 仅负责交互层逻辑


========================================
三、工程目录结构（当前真实状态）
========================================

overlay_viewer2/
├── CMakeLists.txt
├── build/                （构建产物，建议随时可删重建）
└── src/
    ├── main_brother.cpp  （主入口，负责 wiring / 注册回调）
    │
    ├── fracture_select_effect.h
    ├── fracture_select_effect.cpp
    │   （★统一的“选择效果层”：高亮 + HUD 输出 + Hover Tooltip）
    │
    ├── cb_fracture_pick.h
    ├── cb_fracture_pick.cpp
    │   （单击拾取：只负责“选谁”，统一交给 fracture_select_effect 输出）
    │
    ├── cb_fracture_box_select.h
    ├── cb_fracture_box_select.cpp
    │   （框选：画绿框 + 计算选中 fracture 集合，最终交给 effect 输出）
    │
    ├── cb_fracture_hover_tooltip.h
    ├── cb_fracture_hover_tooltip.cpp
    │   （鼠标 hover 到“已高亮裂缝”时显示浮窗信息）
    │
    ├── cb_camera_lock_key.h
    ├── cb_camera_lock_key.cpp
    │   （相机锁定快捷键与状态提示）
    │
    ├── cb_mouse_move_coord.h
    ├── cb_mouse_move_coord.cpp
    │   （鼠标移动显示世界坐标）
    │
    ├── style_lockable.h
    ├── style_lockable.cpp
    │   （可锁定的相机交互 style：后续“彻底接管锁定”主要改这里）
    │
    ├── vtk_utils.cpp
    ├── vtk_utils.h
    │   （VTK 辅助：复制 cell / 生成线段等工具函数）
    │
    ├── data.cpp
    ├── data.h
    └── ui_helpers.cpp / ui_helpers.h


========================================
四、交互功能说明（当前真实行为）
========================================

----------------------------------------
1）裂缝单击高亮（Pick） ✅
----------------------------------------

【功能表现】
- 鼠标左键单击裂缝：
  - 仅高亮被点击的那一条裂缝（黄线）
  - 左上角显示 Pick 信息（fractureIndex / cellId / BBox）
- 单击空白区域：
  - 清空当前选择
  - 清空高亮与左上角信息
- 不影响压力场、网格、井

【实现要点】
- vtkCellPicker
- PickListOn：仅允许拾取 fractureActor（避免点到网格/压力场）
- cellId / CellsPerFracture → fractureIndex
- 高亮与 HUD 完全由 fracture_select_effect 统一处理

【关键代码】
- src/cb_fracture_pick.cpp
  - Execute()
- src/fracture_select_effect.cpp
  - ApplyPick()
  - Clear()


----------------------------------------
2）鼠标移动显示世界坐标（HUD） ✅
----------------------------------------

【功能表现】
- 鼠标移动时，实时显示当前命中的世界坐标 (X / Y / Z)
- 若未命中任何物体，显示 “(no hit)”

【实现要点】
- MouseMoveEvent
- vtkCellPicker::Pick()
- vtkTextActor 实时更新文本（可做简单节流减少 Render）

【关键代码】
- src/cb_mouse_move_coord.cpp
  - Execute()


----------------------------------------
3）框选裂缝（B + 左键拖拽） ✅【已稳定】
----------------------------------------

【功能表现】
- 按住 B + 左键拖拽：绘制绿色框（Display 坐标）
- 松开左键：
  - 绿框立即消失
  - 框内裂缝整体高亮（样式与 Pick 完全一致）
  - 左上角显示框选统计信息
- 若未选中任何裂缝：
  - 自动清空高亮与 HUD

【框选统计信息内容（当前版本）】
- 选中裂缝条数（按 fracture，而非 cell）
- 屏幕框选范围（像素坐标）
- 选中裂缝集合的世界坐标包围盒（X / Y / Z）
- 前若干个 fracture ID（调试/验证）

【重要结构说明】
- Box Select 本身只负责：
  - 画绿框
  - 计算选中 fracture 集合
- 真正的高亮 / HUD / 清空 全部交给 fracture_select_effect

【实现要点】
- vtkActor2D + Display 坐标绘制绿框
- vtkAreaPicker 生成 frustum
- 遍历 fracture polydata，按 CellsPerFracture 判断命中 fractureIndex
- Release 阶段统一完成：清框 → 应用选择 → HUD 更新

【关键代码】
- src/cb_fracture_box_select.cpp
  - Execute()
  - BuildSelectionFromRect()
- src/fracture_select_effect.cpp
  - ApplyBox()


----------------------------------------
4）Hover Tooltip（鼠标悬停浮窗） ✅
----------------------------------------

【功能表现】
- 当存在 Pick 或 Box 的“高亮选中集合”时：
  - 鼠标移动到“已高亮裂缝”上，会在鼠标附近显示浮窗
  - 浮窗内容为该裂缝的简要信息（fractureIndex + BBox 等）
- 鼠标移出高亮裂缝：浮窗自动消失
- 没有任何高亮时：不显示浮窗

【实现要点】
- MouseMoveEvent + vtkCellPicker
- 仅 pick fractureActor
- cellId → fractureIndex
- fractureIndex 必须属于 SelectedFractureIds 才显示 tooltip

【关键代码】
- src/cb_fracture_hover_tooltip.cpp / .h
- src/fracture_select_effect.cpp
  - UpdateHoverTooltip()
  - ShowTooltip() / HideTooltip()


----------------------------------------
5）相机锁定状态提示（HUD） ✅（锁定接管仍进行中）
----------------------------------------

【功能表现】
- 右下角显示相机锁定状态：
  - CAM: UNLOCKED (press L to lock)
  - CAM: LOCKED (hold B to box-select)

【实现要点】
- vtkTextActor 固定右下角
- KeyPress 切换锁定状态文本
- 注意：真正“锁定后禁用旋转”还未完全接管（见 TODO）

【关键代码】
- src/main_brother.cpp（lockHud 创建/定位）
- src/cb_camera_lock_key.cpp


========================================
五、关键结构设计总结（非常重要）
========================================

1）Pick / Box 已彻底解耦
-------------------------
- Pick / Box 只负责：
  - “选中了什么”（fractureIndex 集合）
- fracture_select_effect 负责：
  - “怎么高亮”
  - “显示什么信息”
  - “如何清空旧状态”
  - “Hover Tooltip 的内容与显示/隐藏”

2）统一选择效果层（fracture_select_effect）
---------------------------------------------
- 避免 Pick / Box 各自维护高亮 actor（容易不同步）
- 避免 HUD 指针被覆盖导致“看不到信息”的坑
- 保证：
  - 单选 / 多选 行为一致
  - 高亮样式一致
  - 左上角信息位置一致
  - hover 浮窗逻辑一致

【本次修复经验（关键）】
- 回调里不要反复覆盖 Effect->HudText（容易被空指针/旧指针覆盖）
- HudText（selectHud）统一在 main 中创建并 AddActor2D，再传给 effect / callback



========================================
六、当前结论
========================================

当前版本是一个：

【可运行 · 可 Pick · 可框选 · 可统计 · 可 Hover 浮窗 ·  
 Pick/Box 高亮与 HUD 已统一】

的 3D 交互查看器。
