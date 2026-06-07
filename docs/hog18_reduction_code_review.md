# HOG18 降维与项目代码审查报告

## 结论摘要

当前 HOG31 到 HOG18 的降维实现整体是合理的，符合 `docs/phase2_hog_reduction_plan.md` 中定义的方案：先完整生成 FHOG31，再保留 `PCAFeatureMaps()` 输出中的前 18 个方向梯度通道。该实现不会破坏后续 KCF 的 `size_patch[2]`、Hanning 窗口和 Gaussian correlation 维度传播。

但项目中仍有几个需要优先处理的问题：

- `CN` 配置现在可以被读取并启用，但 `KCFTracker` 中并没有真正提取或融合 CN 特征。
- `getFeatureMaps()`、`normalizeAndTruncate()`、`PCAFeatureMaps()`、`selectFeatureMapChannels()` 的返回值没有检查，异常时可能继续使用无效特征图。
- HOG18 目前只测试了通道裁剪函数本身，没有测试 `KCFTracker::getFeatures()` 实际输出维度是否随配置变成 18。
- `RunKCF` 的默认配置路径仍然硬编码为本机绝对路径，不利于迁移和复现实验。

## HOG18 降维操作检查

### 当前实现路径

HOG 特征提取路径位于 `src/kcftracker.cpp`：

```text
getFeatureMaps()
-> normalizeAndTruncate()
-> PCAFeatureMaps()
-> selectFeatureMapChannels()
-> size_patch[2] = map->numFeatures
```

对应代码：

- `src/kcftracker.cpp:421-429`：在 HOG 分支中完整生成 FHOG31，并在 `_hogChannels` 小于当前通道数时裁剪。
- `src/kcftracker.cpp:430-436`：裁剪后用 `map->numFeatures` 更新 `size_patch[2]`，随后构造 `FeaturesMap` 并释放原始 map。
- `src/fhog.cpp:484-516`：`selectFeatureMapChannels()` 为每个 cell 拷贝前 `selectedFeatures` 个通道，并更新 `map->numFeatures`。

### 通道语义是否正确

`PCAFeatureMaps()` 的输出顺序是清楚的：

- `src/fhog.cpp:441-450`：先写入 `xp * 2` 个通道，即 `9 * 2 = 18` 个方向梯度通道。
- `src/fhog.cpp:451-460`：再写入 9 个无符号梯度聚合通道。
- `src/fhog.cpp:461-469`：最后写入 4 个纹理/归一化能量通道。

因此，如果当前实验定义是：

```text
HOG18 = FHOG31 的前 18 个方向梯度通道
```

那么 `selectFeatureMapChannels(map, 18)` 的做法是正确的。

### 维度传播是否正确

裁剪后的 `map->numFeatures` 会被同步写入 `size_patch[2]`：

```text
src/kcftracker.cpp:430-432
```

后续：

- `createHanningMats()` 会按新的 `size_patch[2]` 生成 Hanning 窗口。
- `gaussianCorrelation()` 会按新的 `size_patch[2]` 遍历特征通道。
- `FeaturesMap = FeaturesMap.t()` 后，特征矩阵形状会从 `31 x cells` 变成 `18 x cells`。

所以从 KCF 主流程看，HOG18 不存在明显的维度不匹配问题。

### 性能收益预期

当前降维是在 `PCAFeatureMaps()` 之后做的，因此：

- Gaussian correlation、模板更新、Hanning 乘法等后续计算会从 31 通道降到 18 通道。
- 但 `getFeatureMaps()`、`normalizeAndTruncate()`、`PCAFeatureMaps()` 仍然完整计算 FHOG31。

这意味着 HOG18 会减少后半段相关滤波计算量，但不会减少 FHOG 前半段特征生成开销。如果后续发现速度提升不足，可以考虑把 HOG18 分支前移到 `PCAFeatureMaps()` 内部或新建 `PCAFeatureMaps(map, outputChannels)`，只生成前 18 通道。

## 主要问题与改进建议

### P1: CN 配置可启用但没有实际生效

位置：

- `src/app_config.cpp:119-122`
- `src/kcftracker.cpp:171-174`
- `src/kcftracker.cpp:421-436`
- `src/runkcf.cpp:174-177`

问题：

配置允许 `features.cn.enabled = 1` 且 `channels = 4`，`KCFTracker` 构造函数也保存了 `_cnfeatures`、`_cnChannels` 和 `_fusionMode`。但是 `getFeatures()` 中只实现了 HOG 和 Lab，没有任何 CN 特征提取或拼接逻辑。

`RunKCF` 只打印了提示：

```text
CN feature extraction will be connected in Phase 2.3.
```

这能避免误解一部分日志，但仍然会导致 `hog31_cn4.yaml` 和 `hog18_cn4.yaml` 可以运行出结果，而这些结果实际不是 HOG+CN。

建议：

- 在 CN 尚未实现前，`validateAppConfig()` 应直接拒绝 `cn.enabled = 1`。
- 或者在 `RunKCF` 中把 CN 配置作为预留配置处理，检测到启用 CN 时直接退出。
- 文档和实验结果中不要把 `hog31_cn4.yaml`、`hog18_cn4.yaml` 描述为 CN 已生效。

### P1: 特征提取函数返回值未检查

位置：

- `src/kcftracker.cpp:423-428`

问题：

当前代码连续调用：

```cpp
getFeatureMaps(z, cell_size, &map);
normalizeAndTruncate(map, 0.2f);
PCAFeatureMaps(map);
selectFeatureMapChannels(map, _hogChannels);
```

但没有检查任何返回值。如果图像尺寸异常、内存分配失败、`map` 为空或通道裁剪失败，后续会继续访问 `map->sizeY`、`map->sizeX`、`map->numFeatures`，可能造成崩溃或错误结果。

建议：

- 检查每一步返回值。
- 如果失败，抛出明确异常或返回空 Mat 并在上层停止跟踪。
- 至少对 `map == NULL` 做防御性判断。

### P2: HOG18 测试覆盖不够完整

位置：

- `tests/hog_feature_tests.cpp:13-41`

当前测试验证了：

- 2 个 cell、31 通道输入时，裁剪后变成 18 通道。
- 第 1 个 cell 保留原始通道 1-18。
- 第 2 个 cell 保留原始通道 32-49。

这说明 `selectFeatureMapChannels()` 的基础拷贝逻辑是正确的。

不足：

- 没有验证 `selectedFeatures == map->numFeatures` 的 no-op 分支。
- 没有验证非法参数，例如 0、负数、大于原通道数。
- 没有验证 `KCFTracker` 在 `hog.channels = 18` 时实际输出 `size_patch[2] == 18`。
- 没有验证 HOG31 baseline 不被 HOG18 配置影响。

建议：

- 给 `selectFeatureMapChannels()` 增加边界测试。
- 给 `KCFTracker` 增加测试入口或只读调试接口，能查询当前特征通道数。
- 至少增加一个小图像 smoke test：同一张图分别用 31 和 18 配置初始化，确认通道数分别为 31 和 18。

### P2: `RunKCF` 默认配置路径硬编码到本机绝对路径

位置：

- `src/runkcf.cpp:21-22`

问题：

默认配置路径是：

```text
D:\code\cpp\vs\myKCF\joaofaro\joao\configs\baseline_hog31.yaml
```

这对当前机器可用，但不利于其他机器、CI 或后续协作者复现实验。

建议：

- 保留命令行传入配置路径作为主入口。
- 默认路径改成相对路径 `configs/baseline_hog31.yaml`。
- 或要求必须传入配置文件，不再提供硬编码默认值。

### P2: 配置校验没有检查输入文件是否存在

位置：

- `src/app_config.cpp:95-130`
- `src/runkcf.cpp:180-192`

问题：

`validateAppConfig()` 只检查路径字符串是否为空，没有检查文件是否存在。实际错误会延迟到打开 annotation 或 video 时暴露。

建议：

- 如果保持 `loadAppConfig()` 只负责语义校验，可以保持现状。
- 如果希望配置加载阶段更严格，可以用 `std::ifstream` 或 OpenCV 打开检查输入文件。
- 对输出视频路径，建议在 `save_video = true` 时检查输出目录是否存在。

### P2: `output.video_path` 在不保存视频时仍是必填

位置：

- `src/app_config.cpp:170-171`

问题：

即使 `output.save_video = 0`，配置文件仍必须包含 `output.video_path`。这不影响当前运行，但配置语义不够自然。

建议：

- 当 `save_video = false` 时允许 `video_path` 为空。
- 当 `save_video = true` 时再要求 `video_path` 非空，并检查目录可写。

### P3: `KCFTracker` 头文件依赖配置模块，耦合略高

位置：

- `src/kcftracker.hpp:96-97`

问题：

`KCFTracker` 现在直接包含 `app_config.hpp`，并暴露：

```cpp
KCFTracker(const TrackerConfig& tracker_config, const FeatureConfig& feature_config);
```

这让核心 tracker 依赖应用层配置结构。短期可以接受，但后续如果要复用 KCFTracker 或保持原始 `KCF` 入口简洁，建议降低耦合。

建议：

- 保留原始 bool 构造函数。
- 新增一个更通用的 `KCFTrackerOptions`，放在 tracker 模块内部。
- `RunKCF` 负责把 `AppConfig` 转换成 `KCFTrackerOptions`。

### P3: 仍有旧式 C 内存管理，后续扩展容易出错

位置：

- `src/fhog.cpp:432`
- `src/fhog.cpp:477-479`
- `src/fhog.cpp:499-514`

问题：

FHOG 代码来自旧 OpenCV Latent SVM 风格，使用 `malloc/free` 和裸指针。当前裁剪函数沿用了这种风格，和原代码一致，但后续继续增加 CN、融合、尺度滤波时，裸指针会增加泄漏和异常路径风险。

建议：

- 短期保持 FHOG 原实现，避免大改引入误差。
- 新增代码尽量使用 `cv::Mat`、`std::vector<float>` 或 RAII 包装。
- 对 `CvLSVMFeatureMapCaskade*` 可以做一个小型释放包装，保证异常路径自动释放。

## 对当前 HOG18 实现的最终判断

如果实验定义是“从 FHOG31 中保留前 18 个方向梯度通道”，当前实现是正确的。

它的优点是：

- 行为确定，不需要训练数据。
- 与 HOG31 baseline 的差异集中在通道数量上，适合做第一轮消融实验。
- 对 KCF 后续矩阵维度传播没有明显问题。

它的限制是：

- 不是严格意义上的 PCA 降到 18 维，而是 FHOG31 后处理通道选择。
- 特征生成前半段仍计算完整 FHOG31，速度收益主要来自后续相关滤波计算。
- 当前测试还没有覆盖 tracker 级别的实际输出维度。

## 建议的下一步修改顺序

1. 暂时禁止 `cn.enabled = 1` 的配置运行，直到 CN4 真正实现。
2. 在 `KCFTracker::getFeatures()` 中检查 FHOG 每一步返回值。
3. 扩展 `HogFeatureTests`，覆盖非法通道数和 no-op 分支。
4. 给 tracker 增加只读调试接口或测试友元，用于验证实际特征通道数。
5. 把 `RunKCF` 默认配置路径从绝对路径改成相对路径，或强制要求命令行传入配置。

## 验证说明

本次检查以静态代码审查为主，没有继续运行构建命令。此前 `HogFeatureTests.exe` 曾输出：

```text
HOG feature tests passed.
```

但该命令在工具侧超时，因此不能把它记录为完整的稳定测试通过。后续建议在 Visual Studio CMake 集成环境中运行 `HogFeatureTests` 和两组 `RunKCF` 配置，并记录 HOG31/HOG18 的平均耗时、CLE、IoU 和 `CLE < 2 px` 比例。
