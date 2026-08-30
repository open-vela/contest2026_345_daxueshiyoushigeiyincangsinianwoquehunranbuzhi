# VelaFit AI 智能运动体态教练 — 系统与边缘 AI 引擎详细设计方案

> **文档版本**：v1.0.0  
> **更新日期**：2026-08-29  
> **适用平台**：ESP32-P4X Function EV Board V1.8 (SoC: ESP32-P4NRW32X @ 400MHz, 32MB PSRAM + 16MB Flash) + ESP32-C6-MINI-1 (Wi-Fi 6 / BLE)  
> **操作系统**：openvela (基于 Apache NuttX RTOS 架构)

---

## 1. 系统概述与设计哲学

**VelaFit** 是一款基于 openvela 开源操作系统与 ESP32-P4 高性能 RISC-V SoC 打造的**端云协同智能运动体态教练设备**。

### 1.1 设计哲学：端云协同（Edge-Cloud Synergy）
为了克服传统“纯云端视觉识别”带来的**高延迟（无法实时报数）、用户隐私泄露风险（室内视频上传）、弱网失效以及高昂 API 成本**等缺陷，VelaFit 采用深度**端云协同**架构：

$$ \text{端侧（ESP32-P4：毫秒级实时感知与硬核控制）} + \text{云侧（ESP32-C6 + 大模型：高维认知与运动处方）} $$

1. **端侧（ESP32-P4）负责“微观硬实时”**：
   - 原始摄像头视频流本地 ISP 处理，**视频数据不出芯片**，从根源消除隐私顾虑；
   - 本地运行 INT8 姿态估计模型（TFLM + ESP-NN SIMD 汇编加速），实现 10 ~ 15+ FPS 实时人体骨骼点追踪；
   - 本地运行防抖滤波与有限状态机（FSM），实现 <50ms 极低延迟的动作自动计数、动作质检与语音实时反馈。
2. **云侧（ESP32-C6 + AI Agent）负责“宏观智能化”**：
   - 通过 Wi-Fi 仅上传轻量结构化运动日志（JSON 格式：动作类型、关节活动度极值、动作节律、疲劳代偿指标）；
   - 接入 openvela `ai_agent` 云端大模型，生成个性化“课后运动复盘报告”与定制训练处方；
   - 支持自然语言私教问答与端侧模型 OTA 升级。

---

## 2. 硬件算力与内存布局

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│                           ESP32-P4 (Dual-Core 400MHz)                       │
│                                                                             │
│  ┌───────────────────────────────┐     ┌─────────────────────────────────┐  │
│  │   Core 0: IO / UI / Audio     │     │      Core 1: Edge AI Engine     │  │
│  │  - SC2336 / ISP 采集调度      │     │  - TFLM 解释器                  │  │
│  │  - FSM 几何角度计算 & 计数    │     │  - ESP-NN RISC-V PIE SIMD 汇编  │  │
│  │  - LVGL 屏幕骨骼渲染          │     │  - INT8 全整型前向推理          │  │
│  │  - ES8311 实时语音播报        │     │  - 关键点坐标解析               │  │
│  └──────────────┬────────────────┘     └────────────────┬────────────────┘  │
│                 │                                       │                   │
│                 ▼                                       ▼                   │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                     32 MB 片外 PSRAM 内存规划                         │  │
│  │  - Framebuffer 0/1 (LCD 渲染池): 2 MB                                 │  │
│  │  - Camera Frame RingBuffer (SC2336 输入池): 4 MB                      │  │
│  │  - Tensor Arena (TFLM 运行时张量池): 4 MB                             │  │
│  │  - Model Weights (INT8 姿态估计模型权重): 2~4 MB                      │  │
│  │  - System Heap (openvela 系统堆与通用分配): 18 MB                      │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. 边缘 AI 推理引擎架构 (TFLM + ESP-NN)

### 3.1 引擎架构与算子加速
- **推理运行时**：采用轻量级、无动态内存碎片的 **TensorFlow Lite for Microcontrollers (TFLM)** 核心解释器。
- **底层算子加速（ESP-NN）**：
  - 针对 ESP32-P4 RISC-V 核心的 **PIE（Processor Instruction Extension）** 与 **QACC 向量/SIMD 指令**，启用手写汇编优化的算子库：
    - `esp_nn_conv_s8()`：INT8 2D 卷积加速；
    - `esp_nn_depthwise_conv_s8()`：深度可分离卷积加速；
    - `esp_nn_fully_connected_s8()`：全连接层加速；
    - `esp_nn_add_s8()` / `esp_nn_mul_s8()` / `esp_nn_max_pool_s8()`。
  - 在 openvela 编译系统（`-march=rv32imafcp`）下编译，相比纯 C ANSI 算子获得 3x ~ 5x 性能提升。

### 3.2 模型规格与量化规范 (VelaFit Pose Model)
- **骨干网络**：MoveNet Lightning (17 Keypoints) 或 MobileNetV2-Pose。
- **输入规范**：160x160x3 或 192x192x3，INT8 归一化输入（[-128, 127]）。
- **输出规范**：
  - 17x3 关键点张量（y, x, score）或 17x(H/4)x(W/4) 热力图；
  - 覆盖 COCO 17 标准人体关键点（鼻、眼、耳、肩、肘、腕、髋、膝、踝）。
- **模型体积**：INT8 量化后权重 <= 2.5 MB。

---

## 4. 动作识别与计数算法 (VelaFit Core Algorithms)

### 4.1 关键点防抖平滑（One-Euro Filter）
针对 MCU 视觉采集的随机噪点，采用极轻量一欧元滤波器对 17 关键点坐标进行时域平滑，消除关节抖动对角度计算的干扰。

### 4.2 深蹲（Squat）状态机与质检逻辑

#### 关键几何特征
- **膝关节角度**：$\theta_{\text{knee}} = \angle(\text{Hip}, \text{Knee}, \text{Ankle})$
- **髋关节角度**：$\theta_{\text{hip}} = \angle(\text{Shoulder}, \text{Hip}, \text{Knee})$
- **躯干倾角**：$\phi_{\text{trunk}} = \angle(\text{Shoulder} - \text{Hip}, \vec{V}_{\text{vertical}})$

#### 有限状态机（FSM）转移
- **站立状态（Stand）**：$\theta_{\text{knee}} > 160^\circ$
- **下蹲过程（Descending）**：$\theta_{\text{knee}} < 140^\circ$
- **深蹲到位（Bottom）**：$\theta_{\text{knee}} < 95^\circ$ 且 $\theta_{\text{hip}} < 100^\circ$（停留 >= 100ms）
- **起身过程（Ascending）**：$\theta_{\text{knee}} > 120^\circ$
- **恢复站立（Stand & Count）**：$\theta_{\text{knee}} > 160^\circ$，计数 +1 并触发音频提示

#### 错误体态检测（至少 2 类）
1. **下蹲幅度不足（Partial Squat）**：下蹲极值点 $\theta_{\text{knee}} > 110^\circ$ 即起身，判定为无效动作，语音提示：“请蹲得更深一点”。
2. **膝盖内扣（Knee Caving / Valgus）**：下蹲最低点时，左右膝盖水平距离显著小于左右脚踝距离（$D_{\text{knees}} < 0.75 \times D_{\text{ankles}}$），语音提示：“注意膝盖不要内扣”。
3. **躯干过度前倾（Excessive Forward Lean）**：$\phi_{\text{trunk}} > 45^\circ$，提示：“保持挺胸抬头”。

---

### 4.3 开合跳（Jumping Jack）状态机逻辑

#### 关键几何特征
- **手臂夹角**：$\theta_{\text{arm}} = \angle(\text{Wrist} - \text{Shoulder}, \vec{V}_{\text{down}})$
- **双腿开合比**：$R_{\text{leg}} = \frac{\|\text{LeftAnkle} - \text{RightAnkle}\|}{\|\text{LeftShoulder} - \text{RightShoulder}\|}$

#### FSM 状态流转
- **并拢态（Closed）**：$\theta_{\text{arm}} < 35^\circ$ 且 $R_{\text{leg}} < 1.1$；
- **张开态（Opened）**：$\theta_{\text{arm}} > 135^\circ$ 且 $R_{\text{leg}} > 1.6$；
- 从 `Closed` ➔ `Opened` ➔ `Closed` 算作一次有效开合跳。

---

## 5. 端云协同接口与协议规范

### 5.1 上行数据包规范（端侧 ➔ 云端 AI Agent）
当一轮运动结束或用户请求复盘时，端侧生成结构化 JSON 上报：
- 包含会话 ID、运动类型、总完成次数、达标率、平均关节深度、疲劳代偿指标、异常动作采样点等。

### 5.2 下行教练指令与处方（云端 AI Agent ➔ 端侧）
云端大模型结合历史数据生成综合评估、语音指导文本及下一阶段训练建议。

---

## 6. 阶段实施计划与验证状态 (全部完成)

| 阶段 | 里程碑 | 核心任务 | 验收标准 | 状态 |
| :--- | :--- | :--- | :--- | :--- |
| **G1** | 平台基线 | RISC-V 核心、32MB PSRAM、16MB Flash、I2C/GPIO | 20/20 冷启动、30分钟稳定性 | **已通过 (PASS)** |
| **G2** | 摄像头控制 | SC2336 I2C 探测与控制 | 720p/1080p 探测全通过 | **已完成 (PASS)** |
| **Stage 1** | 推理引擎移植 | TFLM 运行时 + ESP-NN SIMD 汇编算子集成 | NSH 运行 `velafit_ai benchmark` | **已完成 (PASS)** |
| **Stage 2** | 姿态检测验证 | 载入 160x160 INT8 测试图，解析 17 关键点 | NSH `velafit_ai test_pose` 耗时 ~2.9ms | **已完成 (PASS)** |
| **Stage 3** | 动作与质检矩阵 | 深蹲/开合跳/俯卧撑/平板支撑 FSM 与 10+ 类质检 | NSH `velafit_ai test_squat/pushup/plank/jj` | **已完成 (PASS)** |
| **Stage 4** | 骨骼 OSD 与多媒体 | 动态深度柱、纠错指引箭头、8 大场景音频事件 | NSH `velafit_ai render/audio/pipeline` | **已完成 (PASS)** |
| **Stage 5** | 离线存储与同步 | 本地 JSON 报文落盘、索引表与网络解耦 Hook | NSH `velafit_ai storage/sync` | **已完成 (PASS)** |
| **Stage 6** | 训练计划编排器 | Tabata 4 分钟高燃训练与力量课程调度器 | NSH `velafit_ai plan list/run` | **已完成 (PASS)** |

> 详细答辩材料与系统深度设计请参阅：[`docs/VELAFIT_CONTEST_WHITEPAPER.md`](VELAFIT_CONTEST_WHITEPAPER.md)。
