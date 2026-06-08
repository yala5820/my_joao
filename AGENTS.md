# AGENTS.md

本文件是给 Codex、Claude Code 和后续协作者使用的项目级说明。进入本仓库后，优先阅读并遵守本文件。

## 1. 项目总体信息

本项目基于 joaofaro 的 KCF C++ 实现，用于传统图像处理和相关滤波目标跟踪算法改进实验。

核心目标是建立可复现的 KCF 对照实验体系，逐步完成：

- HOG 特征降维与 CN 颜色特征融合。
- fDSST 尺度滤波器。
- 置信度机制与模板更新控制。

当前进度：

- `RunKCF` 视频评估入口已实现。
- YAML 外部配置入口已实现。
- HOG31 / HOG18 可配置版本已实现。
- CN4 颜色特征已实现，并已接入 HOG + CN 通道级拼接。
- `hog31_cn4.yaml` 与 `hog18_cn4.yaml` 已能成功运行并输出 Summary。
- 当前 CN4 使用固定、可复现的 CN11 近似查表和 PCA4 投影，不是原始 `w2c.mat` 数值表。
- `RunKCF` 已支持两类输入源：视频文件和图片序列文件夹。
- Phase 3 置信度机制已接入 `hog18_conf.yaml` 与 `hog18_cn4_conf.yaml`，使用 `peak / PSR / APCE / EMA` 控制位置、scale、模板更新和学习率。

技术栈：

- C++
- OpenCV 4.10
- CMake + Ninja
- Visual Studio 2022 / MSVC
- YAML 配置由 `src/app_config.cpp` 的项目内轻量解析器读取，支持当前配置使用的缩进 map 和标量值；配置文件必须直接从顶层 key 开始，例如 `experiment:`，不要添加 `%YAML:1.0` 或 `---` 文件头

## 2. 项目的目录与入口

主要目录：

```text
configs/    实验 YAML 配置
docs/       项目文档
run/        运行输出和临时结果
source/     视频和标注输入
src/        C++ 源码
tests/      轻量测试入口
out/        CMake/VS 构建输出
```

主要入口：

```text
src/runtracker.cpp     原始 KCF 示例入口
src/runkcf.cpp         当前实验评估主入口
tests/hog_feature_tests.cpp  HOG 通道裁剪测试
```

核心模块：

```text
src/kcftracker.*       KCFTracker 核心跟踪逻辑
src/fhog.*             FHOG 特征提取与 HOG18 通道裁剪
src/cn_feature.*       CN4 颜色特征提取
src/cn_data.*          CN11 查表与 PCA4 投影数据
src/confidence.*       peak、PSR、APCE、EMA 和置信度分级
src/app_config.*       YAML 配置读取与校验
src/annotation_loader.* 标注文件读取，兼容 5 值显式帧号和 4 值逐行帧号
src/frame_source.*     视频帧源与图片序列帧源
```

当前 CMake 目标：

```text
KCF
RunKCF
HogFeatureTests
CnFeatureTests
ConfidenceTests
DatasetIOTests
```

不要把所有 `src/*.cpp` 直接合进同一个目标，否则 `runtracker.cpp` 和 `runkcf.cpp` 的两个 `main()` 会冲突。

## 3. 项目的构建与运行

推荐使用 Visual Studio 2022 的 CMake 集成构建，配置为：

```text
vs2022-debug
```

命令行配置：

```powershell
cmake --preset vs2022-debug
```

运行 HOG 测试：

```powershell
.\out\build\vs2022-debug\HogFeatureTests.exe
```

运行 CN 测试：

```powershell
.\out\build\vs2022-debug\CnFeatureTests.exe
```

运行数据集输入测试：

```powershell
.\out\build\vs2022-debug\DatasetIOTests.exe
```

运行置信度测试：

```powershell
.\out\build\vs2022-debug\ConfidenceTests.exe
```

运行评估：

```powershell
.\out\build\vs2022-debug\RunKCF.exe D:\code\cpp\vs\myKCF\joaofaro\joao\configs\baseline_hog31.yaml
.\out\build\vs2022-debug\RunKCF.exe D:\code\cpp\vs\myKCF\joaofaro\joao\configs\hog18.yaml
.\out\build\vs2022-debug\RunKCF.exe D:\code\cpp\vs\myKCF\joaofaro\joao\configs\hog31_cn4.yaml
.\out\build\vs2022-debug\RunKCF.exe D:\code\cpp\vs\myKCF\joaofaro\joao\configs\hog18_cn4.yaml
.\out\build\vs2022-debug\RunKCF.exe D:\code\cpp\vs\myKCF\joaofaro\joao\configs\hog18_conf.yaml
.\out\build\vs2022-debug\RunKCF.exe D:\code\cpp\vs\myKCF\joaofaro\joao\configs\hog18_cn4_conf.yaml
```

可用配置：

```text
configs/baseline_hog31.yaml
configs/hog18.yaml
configs/hog31_cn4.yaml
configs/hog18_cn4.yaml
configs/hog18_conf.yaml
configs/hog18_cn4_conf.yaml
configs/dut_baseline_hog31.yaml
configs/dut_hog18.yaml
configs/dut_hog31_cn4.yaml
configs/dut_hog18_cn4.yaml
configs/dut_hog18_conf.yaml
configs/dut_hog18_cn4_conf.yaml
```

输入源说明：

- `input.type: video` 使用 `input.video_path` 读取视频，帧号从 0 开始，对应旧格式 `[fn, x, y, w, h]`。
- `input.type: image_sequence` 使用 `input.image_dir`、`input.image_extension`、`input.image_start_index`、`input.image_index_digits` 读取如 `00001.jpg` 的图片序列。
- 4 值标注格式 `x y w h` 按行号映射帧号，第一行对应第 1 帧；四个值均为 `-100` 时视为该帧无目标，不计入 CLE/IoU。
- `RunKCF` 输出 `load_ms` 和 `avg_frame_load_ms`，但 `kcf_ms` 仍只统计 `tracker.update(frame)`。
- `RunKCF` 每次运行都会把终端指标同步保存到 `run/output_data/<运行时间>.txt`；当 `output.save_video: 1` 时，视频统一保存到 `run/output_video/<运行时间>.mp4`。
- 输出文件名基于运行开始时间，格式如 `20260607_2124`；同一分钟多次运行时追加 `_01`、`_02` 等后缀，避免覆盖已有结果。
- 开启 `confidence.enabled: 1` 后，逐帧输出会追加 `peak`、`PSR`、`APCE`、`psr_ema`、`apce_ema`、`confidence`、`displacement_ratio`、`lr`、`action`、`template_updated`、`scale_updated`、`ema_updated`。
- Phase 3 只继续改进 `hog18` 与 `hog18_cn4` 两条实验线，不继续新增 `hog31` 置信度配置。

构建注意：

- 不要在 Codex 中反复运行 `cmd /c call vcvars64.bat && cmake --build ...`，该命令在本项目中多次出现外层进程不返回。
- 不要在 Codex 中直接运行 `cmake --build ...` 或 `ninja ...`；本项目在 Codex Windows shell 中多次出现外层进程不返回和 stale `.ninja_lock`。
- 如果构建长时间无输出，先检查 `cmake/ninja/cl/link/cmd` 进程和 `out/build/vs2022-debug/.ninja_log`，不要盲等。
- 如果确认没有构建进程但存在 stale `.ninja_lock`，再删除该 lock 文件。

## 4. 工作开发约束

- 回答用户优先使用中文。
- 修改前先阅读相关源码、配置和文档。
- 修改前说明要改哪些文件，修改后说明实际改了哪些文件。
- 不要回退、覆盖或清理用户已有改动。
- 不要使用破坏性命令，例如 `git reset --hard` 或强制删除目录，除非用户明确要求。
- 不要随意删除 `source/`、`run/`、`docs/` 中已有文件。
- 保持原始 `KCF` 入口兼容；后续实验主入口使用 `RunKCF`。
- 新增实验变量必须通过配置控制，避免硬编码分散在多个函数里。
- CN4 已实现，但当前为固定近似 CN11 查表 + PCA4 投影；若论文需要严格 Color Names 口径，应替换为标准 `w2c` 表。
- 当前不支持 CN-only，`features.cn.enabled: 1` 时必须同时启用 HOG。
- 置信度实验变量必须集中写在 YAML 的 `confidence` 段；旧配置缺少该段时应保持 `confidence.enabled: 0` 的原始行为。
- 构建或测试卡住时，必须主动诊断进程、锁文件和日志，不能长时间等待同一条命令。
