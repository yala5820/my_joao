# Phase 3：置信度机制与模板更新控制总结

## 1. 阶段目标

Phase 3 的目标是在现有 KCF 跟踪流程中引入“置信度机制和模板更新控制”，使算法不再对每一帧都使用固定学习率更新模板，而是根据检测响应质量判断当前跟踪结果的可信程度，并据此控制：

- 是否接受检测位置。
- 是否接受多尺度检测得到的 scale。
- 是否更新模板。
- 模板更新时使用多大的学习率。
- 是否更新历史置信度基准。

本阶段只继续改进 `hog18` 和 `hog18_cn4` 两条实验线，不继续扩展 `hog31` 和 `hog31_cn4` 的置信度版本。

## 2. 已完成的工作

### 2.1 新增置信度配置接口

在 `AppConfig` 中新增了 `ConfidenceConfig` 配置结构，并通过 YAML 的 `confidence` 段统一管理置信度相关参数。

当前支持的配置项如下：

```yaml
confidence:
  enabled: 1
  psr_exclusion_radius: 5
  eps: 0.000001
  warmup_frames: 5
  ema_alpha: 0.05
  medium_ema_alpha: 0.0
  high_ratio: 0.95
  low_ratio: 0.60
  medium_lr_factor: 0.30
  low_displacement_threshold: 0.50
  low_displacement_damping: 0.50
```

旧配置文件如果没有 `confidence` 段，则默认：

```yaml
confidence:
  enabled: 0
```

这保证了 `hog18.yaml` 和 `hog18_cn4.yaml` 的原始行为保持不变。

### 2.2 新增置信度计算模块

新增文件：

- `src/confidence.hpp`
- `src/confidence.cpp`

该模块负责：

- 从 response map 中计算 `peak`。
- 计算 PSR。
- 计算 APCE。
- 管理 PSR/APCE 的历史 EMA 基准。
- 根据当前 PSR/APCE 与 EMA 基准的关系划分置信度等级。
- 输出本帧诊断信息。

新增的核心结构包括：

- `ResponseStats`：保存 `peak`、`psr`、`apce`。
- `ConfidenceDiagnostics`：保存逐帧诊断字段。
- `ConfidenceEstimator`：维护 warmup、EMA 和 high/medium/low 分级逻辑。
- `ConfidenceLevel`：表示 `disabled`、`warmup`、`high`、`medium`、`low`。
- `PositionAction`：表示 `disabled`、`accept`、`damped`、`reject`。

### 2.3 改造 KCFTracker 检测阶段

在 `KCFTracker::detect()` 中保留 response map 的统计信息，并基于 response map 计算：

```text
peak = max(response)
PSR = (peak - mean(sidelobe)) / (std(sidelobe) + eps)
APCE = (peak - min)^2 / (mean((response - min)^2) + eps)
```

其中 PSR 的 sidelobe 会排除峰值附近 `psr_exclusion_radius` 个 response cell，避免峰值附近区域影响背景响应统计。

`detect()` 仍然返回原来的 sub-pixel 位移结果，但额外通过 `ResponseStats` 输出当前检测响应质量，供后续置信度判断使用。

### 2.4 改造 KCFTracker 更新阶段

原始 KCF 更新流程是：

```text
检测位置
更新 _roi / _scale
使用固定 interp_factor 更新模板
```

Phase 3 改造后，流程变为：

```text
保存上一帧 ROI 和 scale
多尺度检测只生成候选 ROI 和候选 scale
计算 response 指标 peak / PSR / APCE
根据 PSR/APCE 计算置信度等级
根据置信度等级决定是否提交候选 ROI / scale
根据置信度等级决定是否更新模板以及学习率大小
保存逐帧诊断信息
```

这意味着多尺度检测结果不再立即写入 `_roi` 和 `_scale`，而是先作为候选结果保存。最终是否接受候选位置和候选 scale，由置信度控制逻辑决定。

### 2.5 改造 RunKCF 输出

`RunKCF` 的逐帧终端输出和 txt 输出中追加了置信度诊断字段。

新增字段包括：

- `peak`
- `PSR`
- `APCE`
- `psr_ema`
- `apce_ema`
- `confidence`
- `displacement_ratio`
- `lr`
- `action`
- `template_updated`
- `scale_updated`
- `ema_updated`

这些字段用于分析每一帧为什么被判定为 high、medium 或 low，以及该判定实际导致了什么更新动作。

### 2.6 新增实验配置

新增通用置信度配置：

- `configs/hog18_conf.yaml`
- `configs/hog18_cn4_conf.yaml`

新增 DUT 图片序列置信度配置：

- `configs/dut_hog18_conf.yaml`
- `configs/dut_hog18_cn4_conf.yaml`

同时对 DUT 配置进行了命名整理：

- `dut_video14_baseline_hog31.yaml` 更名为 `dut_baseline_hog31.yaml`
- `dut_video14_hog18.yaml` 更名为 `dut_hog18.yaml`
- `dut_video14_hog31_cn4.yaml` 更名为 `dut_hog31_cn4.yaml`
- `dut_video14_hog18_cn4.yaml` 更名为 `dut_hog18_cn4.yaml`

### 2.7 新增测试

新增 `ConfidenceTests` 测试目标，覆盖：

- 单峰 response 的 PSR/APCE 高于平坦 response。
- `psr_exclusion_radius` 对 PSR 计算生效。
- warmup 后可以正确初始化 EMA。
- high 会更新 EMA。
- medium 默认不更新 EMA。
- low 不更新 EMA。

同时扩展 `DatasetIOTests`：

- 验证旧 YAML 无 `confidence` 段时默认 `enabled=false`。
- 验证带 `confidence` 段的 YAML 可以读取全部参数。
- 修复测试运行目录中 `run/` 不存在时无法创建临时配置文件的问题。

## 3. 现有置信度机制的完整逻辑

### 3.1 response map 的作用

KCF 每一帧检测时会生成一个 response map。response map 可以理解为目标在搜索区域中每个位置的匹配响应：

- 响应峰值越高，说明某个位置与当前模板越匹配。
- 峰值越尖锐，说明目标位置越明确。
- 如果 response map 中很多位置响应都接近，说明背景干扰较强，目标位置不明确。

Phase 3 使用 response map 的形状来估计检测可靠性，而不是只使用峰值位置。

### 3.2 peak

`peak` 是 response map 中的最大响应值：

```text
peak = max(response)
```

它表示当前帧检测到的最强匹配位置的响应强度。

需要注意的是，`peak` 只能说明“最高响应有多高”，不能说明“最高响应是否唯一”。复杂背景中，错误位置也可能产生较高 peak，因此只看 peak 不足以判断跟踪是否可靠。

### 3.3 PSR

PSR 全称是 Peak-to-Sidelobe Ratio，用来衡量峰值相对于旁瓣背景响应的突出程度。

计算方式：

```text
PSR = (peak - mean(sidelobe)) / (std(sidelobe) + eps)
```

其中：

- `peak` 是 response map 最大值。
- `sidelobe` 是排除峰值附近区域后的 response map 其余部分。
- `mean(sidelobe)` 表示背景响应均值。
- `std(sidelobe)` 表示背景响应标准差。
- `eps` 用于防止除零。

PSR 越高，说明峰值越突出，目标位置越明确。PSR 越低，说明背景响应与峰值差距不明显，检测结果更不可靠。

`psr_exclusion_radius` 控制排除峰值周围多大的区域。例如：

```yaml
psr_exclusion_radius: 5
```

表示计算 sidelobe 时排除峰值附近半径为 5 个 response cell 的区域。

### 3.4 APCE

APCE 用来衡量 response map 的整体振幅变化，常用于相关滤波跟踪中的置信度判断。

计算方式：

```text
APCE = (peak - min)^2 / (mean((response - min)^2) + eps)
```

其中：

- `peak` 是 response map 最大值。
- `min` 是 response map 最小值。
- 分子表示峰值和最小响应之间的振幅差。
- 分母表示整个 response map 相对最小值的平均能量。

APCE 越高，说明 response map 中的主峰更明显。APCE 越低，说明响应图整体不够尖锐，可能存在干扰或目标外观变化。

### 3.5 EMA 历史基准

单帧 PSR/APCE 的绝对值在不同视频、不同目标、不同特征组合下差异很大，因此当前实现没有使用固定阈值，而是使用历史 EMA 作为自适应基准。

当前维护两个历史基准：

```text
psr_ema
apce_ema
```

它们表示历史可靠响应质量的参考水平。

#### warmup 初始化

跟踪开始后，前 `warmup_frames` 个检测帧用于初始化 EMA：

```yaml
warmup_frames: 5
```

在 warmup 阶段，算法只统计 PSR/APCE 的均值：

```text
psr_ema = warmup 阶段 PSR 均值
apce_ema = warmup 阶段 APCE 均值
```

warmup 阶段的帧被标记为：

```text
confidence=warmup
```

warmup 阶段默认接受检测位置、接受 scale，并使用原始学习率更新模板。

### 3.6 置信度分级

warmup 完成后，每一帧根据当前 PSR/APCE 与 EMA 基准的比例关系划分为 high、medium 或 low。

#### high

高置信度条件：

```text
PSR >= high_ratio * psr_ema
且
APCE >= high_ratio * apce_ema
```

例如：

```yaml
high_ratio: 0.95
```

表示当前 PSR 和 APCE 都至少达到历史基准的 95%，才认为是 high。

high 表示当前 response map 质量接近或高于历史可靠水平，检测结果较可信。

#### low

低置信度条件：

```text
PSR < low_ratio * psr_ema
或
APCE < low_ratio * apce_ema
```

例如：

```yaml
low_ratio: 0.60
```

表示只要 PSR 或 APCE 低于历史基准的 60%，就判定为 low。

low 表示当前响应质量明显低于历史水平，检测结果可能不可靠。

#### medium

除 high 和 low 之外的情况都属于 medium：

```text
medium = 非 high 且非 low
```

medium 表示当前响应质量有所下降，但没有低到需要强拒绝的程度。

### 3.7 EMA 更新规则

不同置信度等级对 EMA 的更新规则不同。

#### high 更新 EMA

high 帧使用 `ema_alpha` 更新 EMA：

```text
psr_ema = (1 - ema_alpha) * psr_ema + ema_alpha * PSR
apce_ema = (1 - ema_alpha) * apce_ema + ema_alpha * APCE
```

例如：

```yaml
ema_alpha: 0.05
```

表示每次 high 帧只用 5% 的当前值更新历史基准，变化较平滑。

#### medium 默认不更新 EMA

medium 帧使用 `medium_ema_alpha` 控制是否更新 EMA：

```yaml
medium_ema_alpha: 0.0
```

默认值为 0，表示 medium 不更新 EMA。

如果设置为非零，例如：

```yaml
medium_ema_alpha: 0.02
```

则 medium 帧也会以较小权重更新 EMA。

#### low 不更新 EMA

low 帧不更新 EMA。

这样设计的初衷是避免错误检测结果污染历史置信度基准。但在复杂背景中，如果目标长期处于低响应状态，EMA 可能会停留在早期较高值，导致后续帧长期被判定为 low。

## 4. 模板更新控制逻辑

### 4.1 原始 KCF 更新逻辑

原始 KCF 每帧都会执行检测和模板更新：

```text
根据 response peak 得到目标位移
更新目标位置和 scale
提取新位置的特征
使用固定 interp_factor 更新模板
```

模板更新公式可以概括为：

```text
template = (1 - lr) * old_template + lr * new_feature
alphaf   = (1 - lr) * old_alphaf   + lr * new_alphaf
```

其中 `lr` 是学习率。原始 HOG KCF 中，`lr` 等于固定的 `interp_factor`，当前约为 `0.012`。

### 4.2 Phase 3 的候选结果机制

Phase 3 中，检测阶段不会立即提交位置和 scale，而是先生成候选结果：

```text
previous_roi
previous_scale
candidate_roi
candidate_scale
```

多尺度检测会分别测试：

- 当前 scale。
- 较小 scale。
- 较大 scale。

选择响应最高的候选结果后，再由置信度控制逻辑决定是否接受。

### 4.3 displacement_ratio

`displacement_ratio` 用于衡量候选位置相对于上一帧位置的移动幅度：

```text
displacement_ratio = candidate_center_shift / previous_roi_diagonal
```

其中：

- `candidate_center_shift` 是候选框中心和上一帧框中心之间的距离。
- `previous_roi_diagonal` 是上一帧目标框的对角线长度。

该值越大，说明本帧候选位置相对上一帧跳动越大。

### 4.4 high 的更新策略

high 表示当前 response map 质量较好，因此策略最接近原始 KCF：

```text
接受检测位置
接受检测 scale
模板学习率 = interp_factor
更新模板
更新 EMA
```

输出字段表现为：

```text
confidence=high
action=accept
lr=interp_factor
template_updated=true
scale_updated=true 或 false
ema_updated=true
```

如果 scale 没有变化，则 `scale_updated=false`。

### 4.5 medium 的更新策略

medium 表示检测结果有一定可信度，但响应质量不如 high。当前策略是：

```text
接受检测位置
接受检测 scale
模板学习率 = medium_lr_factor * interp_factor
更新模板
默认不更新 EMA
```

例如：

```yaml
medium_lr_factor: 0.30
```

如果原始 `interp_factor = 0.012`，则 medium 帧实际学习率为：

```text
0.30 * 0.012 = 0.0036
```

也就是说，medium 帧仍然更新模板，但更新幅度比原始 KCF 小。

输出字段表现为：

```text
confidence=medium
action=accept
lr=0.0036
template_updated=true
scale_updated=true 或 false
ema_updated=false
```

如果 `medium_ema_alpha` 设置为非零，则 medium 也可能输出 `ema_updated=true`。

### 4.6 low 且小偏移的更新策略

当置信度为 low，但候选位置相对于上一帧移动不大时：

```text
displacement_ratio <= low_displacement_threshold
```

当前策略是阻尼更新位置：

```text
只应用一部分检测位移
不更新模板
不更新 EMA
不更新 scale
```

阻尼系数由 `low_displacement_damping` 控制：

```yaml
low_displacement_damping: 0.50
```

表示只使用 50% 的候选位移：

```text
new_center = previous_center + damping * candidate_shift
```

输出字段表现为：

```text
confidence=low
action=damped
lr=0.000
template_updated=false
scale_updated=false
ema_updated=false
```

该策略的设计意图是：如果响应质量较差但位移不大，可以允许位置缓慢移动，避免目标框完全冻结；同时不更新模板，避免可能错误的外观污染模型。

### 4.7 low 且大偏移的更新策略

当置信度为 low，并且候选位置跳动较大时：

```text
displacement_ratio > low_displacement_threshold
```

当前策略是拒绝位置跳变：

```text
保持上一帧位置
恢复上一帧 scale
不更新模板
不更新 EMA
```

输出字段表现为：

```text
confidence=low
action=reject
lr=0.000
template_updated=false
scale_updated=false
ema_updated=false
```

该策略的设计意图是：如果响应质量低且位置突然大幅跳动，很可能是跟到了背景干扰，因此拒绝该跳变。


