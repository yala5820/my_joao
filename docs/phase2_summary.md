# 第二阶段特征优化总结

## 阶段目标

第二阶段围绕 KCF 特征提取做初步优化，目标是建立可配置、可复现的特征对照实验流程。主要工作包括：

- 建立外部 YAML 配置入口。
- 实现 HOG31 / HOG18 可配置对照。
- 引入 CN4 颜色特征，并与 HOG 做通道级拼接。
- 保持原始 `KCF` 示例入口兼容，实验评估使用 `RunKCF`。

## 已完成工作

### Phase 2.1：特征配置入口

已新增 `src/app_config.hpp` 和 `src/app_config.cpp`，使用 OpenCV `cv::FileStorage` 读取 YAML 配置。

当前配置能够控制：

- 视频路径和标注路径。
- 是否保存输出视频。
- `fixed_window` 和 `multiscale` 跟踪参数。
- HOG 是否启用及通道数。
- CN 是否启用及通道数。
- Lab 是否启用。
- 特征融合方式。

`RunKCF` 已支持默认读取 `configs/baseline_hog31.yaml`，也支持命令行传入配置路径。

### Phase 2.2：HOG 特征降维

已实现 HOG31 / HOG18 可配置版本。

当前 HOG18 定义为：

```text
FHOG31 输出中的前 18 个方向梯度通道
```

实现位置：

```text
src/fhog.hpp
src/fhog.cpp
src/kcftracker.cpp
```

测试入口：

```text
tests/hog_feature_tests.cpp
```

对应配置：

```text
configs/baseline_hog31.yaml
configs/hog18.yaml
```

### Phase 2.3：CN4 颜色特征

已新增 CN 颜色特征模块：

```text
src/cn_data.hpp
src/cn_data.cpp
src/cn_feature.hpp
src/cn_feature.cpp
```

当前流程为：

```text
BGR patch -> RGB 量化 -> CN11 -> cell 平均 -> PCA4 -> HOG + CN concat
```

`KCFTracker::getFeatures()` 已在 HOG 特征生成后追加 CN4 特征：

- `hog31_cn4.yaml`：有效特征通道数为 35。
- `hog18_cn4.yaml`：有效特征通道数为 22。

测试入口：

```text
tests/cn_feature_tests.cpp
```

对应配置：

```text
configs/hog31_cn4.yaml
configs/hog18_cn4.yaml
```

用户已确认 `hog31_cn4.yaml` 和 `hog18_cn4.yaml` 可以成功运行并输出 Summary。

### Phase 2.4：组合实验入口

当前四组对照实验配置已经具备：

| 实验 | 配置文件 | 特征 |
| --- | --- | --- |
| Baseline HOG31 | `configs/baseline_hog31.yaml` | HOG31 |
| HOG18 | `configs/hog18.yaml` | HOG18 |
| HOG31 + CN4 | `configs/hog31_cn4.yaml` | HOG31 + CN4 |
| HOG18 + CN4 | `configs/hog18_cn4.yaml` | HOG18 + CN4 |

四组实验的指标记录由用户后续手动完成。

建议统一记录：

- `avg_kcf_ms`
- `avg_kcf_fps`
- `avg_CLE`
- `avg_IoU`
- `CLE_lt_2_ratio`

## 当前限制

- Phase 2.3 只支持 `HOG + CN` 通道级拼接，不支持 CN-only。
- `features.fusion.mode` 当前只支持 `concat`。
- 当前 CN4 使用固定、可复现的 CN11 近似查表和固定 PCA4 投影，不是直接嵌入原始 `w2c.mat` 数值表。
- CN 与 HOG 的数值尺度是否需要额外权重或归一化，需要通过后续四组实验结果判断。
- 响应图级加权融合尚未实现。

## 后续建议

1. 完成四组 YAML 配置的统一实验记录。
2. 根据实验结果判断 HOG18 是否带来稳定速度收益。
3. 根据实验结果判断 CN4 是否改善 CLE / IoU。
4. 如需论文式严格表述 Color Names，可将当前近似 CN lookup 替换为标准 `w2c` 表。
5. 在第二阶段结果稳定后，再进入 fDSST 尺度滤波器阶段。
