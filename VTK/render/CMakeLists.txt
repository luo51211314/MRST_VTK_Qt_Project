cmake_minimum_required(VERSION 3.10)
project(combined_pressure_visualization)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 查找VTK包
find_package(VTK REQUIRED COMPONENTS
  CommonCore
  CommonDataModel
  FiltersCore
  FiltersGeometry
  RenderingCore
  RenderingOpenGL2
  RenderingAnnotation
  IOImage
)

# 包含VTK头文件
include_directories(${VTK_INCLUDE_DIRS})

# 创建共享库
add_library(shared_lib shared.cpp)

# 包含VTK头文件到共享库
target_include_directories(shared_lib PRIVATE ${VTK_INCLUDE_DIRS})

# 链接VTK库到共享库
target_link_libraries(shared_lib ${VTK_LIBRARIES})

# 确保VTK模块正确初始化
target_compile_definitions(shared_lib PRIVATE ${VTK_DEFINITIONS})

# 创建可执行文件
add_executable(combined_pressure_visualizer combined_pressure_visualizer.cpp)
add_executable(stacked_visualizer stacked_visualizer.cpp)
add_executable(cube_visualizer cube_visualizer.cpp)
add_executable(main_visualizer main.cpp)

# 链接库
target_link_libraries(combined_pressure_visualizer shared_lib ${VTK_LIBRARIES})
target_link_libraries(stacked_visualizer shared_lib ${VTK_LIBRARIES})
target_link_libraries(cube_visualizer shared_lib ${VTK_LIBRARIES})
target_link_libraries(main_visualizer)

# 确保VTK模块正确链接
vtk_module_autoinit(
  TARGETS combined_pressure_visualizer stacked_visualizer cube_visualizer
  MODULES ${VTK_LIBRARIES}
)
