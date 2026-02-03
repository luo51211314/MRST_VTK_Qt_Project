#pragma once

#include "render_bundle.h"

// Mode1：只画裂缝 + 井（不画压力场）
// 注意：这一步只负责“往 renderer 里添加东西”并把新增props收集出来。
inline RenderBundle BuildMode1_FractureWell(
    vtkRenderer* renderer,
    const GridInfo& gridInfo,
    const FieldData& fieldData,
    const std::vector<Fracture>& fractures,
    const std::vector<WellInfo>& wells
) {
    (void)gridInfo;   // 现在不用，但保留参数是为了你后面统一接口
    (void)fieldData;  // 现在不用，但保留参数是为了你后面统一接口

    RenderBundle out;
    out.name = "Mode1: Fracture+Well";
    out.isStacked = false;
    out.zScale = 1.0;

    vtkIdType before = renderer->GetViewProps()->GetNumberOfItems();

    // shared.cpp 里已经有组合函数：createFracturesWellsScene = drawFractures + drawWells
    createFracturesWellsScene(renderer, fractures, wells, out.zScale);

    out.props = CollectNewProps(renderer, before);
    return out;
}
