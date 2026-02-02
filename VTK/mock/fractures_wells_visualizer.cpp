#include "shared.h"
#include "mock_data.h"

int main() {
    try {
        std::cout << "Generating MOCK data for fractures+wells visualization..." << std::endl;

        auto ds = MockData::Generate(MockData::GenerateParams{});

        GridInfo grid;
        FieldData dummyField;
        std::vector<Fracture> fractures;
        std::vector<WellInfo> wells;

        BuildSharedInputsFromMock(
            ds.grid, ds.cells, ds.fractures, ds.wells,
            grid, dummyField, fractures, wells
        );

        // 计算场景中心和最大长度
        double cx = (grid.Lx) / 2.0;
        double cy = (grid.Ly) / 2.0;
        double cz = (grid.Lz) / 2.0;
        double maxLen = std::max({grid.Lx, grid.Ly, grid.Lz});

        // 创建渲染器、渲染窗口
        vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
        vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
        renderWindow->AddRenderer(renderer);
        renderWindow->SetSize(1024, 768);
        renderWindow->SetOffScreenRendering(1);

        // 背景色设置为黑色
        renderer->SetBackground(0.0, 0.0, 0.0);

        // 创建只显示裂缝和井的场景
        double zScaleFactor = 5.0;
        createFracturesWellsScene(renderer, fractures, wells, zScaleFactor);

        // 设置相机
        setupCamera(renderer, cx, cy, cz, maxLen, true);

        // 渲染并保存
        renderWindow->Render();
        saveImage(renderWindow, "fractures_wells.png");

        std::cout << "Fractures and wells visualization completed successfully!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

