#pragma once

#include "render_bundle.h"

// Mode2：严格网格压力（堆叠层）+ 裂缝 + 井
inline RenderBundle BuildMode2_GridPressure(
    vtkRenderer* renderer,
    const GridInfo& gridInfo,
    const FieldData& fieldData,
    const std::vector<Fracture>& fractures,
    const std::vector<WellInfo>& wells
) {
    (void)gridInfo;

    RenderBundle out;
    out.name = "Mode2: GridPressure";
    out.isStacked = true;
    out.zScale = 10.0; // 你现在 main 里就是 10.0

    vtkIdType before = renderer->GetViewProps()->GetNumberOfItems();

    // 1) LUT + ColorBar（每个模式各一套，互不干扰，后面你可以选择只留一个）
    auto lut = createLookupTable(fieldData.p_min, fieldData.p_max);
    auto bar = createColorBar(lut);
    renderer->AddActor2D(bar);

    // 2) 压力场（堆叠层）
    createStackedLayersScene(renderer, fieldData, lut, out.zScale);

    // 3) 叠加裂缝+井（注意 zScale 要一致）
    drawFractures(renderer, fractures, out.zScale);
    drawWells(renderer, wells, fractures, out.zScale);

    out.props = CollectNewProps(renderer, before);
    return out;
}
