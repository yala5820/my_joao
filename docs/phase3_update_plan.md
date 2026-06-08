# Phase 3.1：soft_low / hard_low 与分离提交规则实施计划

## Summary

本次改进基于 `docs/phase3_update.md`，将当前 `high / medium / low` 三等级置信度机制升级为 `high / medium / soft_low / hard_low` 四等级机制，并将 position、scale、template 三类提交规则拆开处理，避免一个 confidence 分支同时冻结位置、尺度和模板。

采用方案 A：

```text
low_ratio      -> soft_low 阈值
hard_low_ratio -> hard_low 阈值
```

默认：

```yaml
low_ratio: 0.60
hard_low_ratio: 0.35
```

## Target Behavior

### 置信度分级

```text
high:
  PSR >= high_ratio * psr_ema
  且 APCE >= high_ratio * apce_ema

hard_low:
  PSR < hard_low_ratio * psr_ema
  或 APCE < hard_low_ratio * apce_ema

soft_low:
  PSR < low_ratio * psr_ema
  或 APCE < low_ratio * apce_ema

medium:
  其他情况
```

判定顺序必须是 `high -> hard_low -> soft_low -> medium`。

### position 提交

```text
high:
  接受 candidate_roi

medium:
  接受 candidate_roi

soft_low:
  使用 soft_low_position_damping 提交位置
  默认 1.0，即完整接受 candidate_roi

hard_low:
  如果 displacement_ratio <= hard_low_displacement_threshold:
    使用 hard_low_position_damping 阻尼更新位置
  否则:
    拒绝位置跳变，恢复 previous_roi
```

### scale 提交

```text
high:
  直接接受 candidate_scale

medium:
  使用 medium_scale_smoothing 平滑接受 candidate_scale

soft_low:
  保持 previous_scale

hard_low:
  恢复 previous_scale
```

### template 提交

```text
high:
  lr = interp_factor
  更新模板

medium:
  lr = medium_lr_factor * interp_factor
  更新模板

soft_low:
  lr = 0
  不更新模板

hard_low:
  lr = 0
  不更新模板
```

### EMA 更新

```text
high:
  使用 ema_alpha 更新 EMA

medium:
  使用 medium_ema_alpha 更新 EMA
  默认 medium_ema_alpha = 0，不更新

soft_low:
  使用 low_ema_alpha 更新 EMA
  默认 low_ema_alpha = 0，不更新

hard_low:
  不更新 EMA
```

## Config Interface

新增字段：

```yaml
confidence:
  low_ema_alpha: 0.0
  hard_low_ratio: 0.35
  soft_low_position_damping: 1.00
  medium_scale_smoothing: 0.50
  hard_low_displacement_threshold: 0.50
  hard_low_position_damping: 0.50
```

兼容旧字段：

```yaml
confidence:
  low_displacement_threshold: 0.50
  low_displacement_damping: 0.50
```

如果新字段缺失但旧字段存在，旧字段分别作为 hard_low 的阈值和阻尼默认值。

## Implementation Tasks

1. 更新测试：增加 `soft_low` / `hard_low` 分级测试和新 YAML 字段解析测试。
2. 扩展 `ConfidenceConfig`：加入 `low_ema_alpha`、`hard_low_ratio`、`soft_low_position_damping`、`medium_scale_smoothing`、`hard_low_displacement_threshold`、`hard_low_position_damping`。
3. 更新 YAML 读取和校验：确保 `0 < hard_low_ratio <= low_ratio <= high_ratio`，阻尼和平滑参数在 `[0, 1]`。
4. 更新 `ConfidenceLevel`：将 `Low` 拆为 `SoftLow` 和 `HardLow`。
5. 更新 `ConfidenceEstimator::evaluate()`：按方案 A 计算四等级。
6. 更新 `KCFTracker::update()`：拆分 position、scale、template 三类提交规则。
7. 更新 `RunKCF` 输出：确认 `confidence=soft_low/hard_low` 可正确打印。
8. 更新 4 个置信度 YAML：`hog18_conf.yaml`、`hog18_cn4_conf.yaml`、`dut_hog18_conf.yaml`、`dut_hog18_cn4_conf.yaml`。
9. 更新文档：`phase3_confidence_update_summary.md` 与 `AGENTS.md`。

## Manual Test Plan

在 VS2022 中重新构建后运行：

```powershell
.\out\build\vs2022-debug\ConfidenceTests.exe
.\out\build\vs2022-debug\DatasetIOTests.exe
```

评估：

```powershell
.\out\build\vs2022-debug\RunKCF.exe D:\code\cpp\vs\myKCF\joaofaro\joao\configs\dut_hog18_conf.yaml
.\out\build\vs2022-debug\RunKCF.exe D:\code\cpp\vs\myKCF\joaofaro\joao\configs\dut_hog18_cn4_conf.yaml
```

重点检查：

- `confidence=soft_low` 是否替代原来大量 `low`。
- `confidence=hard_low` 是否只在响应严重异常时出现。
- `soft_low` 默认是否 `action=accept` 且 `template_updated=false`。
- `medium` 是否接受位置并平滑接受 scale。
- `hard_low` 是否在小位移时 `action=damped`，大位移时 `action=reject`。
