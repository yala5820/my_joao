# JoaoKCF — C++ Implementation of KCF Tracker

## 概述

JoaoKCF 是 KCF (Kernelized Correlation Filter) 跟踪算法的 C++ 实现，由 **Joao Faro、Christian Bailer、Joao F. Henriques** 共同完成。该实现源自 Henriques 等人在 TPAMI 2015 发表的论文 *"High-Speed Tracking with Kernelized Correlation Filters"* 及 ECCV 2012 发表的 *"Exploiting the Circulant Structure of Tracking-by-detection with Kernels"*。

本仓库包含两种跟踪器的 C++ 实现：

| 跟踪器 | 命令 | 特征 |
|--------|------|------|
| KCFC++ | `./KCF` | HOG 特征 |
| KCFLabC++ | `./KCF lab` | HOG + Lab 颜色特征 |

原始的 MATLAB 实现在 **VOT 2014** 中获得第 3 名。

---

## 项目结构

```
├── CMakeLists.txt              # 构建配置文件
├── CMakePresets.json           # VS2022 CMake 预设配置
├── KCFCpp.sh                   # KCF (HOG) 快捷脚本
├── KCFLabCpp.sh                # KCF+Lab 快捷脚本
├── README.md
├── LICENSE
├── src/
│   ├── runtracker.cpp          # 主程序入口 (VOT 工具接口)
│   ├── tracker.h               # 抽象基类 Tracker
│   ├── kcftracker.hpp          # KCFTracker 类声明
│   ├── kcftracker.cpp          # KCFTracker 核心实现
│   ├── ffttools.hpp            # FFT 工具函数命名空间
│   ├── recttools.hpp           # 矩形/图像工具函数命名空间
│   ├── fhog.hpp                # FHOG 特征提取声明
│   ├── fhog.cpp                # FHOG 特征提取实现
│   └── labdata.hpp             # Lab 颜色聚类中心数据
├── out/                        # VS 构建输出目录
└── build/                      # 命令行构建输出目录
```

### 文件详细说明

#### `src/tracker.h` — 跟踪器抽象基类

定义纯虚接口，KCFTracker 继承自该类：

```cpp
class Tracker {
public:
    virtual void init(const cv::Rect &roi, cv::Mat image) = 0;
    virtual cv::Rect update(cv::Mat image) = 0;
protected:
    cv::Rect_<float> _roi;
};
```

- **`init(roi, image)`**: 用首帧图像和初始目标位置初始化跟踪器
- **`update(image)`**: 输入新帧，返回更新后的目标位置

#### `src/kcftracker.hpp` — KCFTracker 类声明

核心类，继承自 `Tracker`，包含：

**构造参数（均为 bool）**：
| 参数 | 默认值 | 说明 |
|------|--------|------|
| `hog` | true | 使用 HOG 特征，false 则使用原始灰度像素 |
| `fixed_window` | true | 固定窗口尺寸，false 则使用 ROI 尺寸（更慢但更精确） |
| `multiscale` | true | 多尺度检测，false 为单尺度 |
| `lab` | true | 启用 Lab 颜色特征（需同时启用 HOG） |

**可调参数（构造后可自定义）**：
| 参数 | 说明 | KCF 默认值 | KCF+Lab 默认值 |
|------|------|-----------|---------------|
| `interp_factor` | 线性插值系数 | 0.012 | 0.005 |
| `sigma` | 高斯核带宽 | 0.6 | 0.4 |
| `lambda` | 正则化系数 | 0.0001 | 0.0001 |
| `cell_size` | HOG cell 大小 | 4 | 4 |
| `padding` | 目标周围填充倍数 | 2.5 | 2.5 |
| `output_sigma_factor` | 高斯标签带宽因子 | 0.125 | 0.1 |
| `template_size` | 模板尺寸 | 96 | 96 |
| `scale_step` | 多尺度步长 | 1.05 | 1.05 |
| `scale_weight` | 多尺度权重 | 0.95 | 0.95 |

**核心成员变量**：
```cpp
cv::Mat _alphaf;     // 频域中的滤波器系数
cv::Mat _prob;       // 高斯标签（频域）
cv::Mat _tmpl;       // 目标模板特征
cv::Mat _num;        // （未使用）正则化分母的分子
cv::Mat _den;        // （未使用）正则化分母
cv::Mat _labCentroids; // Lab 聚类中心
int size_patch[3];   // 特征图尺寸 [rows, cols, channels]
cv::Mat hann;        // 汉宁窗
cv::Size _tmpl_sz;   // 目标模板像素尺寸
float _scale;        // 当前尺度
```

#### `src/kcftracker.cpp` — KCFTracker 核心实现

包含以下关键方法：

| 方法 | 描述 |
|------|------|
| `init()` | 首帧初始化，提取特征、生成高斯标签、首次训练 |
| `update()` | 每帧更新：检测 → 多尺度搜索 → 更新位置 → 在线训练 |
| `detect()` | 在目标区域检测，返回亚像素精度的位移 |
| `train()` | 用当前帧更新相关滤波器系数 |
| `gaussianCorrelation()` | 计算高斯核相关矩阵 K(x,x') |
| `createGaussianPeak()` | 创建高斯响应图（频域） |
| `getFeatures()` | 从图像中提取搜索区域的特征 |
| `createHanningMats()` | 初始化汉宁窗 |
| `subPixelPeak()` | 亚像素峰值估计 |

#### `src/runtracker.cpp` — 主程序入口

VOT (Visual Object Tracking) 评测工具包的接口程序，支持命令行参数：

```bash
./KCF [options...]
```

| 选项 | 说明 |
|------|------|
| `hog` | 使用 HOG 特征（默认） |
| `gray` | 使用灰度原始像素特征（即 CSK 算法） |
| `lab` | 启用 Lab 颜色特征（同时启用 HOG） |
| `fixed_window` | 固定窗口尺寸 |
| `singlescale` | 单尺度检测 |
| `show` | 显示跟踪结果窗口 |

**输入文件**：
- `images.txt` — 每行一张图片路径
- `region.txt` — 首帧目标区域坐标（VOT 格式，4 个角点的 x,y）

**输出文件**：
- `output.txt` — 每帧跟踪结果 `x,y,width,height`

#### `src/ffttools.hpp` — FFT 工具函数

命名空间 `FFTTools`，提供频域操作函数：

| 函数 | 说明 |
|------|------|
| `fftd(img, backwards)` | 一/二维 FFT，可逆变换 |
| `real(img)` | 取复数实部 |
| `imag(img)` | 取复数虚部 |
| `magnitude(img)` | 计算幅值谱 |
| `complexMultiplication(a, b)` | 复数乘法 |
| `complexDivision(a, b)` | 复数除法 |
| `rearrange(img)` | 将零频移至频谱中心（象限交换） |
| `normalizedLogTransform(img)` | 对数变换可视化 |

#### `src/recttools.hpp` — 矩形/图像工具

命名空间 `RectTools`：

| 函数 | 说明 |
|------|------|
| `center(rect)` | 返回矩形中心点 |
| `x2(rect) / y2(rect)` | 返回右下角坐标 |
| `resize(rect, sx, sy)` | 以中心为基准缩放矩形 |
| `limit(rect, limit)` | 将矩形限制在边界内 |
| `getBorder(original, limited)` | 获取裁剪前后的边框差异 |
| `subwindow(in, window)` | 从图像中提取子窗口（带边界填充） |
| `getGrayImage(img)` | 图像灰度化并归一化到 [0,1] |

#### `src/fhog.hpp / fhog.cpp` — FHOG 特征提取

移植自 OpenCV latentSVM 模块的 HOG 特征提取，数据结构：

```cpp
typedef struct {
    int sizeX;           // 特征图宽度（cell 数）
    int sizeY;           // 特征图高度（cell 数）
    int numFeatures;     // 特征维数
    float *map;          // 特征数据数组
} CvLSVMFeatureMapCaskade;
```

核心函数：
- **`getFeatureMaps()`** — 计算图像的梯度直方图，生成 18 维有符号 + 9 维无符号方向直方图
- **`normalizeAndTruncate()`** — 4 邻域归一化 + 截断，扩展为 108 维（9×4 方向 + 18×4 方向）
- **`PCAFeatureMaps()`** — 降维至 31 维（18 维有符号 + 9 维无符号 + 4 维归一化因子）

#### `src/labdata.hpp` — Lab 颜色聚类中心

包含 15 个 Lab 颜色空间的聚类中心（通过对自然图像进行 k-means 聚类获得）。用于 Lab 特征量化：将每个像素归入最近的聚类中心，统计每个 cell 内各聚类出现的频率。

---

## 核心算法原理

### KCF 跟踪流程

```
          ┌─────────────────────────────────────────────┐
          │              输入: 视频帧序列                  │
          └──────────────┬──────────────────────────────┘
                         ▼
          ┌─────────────────────────────┐
          │  首帧?                       │
          │  → init(roi, image)         │
          │    • 提取目标区域特征 (x)     │
          │    • 生成高斯标签 (y)        │
          │    • 训练滤波器:            │
          │      k = gaussianKernel(x,x)│
          │      α = y / (FFT(k) + λ)  │
          └──────────┬──────────────────┘
                     ▼  (后续帧)
          ┌─────────────────────────────┐
          │  1. 提取搜索区域特征 (z)     │
          │  2. 检测:                   │
          │    k_z = gaussianKernel(x,z)│
          │    resp = FFT⁻¹(α ⊙ FFT(k))│
          │    pos = argmax(resp)       │
          │  3. 多尺度搜索 (可选)        │
          │  4. 亚像素峰值估计           │
          │  5. 更新目标位置             │
          │  6. 在线训练:               │
          │    x_new = (1-η)·x + η·z   │
          │    α_new = (1-η)·α + η·α'  │
          └─────────────────────────────┘
```

### 算法核心思想

#### 1. 循环移位与循环矩阵

KCF 的核心创新在于利用**循环矩阵**的性质。通过在目标区域采样一个基础样本，对其进行循环移位生成大量训练样本。这些样本构成的循环矩阵可以在频域通过 FFT 对角化：

```
X = F · diag(x̂) · Fᴴ
```

其中 x̂ = FFT(x)，F 是傅里叶矩阵。这使得原本 O(n³) 的矩阵运算降为 O(n log n)。

#### 2. 岭回归闭式解

在频域中，岭回归的闭式解简化为：

```
α̂ = ŷ / (k̂ˣˣ + λ)
```

其中 k̂ˣˣ 是核相关矩阵的傅里叶变换，ŷ 是高斯标签的傅里叶变换，λ 是正则化参数。

#### 3. 高斯核相关

对于 HOG 特征（多通道），高斯核相关计算为：

```
k(x, x') = exp(-||x - x'||² / (2σ²))
```

利用 FFT 加速互相关计算：

```
k̂ˣˣ' = FFT(k(x, x'))
```

#### 4. 检测

在新帧中，检测响应图通过下式计算：

```
ŷ' = α̂ ⊙ k̂ˣᶻ
y' = FFT⁻¹(ŷ')
```

响应图的最大值位置即为目标的位移量。

#### 5. 亚像素精度

通过三点抛物线拟合在峰值相邻位置进行插值，获得亚像素精度的位移：

```
subpixel = 0.5 * (right - left) / (2 * center - right - left)
```

#### 6. 多尺度检测

每帧在三个尺度上检测目标：
- **原始尺度** (scale = 1.0)
- **缩小尺度** (scale = 1.0 / scale_step)
- **放大尺度** (scale = scale_step)

选择加权后响应最高的尺度，其中 `scale_weight` 用于对非原始尺度的响应进行惩罚。

### 特征提取

#### HOG 特征（FHOG）

1. 计算图像 x/y 方向的梯度
2. 对每个像素，在 18 个有符号方向 + 9 个无符号方向进行软投票
3. 将像素聚合到 cell（4×4 像素），对每个 cell 内的像素进行三线性插值
4. 4 邻域（2×2 cells）block 级归一化，截断阈值为 0.2
5. PCA 降维至 31 维特征向量

#### Lab 颜色特征

1. 将图像从 BGR 转换到 Lab 颜色空间
2. 对每个像素，找到 15 个预计算聚类中心中的最近邻
3. 在每个 cell 内统计各聚类出现的频率，生成 15 维直方图
4. 与 HOG 特征向量拼接

#### 灰度原始特征（CSK）

1. 图像灰度化并归一化到 [0, 1]
2. 减去 0.5 进行零中心化
3. 即为 1 通道特征

### 在线更新策略

采用线性插值进行平滑更新：

```
α̂_new = (1 - η) · α̂_old + η · α̂_current
x_tmpl_new = (1 - η) · x_tmpl_old + η · x_current
```

其中 η = `interp_factor` 控制学习速率。较小的 η 保留更多历史信息，跟踪更稳定。

---

## 初始化流程详解

```
  main()
    │
    ├─ 解析命令行参数 (HOG/FIXEDWINDOW/MULTISCALE/LAB/SILENT)
    │
    ├─ KCFTracker(HOG, FIXEDWINDOW, MULTISCALE, LAB)
    │    │
    │    ├─ 设置默认参数 (lambda, padding, output_sigma_factor)
    │    ├─ HOG 模式: interp_factor=0.012, sigma=0.6, cell_size=4
    │    │    └─ Lab 模式: interp_factor=0.005, sigma=0.4, cell_size=4
    │    ├─ RAW 模式: interp_factor=0.075, sigma=0.2, cell_size=1
    │    ├─ MultiScale: template_size=96, scale_step=1.05, scale_weight=0.95
    │    └─ SingleScale+Fixed: template_size=96, scale_step=1
    │
    ├─ 读取 region.txt (解析 VOT 格式的 4 角点标注)
    ├─ 计算初始目标矩形 (xMin, yMin, width, height)
    │
    ├─ tracker.init(rect, frame)                     ← 第 0 帧
    │    │
    │    ├─ _roi = rect (保存目标矩形)
    │    ├─ _tmpl = getFeatures(image, 1)            ← 提取特征
    │    │    │
    │    │    ├─ 计算 padded 尺寸 (roi * padding)
    │    │    ├─ 计算 _scale、_tmpl_sz
    │    │    ├─ 对齐到 cell_size 和偶数
    │    │    ├─ 提取子窗口 + resize 到模板尺寸
    │    │    ├─ HOG: getFeatureMaps → normalize → PCA → 31-dim
    │    │    │   └─ Lab: 量化→直方图→拼接到 HOG 特征 (46-dim)
    │    │    ├─ 灰度: gray→[0,1], 减 0.5, 1-channel
    │    │    └─ 应用汉宁窗 (createHanningMats + mul)
    │    │
    │    ├─ _prob = createGaussianPeak(rows, cols)   ← 高斯标签 (频域)
    │    ├─ _alphaf = zeros(rows, cols, 2ch)         ← 初始化滤波器
    │    └─ train(_tmpl, 1.0)                        ← 首次训练
    │         │
    │         ├─ k = gaussianCorrelation(x, x)       ← 自核相关
    │         ├─ alphaf = y / (FFT(k) + λ)           ← 频域求解
    │         └─ 更新 _tmpl 和 _alphaf (插值系数=1)
    │
    └─ while (每帧)                                  ← 第 1..N 帧
         │
         └─ 见下文"运行流程"
```

---

## 运行流程详解

```
  while (读取 images.txt 中每帧图像)
  │
  ├─ 第 0 帧: 调用 tracker.init() (如上)
  │
  ├─ 第 1..N 帧: result = tracker.update(frame)
  │    │
  │    ├─ 边界检查: 确保 ROI 不超出图像范围
  │    │
  │    ├─ 保存目标中心 (cx, cy)
  │    │
  │    ├─ 检测 ──────────────────────────────────────────
  │    │    │
  │    │    ├─ z = getFeatures(image, 0, 1.0)     ← 提取搜索区域特征
  │    │    │    (同初始化，但 inithann=false，跳过汉宁窗初始化)
  │    │    │
  │    │    ├─ detect(tmpl, z)                    ← 核心检测
  │    │    │    │
  │    │    │    ├─ k = gaussianCorrelation(z, tmpl)  ← 互核相关
  │    │    │    ├─ res = real(FFT⁻¹(α ⊙ FFT(k)))    ← 响应图
  │    │    │    ├─ minMaxLoc → 峰值位置 (pi)
  │    │    │    ├─ 亚像素插值 → 浮点偏移 (p)
  │    │    │    └─ 中心对齐 → 返回位移向量
  │    │    │
  │    │    └─ 多尺度搜索 ────────────────────────────
  │    │         │  (当 scale_step != 1 时)
  │    │         │
  │    │         ├─ 缩小尺度 (1/scale_step):
  │    │         │   → z_small = getFeatures(image, 0, 1/scale_step)
  │    │         │   → detect(tmpl, z_small)
  │    │         │   → if (scale_weight * peak > best_peak): 采纳
  │    │         │
  │    │         ├─ 放大尺度 (scale_step):
  │    │         │   → z_big = getFeatures(image, 0, scale_step)
  │    │         │   → detect(tmpl, z_big)
  │    │         │   → if (scale_weight * peak > best_peak): 采纳
  │    │         │
  │    │         └─ 更新 _roi.width/height 和 _scale
  │    │
  │    ├─ 更新目标位置 ──────────────────────────────────
  │    │    │
  │    │    ├─ _roi.x = cx - w/2 + res.x * cell_size * _scale
  │    │    ├─ _roi.y = cy - h/2 + res.y * cell_size * _scale
  │    │    └─ 边界检查
  │    │
  │    └─ 在线训练 ──────────────────────────────────────
  │         │
  │         ├─ x = getFeatures(image, 0)           ← 新位置提取特征
  │         ├─ k = gaussianCorrelation(x, x)       ← 自核相关
  │         ├─ alphaf = y / (FFT(k) + λ)           ← 新滤波器
  │         ├─ _tmpl = (1-η)·_tmpl + η·x          ← 更新模板
  │         └─ _alphaf = (1-η)·_alphaf + η·alphaf ← 更新滤波器
  │
  └─ 输出 result.x, result.y, result.width, result.height → output.txt
```

---

## 构建与使用

### 环境要求

- OpenCV 4.x（推荐 4.10.0）
- 编译器：MSVC (VS 2022) 或 GCC
- CMake >= 3.10

### Windows (VS2022) 构建

```bash
# 从 VS 2022 开发人员命令提示符执行
cmake -B build -DOpenCV_DIR="path/to/opencv/build_for_VS"
cmake --build build
```

或在 VS2022 中直接打开项目文件夹，使用 `CMakePresets.json` 自动配置。

### Linux 构建

```bash
cmake .
make
```

### 运行

```bash
# 基本 KCF (HOG 特征)
./KCF

# KCF + Lab 颜色特征
./KCF lab

# CSK (灰度原始像素特征)
./KCF gray

# 显示跟踪窗口
./KCF show

# 单尺度 + 固定窗口
./KCF singlescale fixed_window show
```

### 输入数据格式

**images.txt**：
```
frame_000001.jpg
frame_000002.jpg
frame_000003.jpg
...
```

**region.txt**（VOT 格式，4 角点的 x,y 坐标，以逗号分隔）：
```
100.5,200.3,150.7,200.3,150.7,250.1,100.5,250.1
```

---

## 项目功能特性

1. **三种特征模式**：HOG、HOG+Lab、灰度原始像素（CSK）
2. **多尺度检测**：三个尺度的自适应目标搜索
3. **亚像素精度**：三点抛物线插值实现浮点坐标输出
4. **固定/自适应窗口**：支持固定模板尺寸或自适应 ROI
5. **高速跟踪**：利用 FFT 实现 O(n log n) 复杂度
6. **在线学习**：每帧增量更新目标模板和滤波器
7. **VOT 兼容**：符合 VOT 评测工具包接口标准
8. **可调参数**：所有核心参数均可自定义
