# TinyPose GPU 环境与 v4c 离线复现

所有命令均在团队仓库根目录执行。环境位于 `.secrets/tinypose-gpu-venv`，
原 `.secrets/mww-train-venv` CPU 环境保持原样。

## 使用 GPU 环境

```bash
source .secrets/tinypose-gpu-venv/bin/activate
python -c "import tensorflow as tf; print(tf.config.list_physical_devices('GPU'))"
```

环境使用本地 CPython 3.12.11、TensorFlow 2.18.1、Keras 3.15.1。
TensorFlow 版本与原 CPU 环境对齐；CUDA/cuDNN 通过 pip 安装到独立虚拟环境。
Windows 驱动由 WSL 共享，不需要在 WSL 安装 Linux NVIDIA 驱动。

2026-10-24 验收：RTX 3060 Ti (compute capability 8.6) 实际 GPU 矩阵运算、
TinyPose 前向、梯度和单步更新均通过；五组 v4c 离线报告与归档逐字段完全相等。
详细结果见 [验收记录](../../hardware-logs/tinypose-gpu-v4c-baseline-20261024.md)。

按照 [TensorFlow 官方 WSL2 安装方式](https://www.tensorflow.org/install/pip#windows-wsl2)
配置。全新环境的安装命令：

```bash
.secrets/python/cpython-3.12.11-linux-x86_64-gnu/bin/python3.12 -m venv .secrets/tinypose-gpu-venv
.secrets/tinypose-gpu-venv/bin/python -m pip install 'pip==26.2.2'
.secrets/tinypose-gpu-venv/bin/python -m pip install -r tools/tinypose/gpu-requirements.lock.txt
```

只在目标目录不存在时创建新环境，不要覆盖仍在使用的虚拟环境。

## 验证 GPU 训练能力

下面输出路径需要尚不存在，可更换后缀后重新执行：

```bash
python tools/tinypose/check_gpu.py \
  --checkpoint .secrets/tinypose-keypoints-portrait-model-v4c-20260920/tinypose_float.keras \
  --output .secrets/gpu-smoke-new.json
```

验证内容：真实 GPU 矩阵乘法、v4c 网络前向、全部梯度位于 GPU、一次权重更新。
采用合成 batch，更新只发生在内存中，不保存模型，也不代表新模型训练精度。
脚本启用按需显存分配，并关闭静默 CPU 回退。

Codex 沙箱可能阻止 GPU 访问；此时通过已授权的沙箱外命令执行。
用户自己的普通 WSL 终端不受 Codex 沙箱限制。

## 复现 v4c

```bash
python tools/tinypose/reproduce_v4c.py \
  --output .secrets/v4c-baseline-new
```

脚本固定 v4c TFLite SHA256，调用原有评估程序，不改变其预处理和指标定义。
五组报告分别是：关键点误差、全身可见性、下肢裁切拒绝、空背景拒绝、近距离开发集。
每个字段与原归档报告比较，数值绝对容差为 1e-6，任何不匹配使进程失败。
输出同时包含原始评估日志、依赖快照、输入模型/标签/图像 SHA256。

关键点 holdout 为两个 session 共 24 张人工标注图像、192 个可见关键点。
近距离子集属于开发数据，不作为独立验收集。
PCK 指标使用 96×96 图像像素距离，不是深蹲计数准确率。

TFLite 离线评估使用 CPU。GPU 验证用于 Keras 训练；安装 GPU TensorFlow
不会自动将 `tf.lite.Interpreter` 推理转移到 GPU。

## 数据和模型位置

模型、图像和虚拟环境均位于被 Git 忽略的 `.secrets/`，仅克隆仓库不包含这些资产。
需保留现有私有资产备份。`reproduction.json` 中的文件清单可用于核验恢复后的数据。
