# Phase 3：置信度机制与模板更新控制实施计划

## Summary

本阶段只继续改进 `hog18` 与 `hog18_cn4` 两个版本，引入基于 `peak / PSR / APCE` 的自适应置信度机制，用置信度控制位置更新、scale 更新、模板更新和学习率。

## Key Changes

- 新增通用实验配置：
  - `configs/hog18_conf.yaml`
  - `configs/hog18_cn4_conf.yaml`
- 新增 DUT 图片序列实验配置：
  - `configs/dut_hog18_conf.yaml`
  - `configs/dut_hog18_cn4_conf.yaml`
- 新增 `ConfidenceConfig`，所有关键参数均从 YAML 读取。
  - PSR/APCE 计算参数：`psr_exclusion_radius`、`eps`
  - EMA 参数：`warmup_frames`、`ema_alpha`、`medium_ema_alpha`
  - 分级阈值：`high_ratio`、`low_ratio`
  - 更新控制参数：`medium_lr_factor`、`low_displacement_threshold`、`low_displacement_damping`
- `KCFTracker` 每帧记录诊断信息：`peak`、`psr`、`apce`、`psr_ema`、`apce_ema`、`confidence_level`、`displacement_ratio`、`effective_learning_rate`、`position_action`、`template_updated`、`scale_updated`、`ema_updated`。
- `RunKCF` 将上述字段追加到现有逐帧终端/txt输出。

## Algorithm Design

- `detect()` 阶段保留 response map，并计算：
  - `peak = max(response)`
  - `PSR = (peak - mean(sidelobe)) / (std(sidelobe) + eps)`
  - `APCE = (peak - min)^2 / (mean((response - min)^2) + eps)`
- PSR sidelobe 排除峰值附近 `psr_exclusion_radius` 个 response cell，默认 `5`。
- EMA 初始化：
  - 前 `warmup_frames` 帧只统计 PSR/APCE 均值。
  - 默认 `warmup_frames = 5`。
  - warmup 完成后初始化 `psr_ema` 和 `apce_ema`。
- 置信度分级：
  - `high`: `PSR >= high_ratio * psr_ema` 且 `APCE >= high_ratio * apce_ema`
  - `low`: `PSR < low_ratio * psr_ema` 或 `APCE < low_ratio * apce_ema`
  - `medium`: 其他情况
- EMA 更新：
  - `high` 使用 `ema_alpha` 更新 EMA。
  - `medium` 默认不更新 EMA，即 `medium_ema_alpha = 0.0`，但该值可通过 YAML 调整。
  - `low` 不更新 EMA。

## Update Control

- `high`：
  - 接受检测位置。
  - 接受检测 scale。
  - 模板学习率 = `interp_factor`。
  - 更新模板。
  - 更新 EMA。
- `medium`：
  - 接受检测位置。
  - 接受检测 scale。
  - 模板学习率 = `medium_lr_factor * interp_factor`，默认 `0.3`。
  - 更新模板。
  - 默认不更新 EMA。
- `low + displacement_ratio <= low_displacement_threshold`：
  - 位置阻尼更新，只应用部分检测位移。
  - `damping = low_displacement_damping`，默认 `0.5`。
  - 不更新模板。
  - 不更新 EMA。
  - 不更新 scale。
- `low + displacement_ratio > low_displacement_threshold`：
  - 保持上一帧位置。
  - 恢复上一帧 scale。
  - 拒绝位置跳变。
  - 不更新模板。
  - 不更新 EMA。
- `displacement_ratio = candidate_center_shift / previous_roi_diagonal`。
- 多尺度实现要求：
  - 检测前保存上一帧 `_roi` 和 `_scale`。
  - 多尺度检测只产生候选 ROI/scale。
  - 最终根据置信度 action 决定是否提交候选 ROI/scale。

## Config Interface

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

旧配置缺少 `confidence` 段时默认 `enabled = false`，保证 `hog18.yaml` 和 `hog18_cn4.yaml` 原始行为保持不变。

## Implementation Order

1. 创建 `docs/phase3_confidence_update_plan.md`，保存本计划。
2. 扩展 `AppConfig` 和轻量 YAML 解析器，支持 `confidence` 配置段及默认值。
3. 新增置信度计算结构和工具函数，覆盖 `peak / PSR / APCE / EMA / level`。
4. 改造 `KCFTracker::detect()`，保留 response map 并输出检测诊断。
5. 改造 `KCFTracker::update()`，按 high/medium/low 控制位置、scale、模板和学习率。
6. 改造 `RunKCF` 逐帧输出，追加置信度诊断字段。
7. 新增 `hog18_conf.yaml`、`hog18_cn4_conf.yaml`、`dut_hog18_conf.yaml` 和 `dut_hog18_cn4_conf.yaml`。
8. 更新 `AGENTS.md` 和必要文档，记录 Phase 3 配置与输出规则。

## Test Plan

- 配置测试：
  - 旧 YAML 无 `confidence` 段时加载成功，且 `enabled=false`。
  - `hog18_conf.yaml` / `hog18_cn4_conf.yaml` / `dut_hog18_conf.yaml` / `dut_hog18_cn4_conf.yaml` 能读取全部 confidence 参数。
- 置信度计算测试：
  - 单峰 response 的 PSR/APCE 高于平坦 response。
  - exclusion radius 生效。
  - warmup 后正确初始化 EMA。
  - high 触发 EMA 更新，medium 默认不更新，low 不更新。
- 更新控制测试：
  - high：接受位置/scale，使用原学习率，更新模板。
  - medium：接受位置/scale，使用 `0.3 * interp_factor`，更新模板。
  - low 小偏移：阻尼位置，不更新模板，不更新 scale。
  - low 大偏移：保持上一帧位置和 scale，不更新模板。
- 手动评估：
  - 对照运行 `hog18.yaml` 与 `hog18_conf.yaml`。
  - 对照运行 `hog18_cn4.yaml` 与 `hog18_cn4_conf.yaml`。
  - 对照运行 `dut_hog18.yaml` 与 `dut_hog18_conf.yaml`。
  - 对照运行 `dut_hog18_cn4.yaml` 与 `dut_hog18_cn4_conf.yaml`。
  - 检查 txt 输出中的 `PSR/APCE/confidence/lr/action/template_updated/scale_updated/ema_updated`。
  - 对比 `avg_CLE / avg_IoU / CLE_lt_2_ratio`。

## Assumptions

- 本阶段不实现重检测、不引入 Kalman 或其他运动模型。
- 本阶段不继续改进 `hog31` 与 `hog31_cn4`。
- 默认 PSR exclusion radius 取 `5`，如实验不稳定可直接在 YAML 调整为 `4`。
- medium 默认不更新 EMA，通过 `medium_ema_alpha: 0.0` 表达；后续实验可调为小 alpha。
