# 第二阶段：KCF 特征提取优化初步计划

## 阶段目标

第二阶段围绕 KCF 的特征提取部分做优化，包含两个方向：

1. HOG 特征降维。
2. 引入 CN 颜色特征。

本阶段的核心目标不是一次性把所有特征堆到一起，而是建立可控、可复现实验流程，逐步确认每个特征改动对速度、精度和稳定性的影响。

## 推荐实施顺序

建议分批进行，不建议两个操作同时实现。

推荐顺序：

1. 先做 HOG 特征降维。
2. 再做 CN 颜色特征。
3. 最后做 HOG 降维 + CN 的组合实验。

原因：

- HOG 是当前 KCF 实现里的主要特征，先优化它可以保持系统结构变化较小。
- HOG 降维主要影响特征通道数、计算量和相关滤波器输入维度，适合作为第二阶段的第一步。
- CN 特征会引入新的颜色表征和特征融合问题，变量更多，适合在 HOG 降维稳定后再加入。
- 分阶段实现可以支持清晰的消融实验：Baseline、HOG 降维、CN 单独引入、HOG 降维 + CN 融合。

## 阶段划分

### Phase 2.1：整理特征配置入口

目的：

- 在正式改特征前，先让特征开关具备清晰的配置方式。
- 为后续实验保留统一入口，避免每次改算法都需要改大量代码。

建议工作：

- 梳理当前 `KCFTracker` 中 HOG、gray、Lab 等特征开关的作用位置。
- 明确 `RunKCF` 中实验配置如何传递到 `KCFTracker`。
- 预留后续配置项，例如：

```text
use_hog
use_hog_dim_reduction
hog_reduced_dim
use_cn
feature_fusion_mode
```

产出：

- 一个稳定的 Baseline 配置。
- 一个能明确打开或关闭特征模块的配置结构。

### Phase 2.2：HOG 特征降维

目的：

- 降低 HOG 特征通道数。
- 减少相关滤波器训练和检测时的计算量。
- 观察速度提升是否会显著牺牲精度。

建议工作：

- 先分析当前 FHOG 输出维度。
- 再选择降维策略。
- 第一版建议采用固定通道选择或轻量线性变换，不建议一开始就引入复杂在线 PCA。

推荐优先级：

1. 固定通道裁剪或合并。
2. 离线 PCA 投影矩阵。
3. 在线 PCA 或自适应降维。

原因：

- 固定通道裁剪最容易验证，不容易破坏原始 KCF 流程。
- 离线 PCA 更适合后续做严谨实验，但需要额外训练数据和投影矩阵管理。
- 在线 PCA 改动最大，容易把问题从“特征降维”扩大到“在线特征学习”。

产出：

- `Baseline HOG` 与 `Reduced HOG` 的速度和精度对照。
- 重点观察：
  - 平均 KCF 耗时。
  - 平均 FPS。
  - 平均 CLE。
  - 平均 IoU。
  - `CLE < 2 px` 比例。

### Phase 2.3：引入 CN 颜色特征

目的：

- 增强 KCF 对颜色信息的表达能力。
- 弥补 HOG 对颜色差异不敏感的问题。
- 提升复杂背景下的目标区分能力。

建议工作：

- 先实现 CN 特征提取模块。
- 再单独验证 CN 特征是否能正常输出稳定特征图。
- 然后考虑与 HOG 的融合。

融合策略建议分两步：

1. 特征通道级拼接。
2. 响应图级加权融合。

第一版建议优先尝试通道级拼接，因为它更接近原始 KCF 多通道特征处理方式。

需要注意：

- CN 特征通常需要颜色名查表或颜色映射表。
- 如果项目中已有 `labdata.hpp` 相关颜色数据，可以先评估它是否能复用。
- CN 引入后要特别关注特征归一化，否则 HOG 和 CN 的数值尺度可能不一致。

产出：

- `Baseline HOG` 与 `HOG + CN` 的精度对照。
- 如果实现了 CN 单独模式，也可以增加 `CN only` 对照。

### Phase 2.4：HOG 降维 + CN 组合实验

目的：

- 验证 HOG 降维带来的速度收益，是否可以被 CN 特征补偿精度损失。
- 找到速度和精度之间更合适的折中点。

建议实验组合：

| 实验编号 | HOG | HOG 降维 | CN | 用途 |
| --- | --- | --- | --- | --- |
| Exp-0 | 开 | 关 | 关 | 原始 Baseline |
| Exp-1 | 开 | 开 | 关 | 只看 HOG 降维影响 |
| Exp-2 | 开 | 关 | 开 | 只看 CN 增益 |
| Exp-3 | 开 | 开 | 开 | 最终组合方案 |
| Exp-4 | 关 | 关 | 开 | 可选，观察 CN 单独能力 |

核心判断：

- 如果 `Exp-1` 速度提升明显但精度下降，可以看 `Exp-3` 是否通过 CN 把精度补回来。
- 如果 `Exp-2` 精度提升但速度下降明显，需要评估 CN 通道数或融合方式。
- 如果 `Exp-3` 同时保持较高 IoU 和较低耗时，可以作为后续 fDSST 阶段的基础版本。

## Git 分支建议

建议使用多个分支管理第二阶段，避免不同实验互相污染。

推荐分支结构：

```text
main 或当前稳定分支
  └── codex/phase2-feature-config
        └── codex/phase2-hog-reduction
              └── codex/phase2-cn-feature
                    └── codex/phase2-hog-cn-fusion
```

也可以采用并行分支：

```text
codex/phase2-feature-config
codex/phase2-hog-reduction
codex/phase2-cn-feature
codex/phase2-hog-cn-fusion
```

推荐做法：

- 先完成 `feature-config`，作为第二阶段公共基础。
- `hog-reduction` 从 `feature-config` 分出。
- `cn-feature` 也从 `feature-config` 分出。
- `hog-cn-fusion` 在两个方向都验证后再合并实现。

这样可以分别保存：

- 纯 HOG 降维结果。
- 纯 CN 引入结果。
- HOG 降维 + CN 融合结果。

## 配置文件建议

第二阶段建议逐步从代码顶部硬编码配置，过渡到外部配置文件。

推荐目录：

```text
configs/
  baseline.json
  hog_reduction.json
  hog_cn.json
  hog_reduction_cn.json
```

每个配置文件只描述实验变量，例如：

```json
{
  "video_path": "D:\\code\\cpp\\vs\\myKCF\\joaofaro\\joao\\source\\d4.mp4",
  "annotation_path": "C:\\Users\\yala5\\Desktop\\auav_data\\videos\\d4.txt",
  "save_video_output": false,
  "features": {
    "hog": true,
    "hog_dim_reduction": false,
    "hog_reduced_dim": 18,
    "cn": false,
    "lab": false
  }
}
```

初期可以先不立即实现完整配置系统，但第二阶段开始时应该为它预留结构。

## 测试与评估方式

每完成一个小阶段，都使用 `RunKCF` 进行同一视频、同一标注文件的对照测试。

统一记录指标：

- 平均 KCF 耗时。
- 平均 FPS。
- 平均 CLE。
- 平均 IoU。
- `CLE < 2 px` 比例。

建议每次实验保存：

```text
实验配置
git 分支名
git commit id
视频路径
标注路径
终端统计结果
是否保存输出视频
```

推荐后续新增实验结果目录：

```text
run/experiments/
  baseline/
  hog_reduction/
  hog_cn/
  hog_reduction_cn/
```

## 风险点

### HOG 降维风险

- 降维后特征表达能力下降，可能导致 CLE 增大、IoU 降低。
- 通道数变化会影响 `KCFTracker` 内部矩阵尺寸，需要保证训练和检测阶段一致。
- 如果降维方式不稳定，可能导致响应图异常。

### CN 特征风险

- CN 颜色映射表来源和格式需要明确。
- CN 与 HOG 数值尺度不同，直接拼接可能导致某一类特征主导响应。
- 颜色特征对光照变化敏感，需要观察不同视频场景下的稳定性。

### 实验管理风险

- 如果不分支管理，多个特征改动混在一起后很难判断性能变化来源。
- 如果不保存配置和 commit id，后续实验结果难以复现。

## 建议的第二阶段工作顺序

最终建议按以下顺序推进：

1. 建立第二阶段公共配置入口。
2. 固定 Baseline，并记录当前 `RunKCF` 指标。
3. 实现 HOG 特征降维。
4. 对比 Baseline 与 HOG 降维结果。
5. 实现 CN 特征提取。
6. 验证 CN 单独输出和 HOG + CN 融合。
7. 对比 Baseline、HOG 降维、HOG + CN、HOG 降维 + CN。
8. 选择表现最稳定的版本，作为第三阶段 fDSST 尺度滤波器的基础。

## 初步结论

第二阶段不建议同时改 HOG 降维和 CN 特征。

推荐采用：

```text
先 HOG 降维，后 CN 特征，最后融合验证。
```

这样可以保证每一步都有单独的实验结论，也方便后续论文式或报告式对照分析。
