#include "shared.h"

int main() {
    try {
        // 读取配置文件
        std::string gridFile, fieldFile, fractureFile, wellFile;
        if (!readConfig("config.json", gridFile, fieldFile, fractureFile, wellFile)) {
            std::cerr << "Error reading config file" << std::endl;
            return 1;
        }

        // 读取数据
        GridInfo grid = readGridInfo(gridFile);
        std::vector<Fracture> fractures = readFractures(fractureFile);
        std::vector<WellInfo> wells = readWells(wellFile);

        // 计算场景中心和最大长度
        double cx = (grid.Lx) / 2.0;
        double cy = (grid.Ly) / 2.0;
        double cz = (grid.Lz) / 2.0;
        double maxLen = std::max({grid.Lx, grid.Ly, grid.Lz});

        // 创建渲染器、渲染窗口和交互器
        vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
        vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
        renderWindow->AddRenderer(renderer);
        renderWindow->SetSize(1024, 768);
        renderWindow->SetOffScreenRendering(1);

        // 背景色设置为黑色
        renderer->SetBackground(0.0, 0.0, 0.0);

        // 创建只显示裂缝和井的场景
        double zScaleFactor = 5.0; // 使用与堆叠层相同的缩放因子
        createFracturesWellsScene(renderer, fractures, wells, zScaleFactor);

        // 设置相机
        setupCamera(renderer, cx, cy, cz, maxLen, true);

        // 渲染
        renderWindow->Render();

        // 保存图片
        saveImage(renderWindow, "fractures_wells.png");

        std::cout << "Fractures and wells visualization completed successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}