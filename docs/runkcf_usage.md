# RunKCF 使用说明

本文档说明 `RunKCF` 当前的目录用途、临时测试文件含义，以及程序顶部配置项的使用方法。

## 1. `run/outputdata` 文件夹是什么

路径：

```text
D:\code\cpp\vs\myKCF\joaofaro\joao\run\outputdata
```

这个文件夹里目前存放的是按帧导出的文本数据，例如：

```text
frame_0.txt
frame_1.txt
...
frame_265.txt
```

文件内容类似：

```text
0 0.362500 0.145313 0.648438 0.145313 0.648438 0.371875 0.362500 0.371875
```

它看起来是某个标注或导出流程生成的逐帧归一化多边形数据：第一个数字通常像类别编号，后面是归一化坐标点。

需要注意：

- 当前 `src/runkcf.cpp` 的 `RunKCF` 程序并不会自动读取 `run/outputdata`。
- 当前 `RunKCF` 读取的是代码顶部 `ANNOTATION_PATH` 指向的标注 txt 文件。
- 所以 `run/outputdata` 可以理解为已有的生成数据或导出数据目录，但它不是当前 `RunKCF` 评估入口的默认输入目录。

## 2. `runkcf_test.log` 和 `runkcf_test.exit` 的含义

路径：

```text
D:\code\cpp\vs\myKCF\joaofaro\joao\run\runkcf_test.log
D:\code\cpp\vs\myKCF\joaofaro\joao\run\runkcf_test.exit
```

这两个文件不是 `RunKCF` 程序自动生成的正式输出文件，而是之前为了在命令行里测试程序时手动重定向生成的诊断文件。

它们通常来自类似下面的 PowerShell 命令：

```powershell
& "D:\code\cpp\vs\myKCF\joaofaro\joao\out\build\vs2022-debug\RunKCF.exe" *> "D:\code\cpp\vs\myKCF\joaofaro\joao\run\runkcf_test.log"
$LASTEXITCODE | Set-Content "D:\code\cpp\vs\myKCF\joaofaro\joao\run\runkcf_test.exit"
```

含义：

- `runkcf_test.log`：保存程序运行时的终端输出，包括标准输出和错误输出。
- `runkcf_test.exit`：保存程序退出码。

常见退出码含义：

```text
0
```

表示程序正常结束。

```text
-1073741819
```

这是 Windows 下常见的访问冲突退出码，对应十六进制 `0xC0000005`。之前调试时它和 `fhog.hpp` 中 `IplImage` 包装对象未完整初始化有关，已经在代码里修复。

如果你运行程序后发现 `.log` 是空的，通常有这几种原因：

- 你是在 Visual Studio 里直接运行程序，输出显示在 VS 的控制台窗口中，并不会自动写入这个 `.log` 文件。
- 这个 `.log` 是之前测试命令留下的旧文件，当前运行没有重新重定向到它。
- 程序曾经在很早的位置崩溃，输出还没来得及刷新到文件。
- 程序没有通过上面的 PowerShell 重定向命令运行。

结论：当前 `RunKCF` 正常运行时，默认只向终端打印结果，不会主动写 `runkcf_test.log`。

如果你想主动把终端输出保存到 `.log` 文件，需要使用上面的 PowerShell 重定向命令。

## 3. 当前配置在哪里

目前没有单独的外部配置文件。

之前实现的是“代码顶部配置开关”，也就是在下面这个文件开头直接修改常量：

```text
D:\code\cpp\vs\myKCF\joaofaro\joao\src\runkcf.cpp
```

主要配置项如下：

```cpp
const std::string VIDEO_PATH = "D:\\code\\cpp\\vs\\myKCF\\joaofaro\\joao\\source\\d4.mp4";
const std::string ANNOTATION_PATH = "C:\\Users\\yala5\\Desktop\\auav_data\\videos\\d4.txt";
const bool SAVE_VIDEO_OUTPUT = false;
const std::string OUTPUT_VIDEO_PATH = "D:\\code\\cpp\\vs\\myKCF\\joaofaro\\joao\\run\\runkcf_output.mp4";

const bool HOG = true;
const bool FIXEDWINDOW = false;
const bool MULTISCALE = true;
const bool LAB = false;
```

各配置项含义：

| 配置项 | 含义 |
| --- | --- |
| `VIDEO_PATH` | 要评估的视频路径 |
| `ANNOTATION_PATH` | 标注 txt 文件路径 |
| `SAVE_VIDEO_OUTPUT` | 是否保存绘制了跟踪框和真实框的视频 |
| `OUTPUT_VIDEO_PATH` | 输出视频保存路径 |
| `HOG` | 是否启用 HOG 特征 |
| `FIXEDWINDOW` | KCFTracker 的固定窗口配置 |
| `MULTISCALE` | 是否启用原始 KCFTracker 内置尺度配置 |
| `LAB` | 是否启用 Lab 颜色特征 |

## 4. 标注文件格式

当前 `RunKCF` 使用的新标注格式是：

```text
[fn, x1, y1, w, h]
```

或者不用方括号也可以：

```text
fn, x1, y1, w, h
```

含义：

- `fn`：视频帧序号。
- `x1`：目标框左上角 x 坐标。
- `y1`：目标框左上角 y 坐标。
- `w`：目标框宽度。
- `h`：目标框高度。

示例：

```text
[0, 232, 93, 183, 145]
[1, 233, 93, 183, 145]
[2, 233, 94, 183, 145]
```

程序会用标注文件中的第一条有效标注初始化跟踪器，后续帧按 `fn` 找对应真实框进行 CLE 和 IoU 评估。

## 5. 修改配置后的使用流程

每次修改 `src/runkcf.cpp` 顶部配置后，需要重新编译 `RunKCF`。

### Visual Studio 2022 中运行

1. 打开 Visual Studio 2022。
2. 选择“打开本地文件夹”，打开：

```text
D:\code\cpp\vs\myKCF\joaofaro\joao
```

3. 等待 CMake 配置完成。
4. 在顶部工具栏选择 CMake 配置，例如：

```text
vs2022-debug
```

5. 在启动目标中选择：

```text
RunKCF.exe
```

6. 右键 CMakeLists 或使用菜单执行生成，目标选择 `RunKCF`。
7. 按 `Ctrl + F5` 运行。

运行后，终端会逐帧输出类似：

```text
frame=1, kcf_ms=35.12, CLE=0.80, IoU=0.96
frame=2, kcf_ms=34.75, CLE=1.05, IoU=0.95
...
```

最后会输出整体统计：

```text
evaluated_frames=266
update_frames=265
avg_kcf_ms=35.678
avg_kcf_fps=28.028
avg_CLE=1.524
avg_IoU=0.953
CLE_lt_2_ratio=0.680
```

### 命令行运行并保存 log

如果需要把运行结果保存到 `runkcf_test.log`，可以在 PowerShell 里执行：

```powershell
& "D:\code\cpp\vs\myKCF\joaofaro\joao\out\build\vs2022-debug\RunKCF.exe" *> "D:\code\cpp\vs\myKCF\joaofaro\joao\run\runkcf_test.log"
$LASTEXITCODE | Set-Content "D:\code\cpp\vs\myKCF\joaofaro\joao\run\runkcf_test.exit"
```

这样：

- 终端输出会写入 `run\runkcf_test.log`。
- 程序退出码会写入 `run\runkcf_test.exit`。

## 6. 视频输出模式

默认配置：

```cpp
const bool SAVE_VIDEO_OUTPUT = false;
```

此时只在终端输出逐帧评估结果，不保存视频。

如果改成：

```cpp
const bool SAVE_VIDEO_OUTPUT = true;
```

程序会额外保存带框视频到：

```text
D:\code\cpp\vs\myKCF\joaofaro\joao\run\runkcf_output.mp4
```

绘制规则：

- 绿色框：KCF 跟踪预测框。
- 红色框：标注文件中的真实框。

注意：绘制和视频写入发生在 `tracker.update(frame)` 计时之后，不会被计入 KCF 核心处理耗时。

## 7. 后续建议

当前配置是写在 C++ 文件顶部的硬编码常量，适合短期快速实验。

后续如果要做大量对照实验，建议再升级为外部配置文件，例如：

```text
configs/baseline.json
configs/hog_color.json
configs/fdsst.json
configs/confidence_update.json
```

这样就不需要每次修改 C++ 源码并重新编译，只需要切换配置文件即可运行不同实验。
