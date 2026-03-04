import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import scipy.sparse as sp

# ==========================================
# 1. 设置中文字体（非常关键，防止中文乱码）
# ==========================================
# 如果你是 Windows 用户，通常使用黑体 'SimHei' 或是微软雅黑 'Microsoft YaHei'
plt.rcParams['font.sans-serif'] = ['SimHei']
# 如果你是 Mac 用户，请注释掉上面那行，把下面这行的注释取消：
# plt.rcParams['font.sans-serif'] = ['Arial Unicode MS'] # 或者 'PingFang SC'

plt.rcParams['axes.unicode_minus'] = False # 正常显示负号

# ==========================================
# 2. 读取数据与构建矩阵
# ==========================================
print("Loading Jacobian data...")
df = pd.read_csv('jacobian_sparsity.csv')

# 获取矩阵维度 (行/列的最大索引 + 1)
max_idx = int(max(df['row'].max(), df['col'].max()) + 1)

# 使用 COO 格式构建稀疏矩阵
rows = df['row'].values
cols = df['col'].values
vals = np.ones(len(df))
J_sparse = sp.coo_matrix((vals, (rows, cols)), shape=(max_idx, max_idx))

# ==========================================
# 3. 绘制 Spy 散点图
# ==========================================
plt.figure(figsize=(8, 8), dpi=300)

# markersize 控制点的大小，如果黑点糊成一团，把 0.5 改成 0.1 或 0.05
plt.spy(J_sparse, markersize=0.5, color='darkblue')

# 使用你 PPT 中硬核的中文标题
plt.title("图3：含 EDFM 离散裂缝的三相全隐式 Jacobian 块稀疏矩阵结构", fontsize=15, pad=20, fontweight='bold')
plt.xlabel("列索引 (未知量: 压力 P, 水饱和度 Sw, 气饱和度 Sg)", fontsize=12)
plt.ylabel("行索引 (方程: 水方程, 油方程, 气方程)", fontsize=12)

# 添加网格线辅助查看块状结构（可选）
plt.grid(True, linestyle='--', alpha=0.3)

plt.tight_layout()

# 保存高清图片
plt.savefig('Jacobian_Spy_Plot_CN.png', dpi=300, bbox_inches='tight')
plt.show()

print("Plot saved as Jacobian_Spy_Plot_CN.png!")