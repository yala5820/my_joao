# 项目状态

项目路径：

```text
D:\code\cpp\vs\myKCF\joaofaro\joao
```

本项目基于 joaofaro 的 KCF C++ 实现，用于传统图像处理目标跟踪算法改进实验。后续目标是完成论文式多组对照实验。

主要入口：

```text
src/runtracker.cpp           原始 KCF 示例入口
src/runkcf.cpp               当前视频评估主入口
tests/hog_feature_tests.cpp  HOG 通道裁剪测试
```

核心模块：

```text
src/kcftracker.*             KCFTracker 核心逻辑
src/fhog.*                   FHOG 特征提取与 HOG18 裁剪
src/app_config.*             YAML 配置读取与校验
```

当前 CMake 目标：

```text
KCF
RunKCF
HogFeatureTests
```

# 已确认事实

- `RunKCF` 已支持读取视频、读取逐帧标注、初始化 KCF、逐帧输出耗时/CLE/IoU，并输出 Summary。
- 当前标注格式为：

```text
[fn, x1, y1, w, h]
```

- YAML 外部配置已实现，使用 OpenCV `cv::FileStorage`。
- 当前配置文件：

```text
configs/baseline_hog31.yaml
configs/hog18.yaml
configs/hog31_cn4.yaml
configs/hog18_cn4.yaml
```

- 当前真实生效配置：
  - `baseline_hog31.yaml`：HOG31
  - `hog18.yaml`：HOG18

- 当前预留配置：
  - `hog31_cn4.yaml`
  - `hog18_cn4.yaml`

- CN4 尚未实现，不能声称 CN 特征已经生效。
- FHOG 当前流程：

```text
27维原始梯度 -> 108维归一化特征 -> 31维 PCAFeatureMaps 输出
```

- HOG18 定义：

```text
FHOG31 输出中的前 18 个方向梯度通道
```

# 已完成任务

- 新增 `RunKCF` 视频评估入口。
- 修改 CMake，避免把多个 `main()` 编进同一目标。
- 新增 YAML 配置读取模块：

```text
src/app_config.hpp
src/app_config.cpp
```

- `RunKCF` 支持默认读取 `configs/baseline_hog31.yaml`，也支持命令行传入配置路径。
- Summary 中已输出配置摘要。
- 新增 HOG18 通道裁剪：

```cpp
selectFeatureMapChannels(...)
```

- `KCFTracker` 已支持配置式构造。
- `features.hog.channels: 18` 已能真正影响 FHOG 特征维度。
- 新增 HOG 通道裁剪测试。
- 已给 FHOG 特征提取流程增加防御性返回值检查。
- `RunKCF` 已捕获 tracker 初始化和 update 过程中的异常。

# 未完成任务

- CN4 颜色特征尚未实现。
- HOG + CN 特征融合尚未实现。
- CN 单独特征提取模块尚未实现。
- fDSST 尺度滤波器尚未实现。
- 置信度机制和模板更新控制尚未实现。
- 最近一次 FHOG 防御性检查与测试增强改动后，尚未在 VS2022 中完成重新编译验证。

# 关键约束

- 回答用户优先使用中文。
- 修改代码前先阅读相关源码和配置。
- 修改前说明要改哪些文件，修改后说明实际改了哪些文件。
- 不要回退、覆盖或清理用户已有改动。
- 不要使用破坏性命令，除非用户明确要求。
- 不要随意删除 `source/`、`run/`、`docs/` 中已有文件。
- 保持原始 `KCF` 入口兼容。
- 后续实验主入口使用 `RunKCF`。
- 新实验变量必须通过 YAML 配置控制，避免分散硬编码。
- KCF 核心耗时只统计 `tracker.update(frame)`。
- CN4 未实现前，不能把 CN 配置结果解释为 CN 特征结果。

# 需要避免的错误

- 不要在 Codex 中反复运行：

```powershell
cmd /c 'call "...vcvars64.bat" && cmake --build ...'
```

- 如果构建长时间无输出，不要一直等。应检查构建进程和 `.ninja_log`。
- 如果存在 stale `.ninja_lock`，必须先确认没有 `cmake/ninja/cl/link` 构建进程，再删除 lock。
- 不要把 `cmake --build` 卡住误认为代码编译失败；之前发生过 exe 已生成但外层进程不返回的情况。
- 不要在未重新编译的情况下运行旧 exe 并声称验证了新代码。
- 不要把 `hog31_cn4.yaml` / `hog18_cn4.yaml` 当作 CN 已实现配置。
- 不要把大量流水账写进 `AGENTS.md`；它应保持精简。
