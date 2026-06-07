# Phase 2.2 HOG 特征降维初步计划

## Summary

Phase 2.2 的目标是把当前 FHOG31 特征扩展为可配置的 `HOG31` / `HOG18` 两种实验版本，用于论文对照实验。

当前 FHOG 流程为：

```text
27维原始梯度 -> 108维归一化特征 -> 31维 PCAFeatureMaps 输出
```

本阶段采用确定性路线：

```text
HOG18 = FHOG31 输出中的前 18 个方向梯度通道
```

不引入离线 PCA、在线 PCA 或额外训练数据。

## 技术路线

当前 `PCAFeatureMaps()` 输出 31 维，结构为：

- 前 18 维：方向梯度特征。
- 中间 9 维：无符号梯度聚合特征。
- 最后 4 维：纹理/归一化能量特征。

本阶段实现规则：

- `hog.channels == 31`：保持完整 FHOG31，不改变 baseline。
- `hog.channels == 18`：在 FHOG31 输出后保留前 18 个通道。
- 通道裁剪后同步更新 `map->numFeatures`，后续 `size_patch[2]`、Hanning 窗口和 Gaussian correlation 自动使用新维度。

第一版不把裁剪逻辑前移到 `PCAFeatureMaps()` 内部。这样可以先保证 HOG18 的行为正确、可复现、容易和 baseline 对照。

## 对比方案

| 方案 | 优点 | 缺点 | 结论 |
| --- | --- | --- | --- |
| 保留前 18 维 | 实现稳定、无训练依赖、最适合快速消融实验 | 特征提取前半段仍会计算 31 维中间结果 | 采用 |
| 离线 PCA 到 18 维 | 更符合数学降维概念 | 需要训练样本、投影矩阵和额外实验管理 | 暂不采用 |
| 在线自适应 PCA | 研究空间大 | 改动大、变量多，会干扰 KCF 主实验 | 不采用 |

## 实施要点

- 新增 `selectFeatureMapChannels()`，用于从已有 feature map 中保留前 N 个通道。
- 在 `KCFTracker::getFeatures()` 中，`PCAFeatureMaps()` 之后根据 `_hogChannels` 执行通道选择。
- `configs/baseline_hog31.yaml` 继续使用 31 通道。
- `configs/hog18.yaml` 的 `features.hog.channels: 18` 开始真正影响特征维度。
- Phase 2.2 不引入 CN 特征。

## Test Plan

构建验证：

- `KCF.exe` 和 `RunKCF.exe` 都应成功编译。
- 新增 `HogFeatureTests` 测试目标，用于验证通道裁剪逻辑。

配置验证：

- 运行 `baseline_hog31.yaml`，确认实际 HOG 通道数为 31。
- 运行 `hog18.yaml`，确认实际 HOG 通道数为 18。

行为验证：

- HOG31 结果应与 Phase 2.1 baseline 保持一致或仅有极小浮点差异。
- HOG18 能完整跑完视频，不崩溃、不出现维度不匹配。

指标对照：

- 平均 KCF 耗时。
- 平均 FPS。
- 平均 CLE。
- 平均 IoU。
- `CLE < 2 px` 比例。

## Assumptions

- HOG18 的定义锁定为 FHOG31 的前 18 个方向梯度通道。
- Phase 2.2 不引入 CN 特征。
- Phase 2.2 不引入离线 PCA、在线 PCA 或训练数据。
- 初版优先保证对照实验成立；如果 HOG18 速度提升不足，再做第二轮 FHOG 内部生成优化。
