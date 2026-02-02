cmake_minimum_required(VERSION 3.10)
project(overlay_viewer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(VTK REQUIRED COMPONENTS
  CommonCore
  CommonDataModel
  FiltersCore
  FiltersSources
  FiltersGeometry
  IOImage
  InteractionStyle
  InteractionWidgets
  RenderingCore
  RenderingOpenGL2
  RenderingAnnotation
  RenderingFreeType
)

# ========== overlay_viewer（当前目标） ==========
add_executable(overlay_viewer
  src/main.cpp

  src/data.cpp
  src/ui_helpers.cpp

  src/vtk_utils.cpp
  src/style_lockable.cpp
  src/cb_camera_lock_key.cpp
  src/cb_mouse_move_coord.cpp
  src/cb_fracture_pick.cpp
  src/cb_fracture_box_select.cpp

  src/fracture_select_effect.cpp
  
  src/render_runtime.cpp

  ../combined_visualization/shared.cpp
  
  src/ui_layer_button.cpp
  
  src/layer_ui.cpp
)

target_include_directories(overlay_viewer PRIVATE ../combined_visualization)

target_link_libraries(overlay_viewer PRIVATE
  VTK::CommonCore
  VTK::CommonDataModel
  VTK::FiltersCore
  VTK::FiltersSources
  VTK::FiltersGeometry
  VTK::IOImage
  VTK::InteractionStyle
  VTK::InteractionWidgets
  VTK::RenderingCore
  VTK::RenderingOpenGL2
  VTK::RenderingAnnotation
  VTK::RenderingFreeType
)

vtk_module_autoinit(
  TARGETS overlay_viewer
  MODULES
    VTK::CommonCore
    VTK::CommonDataModel
    VTK::FiltersCore
    VTK::FiltersSources
    VTK::FiltersGeometry
    VTK::IOImage
    VTK::InteractionStyle
    VTK::InteractionWidgets
    VTK::RenderingCore
    VTK::RenderingOpenGL2
    VTK::RenderingAnnotation
    VTK::RenderingFreeType
)

