# VelaFit 边缘 AI 与小米 MIMO 多模态大模型端云协同架构与工程实施白皮书

> **项目名称**：VelaFit 边缘 AI 智能运动体态教练 — 小米 MIMO 端云协同系统  
> **所属赛题**：2026 openvela 开源操作系统与 RISC-V 创新大赛  
> **文档版本**：v1.0.0 (Engineering Baseline)  
> **更新日期**：2026-08-30  
> **关联大模型**：小米 MIMO 系列模型 (`mimo-v2.5-pro`, `mimo-v2.5-asr`, `mimo-v2.5-tts-voiceclone`, `mimo-v2.5-tts`, `mimo-v2.5`)  
> **团队代号**：Contest 2026 Team 345  

---

## 1. 架构愿景与设计哲学

### 1.1 端云协同设计哲学

为了克服传统“纯云端视觉/语音识别”带来的**高延迟、隐私泄露风险、弱网瘫痪与高并发服务器成本**，VelaFit 确立了**“端侧硬实时微观控制 + 云端大模型宏观高维认知”**的双层协同架构：

```mermaid
flowchart TD
    subgraph "端侧 (ESP32-P4: 毫秒级边缘智能 / 绝对隐私安全)"
        A["麦克风采集 (ES8311 /dev/audio/pcm_in0)"] --> B["端侧本地 KWS 关键词唤醒 (velafit_kws)"]
        B -->|触发唤醒 'Ding'| C["语音命令录制 (3~5s PCM)"]
        D["17 关键点姿态追踪 (ESP-NN SIMD)"] --> E["生物力学 FSM 动作质检与计数"]
        E --> F["本地离线落盘 (/data/velafit/sessions/ JSON)"]
    end

    subgraph "传输层 (ESP32-C6 Wi-Fi 6: 解耦注册 Hook)"
        C --> G["velafit_sync / velafit_cloud_agent"]
        F --> G
        G --> H["ESP32-C6 Wi-Fi 链路"]
    end

    subgraph "云端 (小米 MIMO 多模态大模型私教大脑)"
        H -->|语音流 (Base64)| I["mimo-v2.5-asr (语音转文本)"]
        I --> J["mimo-v2.5-pro (多模态私教核心大脑)"]
        H -->|结构化运动 JSON| J
        J -->|运动生理学归因与个性化处方| K["mimo-v2.5-tts-voiceclone / mimo-v2.5-tts"]
        K -->|高品质私教语音流| H
    end

    H -->|下行处方卡片 + TTS 音频| L["ESP32-P4 (LCD 呈现复盘卡片 + ES8311 扬声器播报)"]
```

---

## 2. 小米 MIMO 模型矩阵分工与角色定义

| 模型代号 | 核心角色 | 端云链路位置 | 输入数据 | 输出数据 |
|---|---|---|---|---|
| **`mimo-v2.5-asr`** | **语音识别网关** | 云端入库 | 唤醒后录制的 16kHz 16-bit PCM 语音片段 | 转录文本指令（如 `"开始深蹲训练"`、`"复盘刚才的动作"`） |
| **`mimo-v2.5-pro`** | **多模态私教大脑** | 云端推理 | ASR 意图文本 + 端侧运动指标 JSON（最小关节角极值、浅蹲/塌腰/内扣具体次数、MET 卡路里） | 多维评分、肌肉代偿归因分析、运动处方（`prescription`） |
| **`mimo-v2.5`** | **通用对话助手** | 云端推理 | 用户自由咨询的健身/营养学问答文本 | 自然语言指导与科普回答 |
| **`mimo-v2.5-tts-voiceclone` / `mimo-v2.5-tts`** | **私教语音合成** | 云端出库 | `mimo-v2.5-pro` 生成的私教点评文本 | 充满活力的教练声线音频流（16kHz PCM / MP3） |

---

## 3. 为什么关键词唤醒（KWS）必须留在本地端侧？

1. **用户隐私合规（Zero Privacy Risk）**：
   - 室内私密生活场景下，日常对话绝对不能未经唤醒 24 小时不间断上传云端。本地 KWS 确保**未唤醒前任何麦克风音频数据不出芯片**。
2. **极低功耗与零常态网络开销**：
   - 本地常开监听使用微秒级定点轻量级特征匹配，平时 Wi-Fi 处于低功耗待机状态，零路由器带宽占用。
3. **毫秒级瞬时响应 (<200ms)**：
   - 依托 ESP32-P4 RISC-V 400MHz 硬件算力，本地 KWS 单次推断 <1ms，从叫出唤醒词到响起 `Ding` 唤醒音零延迟。
4. **断网韧性可用（Offline Resilient）**：
   - 无 Wi-Fi 情况下，本地 KWS 依然能够触发离线 Tabata 课程调度与动作计次。

---

## 4. 端云多模态交互协议规范 (JSON 契约)

### 4.1 上行：运动指标复盘报文（端侧 ➔ `mimo-v2.5-pro`）

```json
{
  "version": "1.0.0",
  "device_id": "esp32p4_velafit_01",
  "session_id": "VF-20260830-1001",
  "plan_id": "PLAN-TABATA-001",
  "exercise_type": "squat",
  "duration_seconds": 60,
  "total_reps": 15,
  "valid_reps": 12,
  "accuracy_pct": 80.0,
  "calories_kcal": 14.50,
  "metrics": {
    "min_knee_angle_deg": 84.5,
    "avg_rep_duration_ms": 1850,
    "fault_counts": {
      "shallow": 1,
      "valgus": 2,
      "trunk_lean": 0
    }
  },
  "cloud_target_model": "mimo-v2.5-pro"
}
```

### 4.2 下行：云端私教处方与 TTS 报文（`mimo-v2.5-pro` ➔ 端侧）

```json
{
  "status": "SUCCESS",
  "session_id": "VF-20260830-1001",
  "model": "mimo-v2.5-pro",
  "score": {
    "overall": 86,
    "accuracy": 82,
    "stamina": 90
  },
  "analysis": {
    "primary_fault": "Knee Valgus (Inward Caving)",
    "fatigued_muscle": "Gluteus Medius & Hips",
    "coach_commentary": "整体深蹲节奏非常好！但在后半程起身时检出了2次轻微膝盖内扣，这表明您的臀中肌开始出现疲劳代偿。"
  },
  "prescription": {
    "recommended_drill": "弹力带横向行走 2 组 (激活臀中肌)",
    "next_target_reps": 15
  },
  "tts_response": {
    "model": "mimo-v2.5-tts-voiceclone",
    "voice_style": "energetic_coach",
    "audio_format": "pcm16k_mono",
    "requires_playback": true
  }
}
```

---

## 5. 工程实施路线图与进度跟踪表

| 序号 | 模块 / 任务名称 | 核心文件路径 | 核心技术点 | 当前状态 |
|:---:|---|---|---|:---:|
| **1** | **端云 MIMO 协议头文件定义** | [`include/velafit_mimo_protocol.h`](../app/velafit_ai/include/velafit_mimo_protocol.h) | 定义 ASR、PRO、TTS 请求与处方结构体 | **✅ 已完成 (PASS)** |
| **2** | **端侧本地 KWS 唤醒引擎** | [`algo/velafit_kws.c/h`](../app/velafit_ai/algo/velafit_kws.c) | 16kHz 定点能量/过零率特征匹配 + 状态机 | **✅ 已完成 (PASS)** |
| **3** | **唤醒专用提示音频（Ding）** | [`audio/velafit_audio_cue.c/h`](../app/velafit_ai/audio/velafit_audio_cue.c) | `VELAFIT_AUDIO_CUE_WAKEUP` (1046Hz ➔ 1318Hz) | **✅ 已完成 (PASS)** |
| **4** | **MIMO 云端交互客户端与仿真器** | [`sync/velafit_cloud_agent.c/h`](../app/velafit_ai/sync/velafit_cloud_agent.c) | ASR 意图解析 + MIMO-PRO 运动医学推演与处方生成 | **✅ 已完成 (PASS)** |
| **5** | **CLI 交互与全流程自检套件** | [`velafit_ai_main.c`](../app/velafit_ai/velafit_ai_main.c) | `velafit_ai kws` / `velafit_ai cloud` / `all` | **✅ 已完成 (PASS)** |
| **6** | **C6 物理 Wi-Fi 链路对接 (队友负责)** | `sync/velafit_sync.c` | 队友完成 C6 Wi-Fi 后注册 `velafit_sync_register_sender` | **⏸️ 预留待对接** |
| **7** | **真机麦克风唤醒与扬声器处方播报** | `/dev/audio/pcm_in0` & `pcm0` | 开发板物理连接后现场实测录音与播放 | **⏸️ 待物理实板** |

---

## 6. 验证指令与操作指南 (SOP)

```bash
# 1. 运行本地关键词唤醒引擎 (KWS) 仿真测试
nsh> velafit_ai kws test

# 2. 运行小米 MIMO 多模态端云协同交互 (ASR + PRO + TTS) 仿真
nsh> velafit_ai cloud

# 3. 运行全套自检验证（包含 Stage 1~6 + KWS + MIMO）
nsh> velafit_ai all
```
