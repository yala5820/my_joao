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
- CN4 仅有配置预留，特征提取尚未实现。

技术栈：

- C++
- OpenCV 4.10
- CMake + Ninja
- Visual Studio 2022 / MSVC
- YAML 配置使用 OpenCV `cv::FileStorage` 读取

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
src/app_config.*       YAML 配置读取与校验
```

当前 CMake 目标：

```text
KCF
RunKCF
HogFeatureTests
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

运行评估：

```powershell
.\out\build\vs2022-debug\RunKCF.exe D:\code\cpp\vs\myKCF\joaofaro\joao\configs\baseline_hog31.yaml
.\out\build\vs2022-debug\RunKCF.exe D:\code\cpp\vs\myKCF\joaofaro\joao\configs\hog18.yaml
```

可用配置：

```text
configs/baseline_hog31.yaml
configs/hog18.yaml
```

预留配置：

```text
configs/hog31_cn4.yaml
configs/hog18_cn4.yaml
```

构建注意：

- 不要在 Codex 中反复运行 `cmd /c call vcvars64.bat && cmake --build ...`，该命令在本项目中多次出现外层进程不返回。
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
- CN4 尚未实现，不能把 `hog31_cn4.yaml` 或 `hog18_cn4.yaml` 的结果描述为 CN 特征已生效。
- 构建或测试卡住时，必须主动诊断进程、锁文件和日志，不能长时间等待同一条命令。
