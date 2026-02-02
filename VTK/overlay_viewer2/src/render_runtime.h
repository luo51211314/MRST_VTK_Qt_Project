#pragma once

#include <string>

#include <vtkRenderer.h>

#include "shared.h"                 // GridInfo/FieldData/Fracture/WellInfo/readConfig...
#include "../combined_visualization/render_bundle.h"
#include "../combined_visualization/mode1_fracture_well.h"
#include "../combined_visualization/mode2_grid_pressure.h"
#include "../combined_visualization/mode3_smooth_pressure.h"

// 三个模式 + 相机参数 打包给 main
struct SceneModes {
    RenderBundle mode1;
    RenderBundle mode2;
    RenderBundle mode3;

    // 相机参数（main 切模式时会用）
    double cx = 0, cy = 0, cz = 0;
    double maxDim = 1;
};

// 核心函数：可重复调用（多次调用不会越渲越多）
// renderer 由 main 创建并传进来；函数内部会清理旧的 ViewProps
SceneModes Build3ModesFromConfig(const std::string& configFile, vtkRenderer* renderer);
