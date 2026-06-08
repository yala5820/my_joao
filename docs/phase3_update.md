# 新版核心改进
1. 增加 soft_low / hard_low
2. 分离 position / scale / template 三种提交规则

## 新的置信度等级设计
旧的三等级设计不够用，新的设计为4个等级，分别是：high / medium / soft_low / hard_low

## position / scale / template 三套提交规则
不要再用一个 confidence 同时简单决定所有动作，为三作者设计各自的更新规则。

### position 提交规则
high:
    接受 candidate_roi

medium:
    接受 candidate_roi

soft_low:
    接受 candidate_roi
    或按 soft_low_position_damping 轻微阻尼
    推荐默认 1.0，也就是完整接受位置

hard_low:
    如果 displacement_ratio <= hard_low_displacement_threshold:
        阻尼更新位置
    否则:
        拒绝位置，恢复 previous_roi

### scale 提交规则
high:
    直接接受 candidate_scale

medium:
    平滑接受 candidate_scale

soft_low:
    不更新 scale，保持 previous_scale

hard_low:
    恢复 previous_scale

### template 提交规则
high:
    lr = interp_factor

medium:
    lr = medium_lr_factor * interp_factor

soft_low:
    lr = 0，不更新模板

hard_low:
    lr = 0，不更新模板

### EMA 更新规则
high:
    更新 EMA

medium:
    默认不更新 EMA

soft_low:
    不更新 EMA

hard_low:
    不更新 EMA