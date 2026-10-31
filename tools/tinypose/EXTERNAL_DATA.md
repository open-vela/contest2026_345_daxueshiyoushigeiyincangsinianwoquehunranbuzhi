# COCO 与健身视频导入

在团队仓库根目录执行，使用 `.secrets/tinypose-gpu-venv/bin/python`。
依赖已写入 `gpu-requirements.lock.txt`：Pillow 11.3.0、OpenCV headless 4.12.0.88。
工具不替换现有 v4c、不修改 SC2336 数据、不启动模型训练。

2026-10-24 已实际导入 200 train + 50 validation；尺寸筛选后为 108 + 28。
原训练加载器全部接受，14 项测试通过。详细结果见
[验收报告](../../hardware-logs/tinypose-external-data-20261024.md)。数据仍待完整视觉复核。

## COCO 单人候选子集

[COCO 官方下载说明](https://cocodataset.org/#download)提供 2017 标注。
只下载标注包和筛选后的图像，不需要 18GB 完整 train 图像包。

```bash
mkdir -p .secrets/dataset-cache
curl --fail --location --retry 6 --continue-at - \
  --output .secrets/dataset-cache/annotations_trainval2017.zip \
  https://s3.amazonaws.com/images.cocodataset.org/annotations/annotations_trainval2017.zip

.secrets/tinypose-gpu-venv/bin/python tools/tinypose/import_coco.py \
  --annotations-zip .secrets/dataset-cache/annotations_trainval2017.zip \
  --cache .secrets/dataset-cache/coco-images \
  --output .secrets/datasets/coco-singleperson-v1 \
  --train-count 200 --validation-count 50 --download
```

输出目录必须不存在；缓存允许重用。完整导入以 `import-report.json` 为完成标志。

初次导入保留宽泛的候选池，建议再按人物在模型输入中的大小筛选：

```bash
.secrets/tinypose-gpu-venv/bin/python tools/tinypose/curate_coco_subset.py \
  --dataset .secrets/datasets/coco-singleperson-v1 --min-body-span 0.30
```

原始 `labels.jsonl` 不变，生成同目录 `labels-size-filtered.jsonl`，图像路径保持兼容。
默认要求可见肩/髋/膝/踝点的 X 或 Y 跨度至少占画面 30%，即约 29 个 96×96 输入像素；
这能减少全画面中太小的人物，又保留横向姿态。被筛除记录进入 `size-review-queue.jsonl`。
可用 `--exclude-image-ids` 加上人工检查发现的无关或标注异常图像 ID。
它只完成尺寸筛选，仍需视觉复核；导入器不声称该子集都是健身动作或成人。

过滤规则：

- 按全部 person annotations 计数，恰好一人；有额外未标关键点的人也排除。
- 排除 crowd、小于 100 像素高的人和 8 个目标点中不足 6 个可见的样本。
- 默认只选图像 license ID 4（CC BY）；每帧保留 license URL、原图和 Flickr 链接。
- 按膝角分成 straight / bent / deep_bend / unknown_knee 后轮流抽样，固定随机种子。
- 使用官方 train2017 / val2017 划分，并检查原图内容哈希无交叉重复。

“单人”指单个已标注 person，仍需要检查 review.png 是否包含未标注的人、雕像或异常标注。
COCO 没有人物身份编号，官方图像划分不等于已证明的人物隔离。
膝弯曲分桶只是抽样方法，坐姿、骑车也可能膝弯曲，绝不自动标为深蹲。
CC BY 元数据过滤不是对全部商业权利的保证，原图署名及具体来源条款需要保留。

默认 `--framing full-frame`：保持整帧、等比例缩放、黑边填充至 192×192，接近当前
部署的全画面输入结构。可选 `--framing person-context`：使用 1.5 倍人物框上下文裁剪。
若使用后者预训练，后续仍需用实际 SC2336 画面微调，不能直接假设与部署分布一致。

## 坐标、可见性和文件格式

`train/labels.jsonl`、`validation/labels.jsonl` 可由当前 `train_keypoints.load_records`
直接读取。图像为 RGB PPM 192×192，训练时仍由原程序缩小到 96×96。
外部素材已经是直立图像，不再追加摄像头物理安装旋转。

每条记录包含：

- `keypoints`：肩、髋、膝、踝共 8 点，顺序与当前学生模型一致。
- `keypoints17` 和 `source_keypoints_coco`：保留完整 17 点，方便后续扩展，当前训练器仍为 8 点。
- `transform`：源图宽高、实际整数裁剪框、缩放尺寸、X/Y 精确比例、黑边位置。
- 坐标：左上角为原点，X 向右、Y 向下，以输出图像 extent 192 归一化至 [0,1]。
- `sha256`、`source_sha256`、来源/许可信息，以及亮度和图像梯度诊断值。

按 [COCO 官方定义](https://cocodataset.org/#format-data)：

| 原始 v | 含义 | 当前 8 点训练目标 |
|---|---|---|
| 2 | 已标注且可见 | confidence=1, usable=true；坐标参与损失 |
| 1 | 已标注但遮挡 | 保留坐标和 v=1，confidence=0；当前坐标损失屏蔽 |
| 0 | 未标注 | 保留 v=0，使用无效坐标占位，confidence=0 |

裁剪出画的点额外标记 `outside_crop`，不能通过坐标截断伪装为可见点。
这是针对当前应用可见性门限的训练策略，不是说 COCO 的 v=1 没有坐标监督价值。
COCO ground-truth 不能重命名成 `manual` 以绕过原人工 holdout 验收限制。

## 视频素材

**2026-10-24 状态：个人手机深蹲/波比跳采集均为待完成，按用户要求暂缓；
SC2336 补录待板子归还后进行。** 当前不要求用户立即录制，不阻塞 COCO 候选集复核。

使用自录手机视频、后续 SC2336 视频或许可明确的公开健身视频。
板子缺席不影响手机录像和此流程。优先完整深蹲序列，再补波比跳。
当前工具不会自动下载需要账号/申请的 Fit3D，也不将任意网络视频视为可用素材。

Fit3D 获取说明见 [FIT3D_ACQUISITION.md](FIT3D_ACQUISITION.md)。其免费许可禁止为商业用途
训练模型，取得与实际使用目的相符的许可前，不能将素材混入计划商业化的产品模型。

参考 `video-clips.example.json`，在素材目录创建私有 clips.json。
示例里的路径、许可、次数都是占位示例，必须改成真实值；不要原样声称取得许可。
`path` 相对于 clips.json；同一人物所有录像必须使用相同 subject_id 和 split。
`single_person_confirmed` 由素材筛选者确认，单人 MoveNet 无法保证画面没有第二个人。

```bash
.secrets/tinypose-gpu-venv/bin/python tools/tinypose/import_workout_videos.py \
  --clips .secrets/workout-sources/clips.json \
  --output .secrets/datasets/workout-video-v1 \
  --fps 3 --max-frames-per-clip 300 \
  --teacher .secrets/build-inputs/movenet_singlepose_lightning_int8_v4.tflite
```

不传 `--teacher` 则只抽帧，供人工标注。时间段、可选 phases 均使用原视频秒数。
默认每秒最多采样 3 帧，每段上限 300 帧；报告明确记录是否达到上限。
解码器时间戳优先，失效时记录使用 frame_index/FPS 回退，不能声称是精确 VFR 时钟。

输出保留人物/视频/帧号/时间/动作类型/可选人工阶段；同人物或同视频跨 split 会报错。
完全相同的解码帧保留在时序 manifest 中，只标记为不重复用于训练，跨 split 重复会拒绝。
黑暗、模糊只记录诊断，不自动删除，
以免丢掉需要改善的困难样本。

教师返回的概率与 COCO 二元可见性目标通过 `confidence_semantics` 区分。
至少 6 个目标点满足教师门限的 train/validation 帧进入 `labels.jsonl`，仍需人工抽查；
其余进入 `review-queue.jsonl`。holdout 全部进入 review queue，绝不自动批准为人工真值。
对教师帧使用 `--binary-visibility-targets` 时可按 `usable` 训练二元可见性。

先检查每组 `review.png`，重点修正底部、遮挡、暗光帧，再开始训练。
`expected_reps` 和 phases 是人工填入的信息，导入器不宣称已经识别或验证计数。

对已有标签生成带全局编号的分页预览：

```bash
.secrets/tinypose-gpu-venv/bin/python tools/tinypose/review_pages.py \
  --labels .secrets/datasets/coco-singleperson-v1/train/labels-size-filtered.jsonl \
  --output .secrets/datasets/coco-singleperson-v1/train/review-pages
```

默认每页48张，输出目录必须不存在；原图和标签不变。审批按用户明确确认的页面范围
另存记录，生成预览不会自动批准样本。当前用户已批准全部108张训练候选，无需修改标签；
验证第1–27张已批准，第28张由用户标记不确定，单独保留待复核；编号与训练集分别计算。
验证目录 `labels-reviewed.jsonl` 只包含已批准的27张，`labels-pending-review.jsonl` 保存第28张；
原始 `labels-size-filtered.jsonl` 的28张及所有标签字段保持不变。

## 验证与下一步

```bash
.secrets/tinypose-gpu-venv/bin/python -m unittest discover \
  -s tools/tinypose -p test_dataset_import.py -v
```

测试覆盖实际图像标记与关键点映射、整数黑边/裁剪边界、遮挡、多人/许可过滤、
固定种子、人物/源视频泄漏，以及真实 AVI 解码抽帧。合成视频仅测试工具，不当作健身训练集。

新增公共数据先作为泛化预训练候选；下一阶段再确定与 SC2336 数据的混合比例并训练对照模型。
原 24 张 SC2336 holdout 保持独立，新数据导入不等于 v4c 精度已提高。

已完成下一阶段[对照训练准备](COMPARISON_TRAINING.md)：冻结108张已批准公共训练图、
27张已批准公共验证图与原210张SC2336训练池，待定第28张未加入。
2026-10-25已完成三组正式GPU对照训练及INT8评估；公共数据带来有限泛化收益，
但混合训练导致SC2336与负例表现退化，三组均未通过全部门槛，保留原v4c。
详见[正式训练报告](../../hardware-logs/tinypose-comparison-results-20261025.md)。
