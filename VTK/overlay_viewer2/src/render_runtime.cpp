#include "render_runtime.h"

#include <filesystem>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

// 把 config 里的相对路径，转换成“相对 config 所在目录”的绝对路径
// 你之前已经这么做过，这里把逻辑封装成函数，避免你到处复制
static std::string ResolvePathRelativeToConfig(const std::string& configFile, const std::string& p) {
    fs::path cfg = fs::absolute(fs::path(configFile));
    fs::path baseDir = cfg.parent_path(); // config 的所在目录
    fs::path raw = fs::path(p);

    if (raw.is_absolute()) return raw.string();
    return fs::absolute(baseDir / raw).string();
}

SceneModes Build3ModesFromConfig(const std::string& configFile, vtkRenderer* renderer) {
    SceneModes out;
    if (!renderer) {
        throw std::runtime_error("Build3ModesFromConfig: renderer is null");
    }

    // ===== 0) 清理旧的东西，保证“可多次调用” =====
    // RemoveAllViewProps 会把 3D actor 和 2D actor 都清掉（包括 colorbar/HUD等）
    // 你后面 main 里如果还有 HUD，要在调用本函数后重新 AddActor2D(HUD)。
    renderer->RemoveAllViewProps();

    // ===== 1) 读 config 得到四个文件路径（原始字符串）=====
    std::string gridFile, fieldFile, fractureFile, wellFile;
    if (!readConfig(configFile, gridFile, fieldFile, fractureFile, wellFile)) {
        throw std::runtime_error("readConfig failed: " + configFile);
    }

    // ===== 2) 解析成“相对 config 的绝对路径”=====
    gridFile     = ResolvePathRelativeToConfig(configFile, gridFile);
    fieldFile    = ResolvePathRelativeToConfig(configFile, fieldFile);
    fractureFile = ResolvePathRelativeToConfig(configFile, fractureFile);
    wellFile     = ResolvePathRelativeToConfig(configFile, wellFile);

    // （可选）打印一下，方便你确认路径
    std::cout << "[resolved paths]\n"
              << "  gridFile=" << gridFile << "\n"
              << "  fieldFile=" << fieldFile << "\n"
              << "  fractureFile=" << fractureFile << "\n"
              << "  wellFile=" << wellFile << "\n";

    // ===== 3) 读数据 =====
    GridInfo gridInfo = readGridInfo(gridFile);
    FieldData fieldData = readFieldData(fieldFile);
    auto fractures = readFractures(fractureFile);
    auto wells = readWells(wellFile);

    // ===== 4) 计算相机参数（给 main 用）=====
    out.cx = gridInfo.Lx / 2.0;
    out.cy = gridInfo.Ly / 2.0;
    out.cz = gridInfo.Lz / 2.0;
    out.maxDim = std::max({gridInfo.Lx, gridInfo.Ly, gridInfo.Lz});

    // ===== 5) 构建 3 个模式（调用第 1 步接口）=====
    out.mode1 = BuildMode1_FractureWell(renderer, gridInfo, fieldData, fractures, wells);
    out.mode2 = BuildMode2_GridPressure(renderer, gridInfo, fieldData, fractures, wells);
    out.mode3 = BuildMode3_SmoothPressure(renderer, gridInfo, fieldData, fractures, wells);

    // ===== 6) 默认显示 Mode1，隐藏 Mode2/Mode3 =====
    SetPropsVisible(out.mode1.props, true);
    SetPropsVisible(out.mode2.props, false);
    SetPropsVisible(out.mode3.props, false);

    return out;
}
