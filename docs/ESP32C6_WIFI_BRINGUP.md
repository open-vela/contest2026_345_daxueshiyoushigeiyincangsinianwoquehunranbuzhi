# ESP32-P4 / ESP32-C6 Wi-Fi 链路移植与验收记录

## 1. 基线与范围

- 板卡：ESP32-P4X Function EV Board V1.8（2026-08-05）。
- 主控：ESP32-P4 rev v3.2；协处理器：ESP32-C6-MINI-1。
- 团队仓基线：`559c2b90ecaabd58d59427826112f1ff2bb38bad`（PR #13）。
- NuttX 基线：PR #340 的 CSI 提交
  `d3b28596e8d2d02bf2a41967103c2edb6ca1d8e5`。
- C6 固件：ESP-Hosted-MCU 1.4.7，SDIO transport。

本阶段完成 P4 到 C6 的 SDIO/CMD53 DMA、ESP-Hosted RPC 和 Wi-Fi STA
关联。当前验收范围是链路层关联，不包含 NuttX netdev、IPv4/DHCP 或数据面。

## 2. 硬件连接依据

原理图 `SCH_ESP32-P4X_FUNCTION_EV_BOARD_V1.8_20260805.pdf` sheet 5/6
给出以下连接：

| P4 GPIO | C6 信号 | 用途 |
| --- | --- | --- |
| GPIO14～17 | SD2_D0～D3 | SDIO 4-bit data |
| GPIO18 | SD2_CLK | SDIO clock |
| GPIO19 | SD2_CMD | SDIO command |
| GPIO54 | C6_EN | C6 低有效复位 |
| GPIO6 | C6_WAKEUP | C6 wakeup（本阶段保留） |

P4 SDMMC slot 1 支持 GPIO matrix 路由。GPIO54 必须显式连接
`SIG_GPIO_OUT_IDX` 后再驱动；否则只配置输出方向不能保证 C6_EN 电平变化。

## 3. PR #342 的复用与补齐

openvela NuttX PR #342（提交
`86e229ca9c2bb5488d4896ce9e8e9909c2cb559e`）提供了 P4 SDMMC host、
slot 1 GPIO matrix、LDO_VO4、内部描述符 DMA、cache 同步以及通用
CMD5/CMD52/CMD53 流程。本实现补齐了实板所需差异：

1. 低速分频前清除粘滞的 SDIO high-speed bypass 位；初始 host divider 为 2；
2. 保持 `CTRL.INTENABLE`，按参考序列使用 `CLKENA.LOWPOWER`；
3. 枚举期间保持 DAT3 GPIO 输出高，CCCR 切换后再进入 4-bit；
4. CMD52/53 返回错误传播、R4/R5 处理和 IORDY 等待修正；
5. C6 复位释放后等待 2.5 秒，避免 C6 尚未完成 SDIO 初始化时写 IOEN。

PR #342 不包含 ESP-Hosted RPC 和 Wi-Fi STA 关联，不能只用 CIS probe 作为
Wi-Fi 联网证据。

## 4. ESP-Hosted 实现要点

- function 0/1 block size 均为 512 字节，4-bit、20 MHz。
- SLC 寄存器访问使用低 10 位地址；FIFO 保留 17 位窗口地址，不能套用寄存器
  的 `0x3ff` 掩码。
- FIFO CMD53 按 512 字节 block mode 传输，ESP-Hosted header 保持实际帧长。
- 接收长度使用累计字节计数器，避免只依赖可丢失的中断边沿。
- 同一累计增量中的多个连续 RPC 帧逐帧解析，避免吞掉 `StaConnected`。
- 支持 proto3 零值字段省略、RPC 超时重试和断开 reason 解析。
- STA 流程为 WifiInit、SetMode(STA)、SetConfig、WifiStart、WifiConnect，
  等待 `Event_StaConnected(775)`；全信道关联失败重试次数为 3。

工具命令：

```text
c6_wifi probe
c6_wifi version
c6_wifi connect <ssid> <password>
```

密码仅作为运行时参数传递，不应写入配置、提交或验收日志。

## 5. 构建、下载与验收

构建：

```sh
export PATH=/home/uleemos/openvela-contest/prebuilts/gcc/linux-x86_64/riscv-none-elf/bin:$PATH
make -C nuttx EXTRAFLAGS="-Wno-cpp -Wno-deprecated-declarations" -j16
```

COM3 下载：

```powershell
py -m esptool --chip esp32p4 --port COM3 --before usb-reset `
  --after hard-reset write-flash 0x2000 nuttx.bin
```

验收日期：2026-09-12。热点为临时 2.4 GHz WPA2 AP，口令已脱敏。

| 验收项 | 结果 |
| --- | --- |
| 构建及镜像生成 | PASS |
| COM3 下载、SHA 校验、自动 hard reset | PASS |
| GPIO54 自动复位 C6 | PASS，COM4 观察到 `rst:0x1 (POWERON)` |
| SDIO CIS | PASS，vendor `0x0092`、device `0x6666` |
| CMD53 双向 DMA | PASS |
| ESP-Hosted 握手及版本 | PASS，1.4.7 |
| Wi-Fi STA 关联 | PASS，收到事件 775 |
| 最终配置冷启动重复性 | PASS，3/3 |
| 连接保持 | PASS，连续 60 秒无断开；10 秒周期 HINF 均保持 IOEN/IORDY |
| IPv4/DHCP/ping | 未纳入本阶段，尚无 NuttX Wi-Fi netdev 数据面 |

调试阶段曾出现一次热点扫描瞬态超时；加入 3 次关联重试后重新执行最终固件
冷启动验收为 3/3。脱敏证据见
`hardware-logs/esp32c6-wifi-acceptance-20260912.log`。

## 6. 后续工作

下一阶段需接入 ESP-Hosted WLAN 数据帧收发、注册 NuttX netdev，并完成
DHCP、网关 ping、吞吐和长稳测试。关联成功不能等同于 IP 数据面可用。

## 7. 参考

- <https://github.com/open-vela/nuttx/pull/342>
- <https://github.com/espressif/esp-hosted-mcu/blob/main/docs/sdio.md>
- <https://github.com/espressif/esp-hosted-mcu/blob/main/docs/porting.md>
