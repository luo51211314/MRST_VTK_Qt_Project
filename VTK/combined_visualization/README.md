# 压力场可视化系统

## 项目概述

本项目提供了两种压力场可视化方式：堆叠层可视化和实体立方体可视化。系统支持从CSV文件读取网格信息、压力场数据、裂缝数据和井数据，并根据配置文件进行可视化渲染。

## 文件结构

### 主程序文件

- **main.cpp**: 主控制器，负责调用不同的可视化程序。
- **stacked_visualizer.cpp**: 堆叠层可视化程序，将压力场按Z轴分层显示。
- **cube_visualizer.cpp**: 实体立方体可视化程序，将压力场显示为实体立方体。
- **shared.h**: 共享头文件，包含所有数据结构和函数声明。
- **shared.cpp**: 共享实现文件，包含所有函数的实现。

### 配置文件

- **config.json**: 配置文件，包含所有CSV文件的路径信息。

### 依赖CSV文件

- **/root/csv_file/grid_info.csv**: 网格信息文件，包含网格的维度和尺寸信息。
- **/root/csv_file/final_field.csv**: 压力场数据文件，包含每个网格点的压力值。
- **/root/csv_file/fracture_geometry.csv**: 裂缝数据文件，包含裂缝的几何信息。
- **/root/csv_file/well_info.csv**: 井数据文件，包含井的位置和参数信息。

## CSV文件格式说明

### grid_info.csv

| 字段 | 描述 |
|------|------|
| nx | X方向网格数 |
| ny | Y方向网格数 |
| nz | Z方向网格数 |
| Lx | X方向总长度 |
| Ly | Y方向总长度 |
| Lz | Z方向总长度 |
| npx | X方向处理器数 |
| npy | Y方向处理器数 |
| dx | X方向网格间距 |

### final_field.csv

| 字段 | 描述 |
|------|------|
| x | X坐标 |
| y | Y坐标 |
| z | Z坐标 |
| pressure | 压力值(MPa) |

### fracture_geometry.csv

| 字段 | 描述 |
|------|------|
| fracture_id | 裂缝ID |
| x1 | 第一个角点X坐标 |
| y1 | 第一个角点Y坐标 |
| z1 | 第一个角点Z坐标 |
| x2 | 第二个角点X坐标 |
| y2 | 第二个角点Y坐标 |
| z2 | 第二个角点Z坐标 |
| x3 | 第三个角点X坐标 |
| y3 | 第三个角点Y坐标 |
| z3 | 第三个角点Z坐标 |
| x4 | 第四个角点X坐标 |
| y4 | 第四个角点Y坐标 |
| z4 | 第四个角点Z坐标 |

### well_info.csv

| 字段 | 描述 |
|------|------|
| well_id | 井ID |
| node_idx | 节点索引 |
| type | 井类型 |
| x | X坐标 |
| y | Y坐标 |
| z | Z坐标 |
| WI | 井筒指数 |
| P_bhp | 井底压力 |

## 使用方法

### 编译项目

```bash
cd build
cmake ..
make -j4
```

### 运行可视化

#### 运行主控制器

```bash
./main_visualizer [visualization_type]
```

- `visualization_type` 参数可选，默认值为 "both"：
  - "stacked": 仅运行堆叠层可视化
  - "cube": 仅运行实体立方体可视化
  - "both": 运行两种可视化

#### 直接运行特定可视化

```bash
# 运行堆叠层可视化
./stacked_visualizer [config_file]

# 运行实体立方体可视化
./cube_visualizer [config_file]
```

- `config_file` 参数可选，默认值为 "./config.json"

## 可视化结果

程序将生成以下可视化结果文件：

- **pressure_layers_stacked.png**: 堆叠层可视化结果
- **pressure_solid_block.png**: 实体立方体可视化结果

## 注意事项

1. 确保所有CSV文件路径正确配置在config.json中
2. 可视化程序需要在支持VTK的环境中运行
3. 可以通过修改config.json文件来更改输入文件路径
