# TinyPose 对照训练准备

已完成数据冻结、前向预检、原模型基线测量，并于2026-10-25完成三组正式GPU训练和INT8评估。
三组均未通过全部预设门槛，结果见[正式训练报告](../../hardware-logs/tinypose-comparison-results-20261025.md)。
后续“仅SC＋光照”、COCO 12.5%和25%也已完成GPU训练，见[第二轮报告](../../hardware-logs/tinypose-followup-results-20261025.md)。
保留现有8点/96×96网络、v4c检查点和INT8部署契约，不更换设备上的模型。

## 已冻结实验

目录：`.secrets/experiments/tinypose-coco-control-v1-20261024/`

`experiment.json` 固定源图、标签、人工审批、初始模型、训练/评估源码和依赖锁定文件的哈希。
训练入口在开始及结束时校验哈希。代码或数据发生改变后应重新准备一个新目录，不能
修改已冻结清单以掩盖变更。私有数据清单使用相对图像路径，不复制或修改源图。

| 角色 | 数量 | 使用方式 |
|---|---:|---|
| sc2336_train | 210 | 原v4c最终refit用过的训练池，含空背景/下肢截断负例 |
| coco_train | 108 | 用户已批准的公共训练候选 |
| coco_validation | 27 | 用户已批准的公共验证候选，永不并入训练/校准 |
| sc2336_regression | 24 | 原人工保留图像，仅比较历史表现，不作为新盲测声明 |
| sc2336_empty_development | 15 | 历史开发负例，检查退化；与训练有重叠 |
| sc2336_cropped_development | 16 | 历史开发负例，检查退化；与训练有重叠 |
| calibration | 100 | 固定50张SC2336训练图＋50张COCO训练图，三组共用 |

原SC2336记录共215条，其中5条按原加载器规则过滤，剩余210条与原报告一致。
四个主要角色间无完全相同的解码图像；COCO待定第28张没有进入任何主要角色或校准集。
这不是人物身份隔离证明：COCO没有人物ID，旧SC2336资料也不足以证明跨人物泛化。

## 对照组

保留原v4c TFLite作为固定参考，三个实验都从同一个v4c浮点检查点开始：

| ID | 每个batch | 增强 | 用途 |
|---|---|---|---|
| sc_only | 32 SC2336 | 随机水平翻转 | 控制额外微调和重新量化本身的影响 |
| sc_coco | 16 SC2336＋16 COCO | 同样的随机翻转 | 检查增加公共数据的收益/遗忘 |
| sc_coco_photo | 与sc_coco相同抽样和翻转 | 再加亮度/对比度/伽马/噪声 | 检查光照增强的增量效果 |

固定seed=34596、batch=32、10个epoch、每epoch14步、Adam学习率0.0002。
每组140次参数更新，按域有放回抽样，避免数据量不同导致训练预算不同。
光照随机数独立于样本/翻转随机数，使后两组取样与标签完全相同。
光照增强保留黑色填充，不移动坐标。

保留原SC2336软置信度及COCO二元可见性目标，不在本轮额外改变置信度目标定义。
这两类目标语义不同，仍需同时观察坐标精度和可见性指标，不能只看训练loss。

## 验证集使用

原 `train_keypoints.py` 调参后会将其 validation 合并进行refit，这是历史流程，未修改。
本实验使用独立 `run_comparison.py`：固定最终第10个epoch，不做early stopping，不将
COCO验证或SC2336回归图像用于梯度更新、INT8校准或refit。
每epoch的COCO loss只记录，不据此挑epoch。训练结束后才评估INT8结果。

## 预检与正式执行

所有命令在团队仓库根目录执行。

创建新数据快照（输出目录需不存在）：

```bash
CUDA_VISIBLE_DEVICES=-1 .secrets/tinypose-gpu-venv/bin/python \
  tools/tinypose/prepare_comparison.py \
  --output .secrets/experiments/tinypose-coco-control-v2
```

默认仅前向预检，不更新权重、不导出新模型：

```bash
CUDA_VISIBLE_DEVICES=-1 .secrets/tinypose-gpu-venv/bin/python \
  tools/tinypose/run_comparison.py \
  --manifest .secrets/experiments/tinypose-coco-control-v1-20261024/experiment.json \
  --output .secrets/experiments/tinypose-coco-control-v1-20261024/preflight-repeat
```

正式训练需要明确加 `--train`，不要设置 `CUDA_VISIBLE_DEVICES=-1`：

```bash
.secrets/tinypose-gpu-venv/bin/python tools/tinypose/run_comparison.py \
  --manifest .secrets/experiments/tinypose-coco-control-v1-20261024/experiment.json \
  --output .secrets/experiments/tinypose-coco-control-v1-20261024/trained \
  --train
```

正式训练要求可见GPU并检查梯度设备，避免静默CPU回退。Codex沙箱可能需要单独授权GPU访问。
可用 `--arm sc_only` 等参数单独运行；输出目录必须不存在。
正式训练已执行；实际输出为 `trained-20261025/`，每组完成140次GPU参数更新和INT8导出。
脚本只写新实验输出，不自动刷机或替换v4c。

## 原模型基线（已测量）

| 数据 | 图像数 | PCK@10px | 平均误差px |
|---|---:|---:|---:|
| 原SC2336回归集 | 24 | 95.83% | 3.96 |
| COCO验证，原图 | 27 | 12.56% | 24.47 |
| 同COCO验证，亮度×0.6 | 27 | 11.56% | 22.88 |
| 同COCO验证，亮度×1.3 | 27 | 13.57% | 24.15 |

原SC2336五项坐标指标与归档逐项匹配。空背景和下肢截断历史开发集隐藏点拒绝率均100%。
COCO原图中的17个隐藏点仅有2个被正确拒绝（11.76%），说明还需要关注可见性泛化。
PCK以96×96图像坐标距离计算，不是深蹲计数准确率。
合成亮度压力测试只乘像素增益，不模拟真实传感器噪声、曝光或运动模糊，不能替代真机测试。

单独复测某个集合时，用新评估器（原人工验收脚本保持原样）：

```bash
CUDA_VISIBLE_DEVICES=-1 .secrets/tinypose-gpu-venv/bin/python \
  tools/tinypose/evaluate_comparison.py \
  --model .secrets/tinypose-keypoints-portrait-model-v4c-20260920/tinypose_keypoints_int8.tflite \
  --labels .secrets/experiments/tinypose-coco-control-v1-20261024/data/coco_validation.jsonl \
  --output .secrets/experiments/tinypose-coco-control-v1-20261024/baseline-repeat.json
```

## 比较标准与限制

工程门槛已在manifest中提出：SC2336 PCK@10下降不超过1个百分点，平均误差增加不超过
0.5px，P95增加不超过1px；历史负例隐藏点拒绝率下降不超过2个百分点。
同时观察COCO PCK@10是否至少提高5个百分点及合成亮度压力表现。
这些是候选筛选门槛，不是统计置信区间或产品准确率承诺。

三个训练组统一INT8校准集，与历史v4c校准不完全相同，因此必须保留sc_only对照。
当前只有单个随机种子和小验证集；首轮有收益后才考虑多seed和新增真人/SC2336视频。
Fit3D、个人视频和待定第28张均未加入。

验收记录：[对照训练准备报告](../../hardware-logs/tinypose-comparison-preparation-20261024.md)。
