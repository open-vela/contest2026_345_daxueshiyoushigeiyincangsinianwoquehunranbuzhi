# ESP32-P4X Function EV Board 移植记录

## 1. 文档范围

本文记录 Apache NuttX ESP32-P4 支持与比赛 openvela 基线之间的依赖和差异，作为后续增量移植的依据。

当前阶段已在依赖分析基础上完成最小 P4 USB NSH 构建：

- 不复制 Apache 整个目录。
- 不 cherry-pick Apache 提交。
- 只按文件和接口边界增量加入 P4 SoC、公共层适配和团队板级代码。
- 不启用或移植 PSRAM、Ethernet、LCD、Camera、Audio 等后续外设。
- Apache 源码只作为功能和来源参考，比赛交付仍以 openvela `dev-ai-contest-2026` 为基线。

分析日期：2026-08-09。

## 2. 仓库边界和当前状态

### 2.1 工作区

- repo 工作区：`~/openvela-contest`
- openvela NuttX 项目：`~/openvela-contest/nuttx`
- 团队项目：`~/openvela-contest/contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi`
- Apache 参考项目：`~/openvela-reference/nuttx`

### 2.2 Git 状态

| 项目 | 状态 | 说明 |
| --- | --- | --- |
| openvela `nuttx` | `feat/esp32p4-soc-contest2026` | 从 `dd92bcf4257` 创建，已落地最小 P4 SoC 支持和兼容层 |
| 团队项目 | `feat/esp32-p4x-bringup` | 跟踪 `fork/feat/esp32-p4x-bringup` |
| Apache 参考项目 | `master` at `31fbb9921899` | 已有成功的 P4 usbconsole 构建产物 |

开始分析前已经存在、必须保留的工作包括：

- 团队 manifest 修改：`contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi.xml`
- 团队项目未跟踪的 `ai/`
- repo 中 apps/testing 等其他现有修改和 manifest linkfile 映射

本轮没有清理、回退或覆盖这些内容。为读取 Apache partial clone 中缺失的提交对象，执行了 `git fetch apache --tags`；这只更新 `nuttx/.git` 中的远端引用和对象，不改变工作树文件。

## 3. Apache 可复现参考

### 3.1 提交和配置

| 项目 | 值 |
| --- | --- |
| 初始 P4 支持提交 | `cda4af9f0026a25275953392ea63245ce339b82b` |
| 已验证 Apache 参考提交 | `31fbb9921899325fc0c17e39cda90d0d621c4eae` |
| 2026-08-09 获取的 Apache master | `b8e26b127e4c0b17652aba1770da5cda6e6b6fd1` |
| Board config | `esp32p4-function-ev-board:usbconsole` |
| 芯片 revision 配置 | 最低 rev 3.1，范围 3.1 至 3.99 |
| Flash | 4 MiB、DIO、80 MHz |
| Boot | Espressif simple boot |
| Console | USB Serial/JTAG；UART0 未启用 |

从 `31fbb9921899` 到本次获取的 Apache master，以下路径没有新增提交：

```text
arch/risc-v/src/esp32p4/
arch/risc-v/include/esp32p4/
arch/risc-v/src/common/espressif/
boards/risc-v/esp32p4/
tools/espressif/
```

因此本轮分析使用的已验证参考没有遗漏上述路径上的更新。

### 3.2 工具链和产物

已存在构建目录：`~/openvela-reference/nuttx/build-p4-usb`。

| 项目 | 值 |
| --- | --- |
| 构建系统 | CMake + Ninja |
| 编译器 | `/home/uleemos/toolchains/riscv-none-elf-gcc-14.2.0-3/bin/riscv-none-elf-gcc` |
| 版本 | xPack GNU RISC-V Embedded GCC 14.2.0 |
| `nuttx` | 412200 bytes；SHA-256 `4f6f944a5d12178a96fb817386c064a158bc6edc2cc67692024678a0de4412c8` |
| `nuttx.bin` | 228072 bytes；SHA-256 `48f7198e2c371387920bf6e8a9dfb47390679aa89059f7c87c2ce09f9ed0f2a4` |
| `nuttx.hex` | 439335 bytes；SHA-256 `3cde8d89ca9507648d4636321eab6f6a567ceb36f9a04b11820b8394ed145150` |

本轮只检查上述既有构建证据，没有重新构建、烧录或执行新的硬件测试。检查清单已说明该固件此前在同一块 ESP32-P4X Function EV Board 上通过 USB Serial/JTAG 进入 `NuttX-13.0.0` 的 `nsh>`。

## 4. 为什么不能复制目录或 cherry-pick

### 4.1 `cda4af9f002` 不是自包含补丁

该提交修改 98 个文件，统计为 9668 insertions、46 deletions。它同时包含：

- P4 SoC 入口。
- Function EV Board 及 26 个外设/测试配置。
- ADC、I2C、I2S、MCPWM、PCNT、RMT、SPI、TWAI、WDT 等与最小 USB NSH 无关的内容。
- 对 C3、C6、H2 和 RISC-V 公共异常代码的修改。
- 镜像生成和烧录工具修改。

初始 P4 SoC 目录本身只有少量入口文件：

```text
arch/risc-v/include/esp32p4/chip.h
arch/risc-v/src/esp32p4/Kconfig
arch/risc-v/src/esp32p4/Make.defs
arch/risc-v/src/esp32p4/esp_chip_rev.c
arch/risc-v/src/esp32p4/hal_esp32p4.mk
```

复位、时钟、cache/MMU、中断、timer、UART 和 USB Serial/JTAG 的主要实现来自 Apache 在该提交之前已经演进的 `arch/risc-v/src/common/espressif` 和外部 HAL。仅复制 P4 目录会同时缺少头文件、符号、链接脚本和镜像规则。

### 4.2 两个 Git 历史已经长期分叉

openvela 基线和 Apache 初始 P4 提交的 merge-base 是：

```text
9bbacc44ffa9846dc52c08f772cfdea4c6cda255
2018-08-22 fs/hostfs: Add support for open() append mode
```

从该 merge-base 计算：

- openvela 一侧有 39676 个独有提交。
- Apache P4 一侧有 28492 个独有提交。

这说明 Apache master 不能被视为比赛分支的线性升级源。

### 4.3 公共 Espressif 层差异过大

快照统计：

| 快照 | `arch/risc-v/src/common/espressif` 文件数 |
| --- | ---: |
| openvela `dd92bcf4257` | 79 |
| `cda4af9f002` 的父提交 | 104 |
| Apache 参考 `31fbb9921899` | 130 |

openvela `dd92bcf4257` 到 `cda4af9f002` 父提交的相关快照差异涉及 170 个文件，约 25026 insertions、7648 deletions。这里包含大量与 P4 无关的 Wi-Fi、外设和公共 API 改造，不能整体覆盖。

### 4.4 外部 HAL 不是兼容的小版本升级

| 使用方 | `esp-hal-3rdparty` commit | P4 路径情况 |
| --- | --- | --- |
| openvela `dd92bcf4257` | `9fc713a95b1ff150dd0b0647e465d3c624056bb1` | 只有一个早期 `esp_cpu_intr.c` 路径 |
| Apache 初始 P4 | `a85ce2f1bad9f745090146eb30a18d91b8ddd309` | 约 722 个 P4 路径 |
| Apache 已验证参考 | `8d0a898910084206721a0892ab093021bca1496a` | 约 722 个 P4 路径 |

`9fc713a...` 与 `a85ce2f...` 不是线性祖先关系；两者快照差异超过一万个文件。因此不能为了 P4 直接替换 openvela 的全局 HAL pin，否则可能破坏现有 ESP32-C3/C6/H2。

建议在 P4 `Make.defs` 进入公共 Espressif `Make.defs` 之前设置 P4 专用的 `ESP_HAL_3RDPARTY_VERSION`。公共文件使用 `ifndef` 设置默认版本，允许这种按芯片覆盖。具体 pin 在实现前仍需确认许可证、下载缓存行为及既有芯片的回归策略。

Apache 已验证 usbconsole 的 `compile_commands.json` 中约有 227 个 `esp-hal-3rdparty` 源文件参与编译；链接 map 的 archive dependency 部分按对象名交叉核对约有 112 个 HAL 对象进入最小镜像依赖闭包。即使只做 USB NSH，HAL 依赖仍然不可简化成少数寄存器头文件。

## 5. 依赖台账

### 5.1 最小 USB NSH 所需层次

```text
usbconsole defconfig
├── Function EV Board：boot、bringup、reset
├── P4 SoC：Kconfig、chip.h、revision、HAL source manifest
├── Espressif 公共层
│   ├── reset/start、clock、BSS、MMU/cache
│   ├── CLIC IRQ、vectors、exception
│   ├── system timer
│   └── USB Serial/JTAG、低层日志、serial registration
├── esp-hal-3rdparty 精确版本
├── P4 rev3 linker scripts
└── esptool image：simple boot offset 0x2000
```

### 5.2 文件/API 对照

| Apache 文件或功能 | openvela 对应位置 | 主要差异 | 处理原则 |
| --- | --- | --- | --- |
| `arch/risc-v/Kconfig` P4 entry | 同路径 | openvela 仍保留 `ARCH_CHIP_ESPRESSIF` 和旧 Espressif choice 模型 | 只加入 P4 选择和必要 `select`，不顺带重构 C3/C6/H2 |
| `arch/risc-v/src/esp32p4/Kconfig` | 当前不存在 | P4 双核、400 MHz、rev 3.x、cache line 和 workaround | 首版只保留 rev3.1、CPU、cache、最小 boot 所需项 |
| `arch/risc-v/include/esp32p4/chip.h` | 当前不存在 | 芯片能力由外部 HAL 的 `irq.h`、GPIO signal headers 补全 | 保留公共接口，生成/下载头文件不提交到仓库 |
| `arch/risc-v/src/esp32p4/hal_esp32p4.mk` | 当前不存在 | 初始提交有 164 条 source entry，参考版本有 213 条 | 先建立可解释的最小 source manifest；外设源后续按配置加入 |
| `esp_start.c` | 同路径 | Apache 新增 CLIC MTVT、chip revision、BSS clear、SPI flash state、MMU map；cache HAL 函数签名不同 | 以 openvela 文件为底稿按功能移植，不能覆盖 |
| `esp_irq.c/.h` | 同路径 | openvela 使用旧三参数 `esp_setup_irq`；Apache 新 API传 handler/arg，并使用 HAL interrupt handle、CLIC、SMP map | 保留旧芯片 ABI；为 P4增加兼容层或条件实现 |
| `esp_vectors.S` | 同路径 | openvela 没有 P4 CLIC `_mtvt_table`；Apache 同时支持 PLIC/CLIC | 只引入经 P4 宏保护的 CLIC vector table |
| `riscv_exception_common.S` | 同路径 | P4 需要对异常 cause 做 0..63 mask | 作为独立 RISC-V 公共修复审查 |
| `esp_timerisr.c` | 同路径 | P4 使用 `SYSTIMER_TARGET0_INTR_SOURCE`，旧芯片使用 edge source | 用芯片宏隔离差异 |
| `esp_lowputc.c` | 同路径 | 新 HAL 使用 `uart_periph_signal`、新的 RCC/clock 使能和 generic UART context | USB console 首版避免迁入 LP UART/RS485 等无关重构 |
| `esp_serial.c` | 同路径 | P4 TX empty raw bit 名称不同 | 迁入小型 P4 条件分支 |
| `esp_usbserial.c` | 同路径 | LL interrupt mask 名称变化；P4 需显式 bus clock、PHY defaults、source-to-IRQ；IRQ attach 流程变化 | 依据 P4 HAL做条件适配，不能照搬新版 IRQ API到所有芯片 |
| P4 common linker scripts | Apache `boards/risc-v/esp32p4/common/scripts` | rev <3 和 rev3 内存布局不兼容 | 当前实板只采用 rev3.1 路径；通用 SoC layout 归公共 `nuttx` |
| board `scripts/Make.defs` | 团队 `board/contest_board` | Apache 会选择 `sections.rev3.ld` | 团队仓仅保留板级 wrapper；通用 layout 不藏进团队目录 |
| `tools/espressif/Config.mk` | 同路径 | P4 simple boot/MCUBoot 的 app/bootloader offset 是 `0x2000` | 形成独立、P4 条件化的 image generation 修改 |
| Apache board bringup | 团队 `board/contest_board/src` | Apache bringup 同时注册大量外设 | 首版只做最小 boot/reset/USB console，不移植外设注册 |
| Apache usbconsole defconfig | 团队 `board/contest_board/configs/nsh` | Apache 新版 symbol 包含部分 openvela 不存在或语义不同的配置 | 以 openvela Kconfig 解析结果为准逐项添加 |

## 6. 后续 Apache 提交分类

### 6.1 必须吸收语义，不直接 cherry-pick

| Commit | 作用 | 采用方式 |
| --- | --- | --- |
| `cda4af9f002` | 初始 P4、Function EV Board、公共层条件和 image offset | 只选取最小启动所需文件和 hunk |
| `81602e16a20` | 恢复受宏保护的 P4 revision check | 纳入启动/revision 逻辑；不得绕过实板 revision 检查 |
| `4047b9f42b8` | RISC-V Espressif 启动时清除 BSS | 纳入 reset/start 最小闭环 |
| `2c8dc175102` | P4 L1/L2 cache line 配置 | 纳入 cache 基础配置，避免后续 cache/DMA 假设错误 |
| `612aca73ce0` 的 HAL pin 结果 | Apache 参考更新到 `8d0a8989...` | 作为 P4 专用可复现 pin 评估，不全局应用该提交 |

### 6.2 仅 CMake 路径需要，第一轮可推迟

openvela `build.sh` 默认走 Make；只有显式 `--cmake` 才走 CMake。因此第一轮先保证默认比赛构建入口，CMake 作为独立逻辑单元处理：

| Commit | 作用 |
| --- | --- |
| `ed217f8f3ff` | Espressif RISC-V CMake 基础支持 |
| `e59604bbe61` | CMake flashing target |
| `4f1a3356f90` | CMake 与 Make source set 对齐 |
| `19c1d86a16` | board CMake linker script 操作修复 |
| `5adbddf52ac` | P4/C3/C6/H2 HAL CMake linker script 修复 |
| `5053734b503` | CMake 生成 `irq.h` 的依赖顺序修复 |

### 6.3 当前最小里程碑不需要

- `b606180da92`：只为 rev <3 增加 `sram_high` heap；当前板卡配置为 rev3.1。
- `8c42fda257f` 及后续 RTC GPIO。
- `dedf9045c13`、`6480bb231dd`、`7e321e0aba3`：LP core/LPUART/LPI2C。
- `d6a55824b38`、`92a0b18ca11`：PSRAM。
- `5c4c60f9d26`、`c63c061a52c`：Ethernet。
- touch、analog comparator、LP mailbox、TWAI2、MIPI-DSI、LCD、touchscreen 等后续驱动。
- `73e243584eb`、`51f77ee111f`：Apache 板级初始化和 `NSH_ARCHINIT` API 清理；应适配 openvela 当前 API，而不是反向升级 openvela。
- `e6b08b2a1e`、`eb0834cd27`：只影响非 usbconsole 的外设配置。

## 7. 建议的最小移植和提交顺序

芯片层修改归 `nuttx` 项目；Function EV Board 修改归团队项目。两条轨道必须分开提交和 PR。

### N1：配置、工具链和 HAL 边界

内容：

- `ARCH_CHIP_ESP32P4` 和必要 Kconfig source。
- P4 `Kconfig`、`Make.defs`、`chip.h` 骨架。
- P4 专用 `esp-hal-3rdparty` pin 和许可证/来源记录。
- 首轮只支持 Make，不加入 Apache 全套 CMake。

验收：P4 defconfig 能通过配置解析，无 unknown/stale symbol；既有 C3/C6/H2 defconfig 不因 P4 选择而改变。

### N2：memory map、linker 和 image generation

内容：

- rev3.1 memory layout、aliases、flat memory 和 sections linker scripts。
- P4 simple boot 的 `0x2000` image offset。
- ROM linker fragments 的引用方式。

验收：能链接最小 ELF，并从本次产物生成可由 esptool 识别的 BIN；记录完整 chip、flash mode/frequency/size 和 offset。

### N3：reset/start、clock、BSS、cache/MMU 和 revision

内容：

- 复位入口和 `__esp_start`。
- `bootloader_clear_bss_section()` 对应语义。
- clock、SPI flash state、MMU/cache 初始化。
- rev3.1 检查和 cache line 配置。

验收：编译和链接成功，且能加入不依赖 syslog 的早期 progress markers。

### N4：CLIC、exception 和 timer

内容：

- CLIC `_mtvt_table`。
- IRQ source/CPU interrupt 映射和动态 vector table。
- exception cause mask。
- system timer interrupt source。

验收：timer tick 和中断路径可编译；硬件上无重复异常/复位后再进入下一步。

### N5：USB Serial/JTAG console

内容：

- USB Serial/JTAG clock、PHY 和 LL interrupt mask。
- console lowputc、serial registration 和 P4 IRQ attach 适配。
- 不启用 UART0、USB OTG 或无关串口功能。

验收：早期日志可见，随后稳定进入 console。

### B1：团队板级最小 USB NSH

内容放在：

```text
contest2026_345_daxueshiyoushigeiyincangsinianwoquehunranbuzhi/
  board/contest_board/
```

只加入：

- team 345 board symbol 和正确板名。
- 最小 boot、reset、bringup。
- `configs/nsh/defconfig`。
- 对公共 NuttX P4 integration commit 的精确 SHA 记录。

不加入 LCD、Camera、Audio、Ethernet、PSRAM 或其他外设。

验收构建命令预期为：

```bash
cd ~/openvela-contest
./build.sh vendor/openvela/boards/contest2026_345_board/configs/nsh -j8
```

配置实际落地前需要先确认 `build.sh` 能识别该路径。第一轮优先默认 Make；CMake 验证单独执行：

```bash
./build.sh vendor/openvela/boards/contest2026_345_board/configs/nsh --cmake -j8
```

## 8. 风险和阻塞点

1. **HAL 版本隔离**：P4 所需 HAL 与 openvela 现有 pin 非线性兼容，必须证明按芯片覆盖不会污染其他 Espressif 构建。
2. **IRQ API 冲突**：Apache 新版 `esp_setup_irq` 签名和 handler ownership 与 openvela 不同。直接替换会使现有公共驱动编译失败或改变 ISR 行为。
3. **CLIC/SMP**：P4 是双核并使用 CLIC；不能假设 C3/C6/H2 的 PLIC/单核路径可以直接复用。
4. **启动和 cache/MMU**：HAL API 签名、cache level 和 internal-memory-via-L1 行为不同，错误通常表现为上电早期无输出或随机 exception。
5. **revision/linker 耦合**：rev <3 与 rev3.x 硬件差异大；当前只面向 rev3.1，不应同时支持两套 layout。
6. **镜像 offset**：P4 simple boot 使用 `0x2000`；烧录时必须读取本次构建生成的参数，不能沿用旧命令。
7. **板级模板占位风险（已解决）**：`board/contest_board` 已改为 team 345 P4X symbol、最小 boot/app/reset 和 USB NSH 配置。
8. **许可证与来源**：NuttX 文件保留 Apache-2.0/SPDX 和原作者；外部 HAL 使用精确 commit 拉取，不把下载目录、生成头文件或 SDK blob 提交进仓库。

## 9. 已落地的最小实现

### 9.1 SoC 和依赖边界

- `nuttx` 已创建具名分支 `feat/esp32p4-soc-contest2026`，基点仍为 `dd92bcf4257`。
- 增量加入 `arch/risc-v/include/esp32p4` 和 `arch/risc-v/src/esp32p4` 的最小入口文件，没有复制 Apache 整个目录。
- P4 单独固定 `esp-hal-3rdparty` 为 `8d0a898910084206721a0892ab093021bca1496a`；C3/C6/H2 继续使用 openvela 原有默认 pin。
- 对公共 Espressif 层只加入 P4 所需的 startup、CLIC IRQ、GPIO、lowputc、serial、timer、USB Serial/JTAG 和 vector 变体；旧芯片仍选择原实现。
- HAL OS adapter 通过单文件 force-include 兼容头适配 openvela 的 `nxtask_init`、`nxsched_usleep`、IRQ 和 `fcntl` API，没有修改下载的 HAL checkout。
- P4 选择 openvela 的 IRQ 保护型 64-bit atomic fallback，以适配 GCC 13.4.0 不提供 RV32 `libatomic.a` 的情况。
- HAL 清单补入 rev3 启动实际依赖的 `pmu_pvt.c`。
- P4 不编译未使用且 ABI 不匹配的通用 `riscv_mtimer.c`，scheduler tick 继续由 Espressif system timer 提供。

### 9.2 团队板级

团队板已从占位模板改为 ESP32-P4X Function EV Board 最小 USB NSH 配置，落在 manifest 映射后的：

```text
vendor/openvela/boards/contest2026_345_board/
```

板级只包含 boot、app initialize、reset、USB console 配置和 rev3 linker scripts。链接脚本额外保留 `esp_start_p4` 的 SRAM 启动段，并采用 Apache 后续修正后的 rev3 MSPI workaround/LP RAM 非重叠布局。

## 10. 构建验证

配置和构建命令：

```bash
cd /home/uleemos/openvela-contest
./build.sh vendor/openvela/boards/contest2026_345_board/configs/nsh olddefconfig
PATH=/tmp/openvela-esptool-venv/bin:$PATH \
  ./build.sh vendor/openvela/boards/contest2026_345_board/configs/nsh -j16
```

结果：

| 项目 | 值 |
| --- | --- |
| 配置 | `olddefconfig` 和 `savedefconfig` 成功 |
| 编译器 | openvela prebuilt `riscv-none-elf-gcc` 13.4.0 |
| HAL | `8d0a898910084206721a0892ab093021bca1496a` |
| 镜像工具 | 隔离安装在 `/tmp/openvela-esptool-venv` 的 esptool 4.12.0；项目最低要求 4.8.0 |
| Flash 参数 | ESP32-P4、4 MiB、DIO、80 MHz、simple boot |
| Flash offset | `0x2000`，来自本次 `tools/espressif/Config.mk` 和 `.config` |
| `nuttx` | 381096 bytes；SHA-256 `c51a445778e62409a871e04dde722490ee5dcd9a51471711408833d205fa4ad9` |
| `nuttx.hex` | 376307 bytes；SHA-256 `41f0f83258926651729cb0fb6b1b348f846281bddfa2fa71f249179c77149d73` |
| `nuttx.bin` | 221316 bytes；SHA-256 `b785f31fafb90ceadca331d7469a999b108b5dfde7e8d62c4d0dcb4b4c61fe36` |

`esptool.py image_info nuttx.bin` 识别为 ESP32-P4 image v1，入口 `0x4ff4447a`，3 个 segment，checksum `0xe9` 有效。最终链接内存占用为：SRAM 26836 bytes、IROM 98434 bytes、DROM 146084 bytes；rev3 MSPI workaround 保留 256 bytes，LP RAM 可用区在修正后为 32488 bytes。

完整重编译出现一条既有公共层告警：`esp_libc_stubs.c` 中 `__assert_func` 被声明为 `noreturn` 但编译器认为可能返回。该告警不影响链接或镜像生成，后续应单独核对 panic/assert 路径，不在最小 bring-up 中扩大修改范围。

## 11. 硬件测试状态

### 11.1 板卡识别和烧录

WSL2 内没有映射 `/dev/ttyACM*`，随后通过 Windows 侧只读枚举确认：

| 项目 | 值 |
| --- | --- |
| Windows port | `COM3` |
| PNP ID | `USB\VID_303A&PID_1001&MI_00` |
| 芯片 | ESP32-P4 revision v3.2 |
| 特性 | Dual Core + LP Core、400 MHz、40 MHz crystal |
| USB mode | USB-Serial/JTAG |
| MAC | `e8:f6:0a:e3:a6:5e` |
| Windows esptool | 5.3.1 |

由于 Windows esptool 不能直接读取 WSL UNC 路径，`nuttx.bin` 临时复制为 `C:\Users\uleem\AppData\Local\Temp\openvela-esp32p4-nuttx.bin`。Windows 侧 SHA-256 为 `B785F31FAFB90CEADCA331D7469A999B108B5DFDE7E8D62C4D0DCB4B4C61FE36`，与 WSL 构建产物一致后才执行烧录：

```text
esptool.exe -c esp32p4 -p COM3 -b 921600 write-flash \
  --flash-size 4MB --flash-mode dio --flash-freq 80m \
  0x2000 C:\Users\uleem\AppData\Local\Temp\openvela-esp32p4-nuttx.bin
```

烧录结果：擦除范围 `0x00002000–0x00038fff`，写入 221316 bytes，写后 hash 校验通过，并通过 RTS 硬复位。

### 11.2 NSH smoke test

首次运行发现 `free` 和 `ps` 因 procfs 未挂载而失败。配置本身已有 `CONFIG_FS_PROCFS=y` 和 `CONFIG_NSH_PROC_MOUNTPOINT="/proc"`，因此只在团队板 `board_app_initialize()` 增加 `nx_mount()`，重建、重刷后最终结果如下：

| 命令/检查 | 结果 |
| --- | --- |
| 冷启动 | 出现 `NuttX-13.0.0` 和 `nsh>`；USB 重新枚举会截断最前面的 0–2 个字符 |
| `help` | 正常，列出 NSH 命令和 `dumpstack/hello/nsh/sh` builtin apps |
| `uname -a` | `NuttX 13.0.0 dd92bcf4257-dirty ... risc-v esp32p4x-function-ev-board` |
| `free` | total 494296、used 6912、free 487384 bytes |
| `ps` | CPU0 IDLE 和 `nsh_main` 两个任务正常 |
| `hello` | `Hello, World!!` |
| `ls /dev` | `console null random ttyACM0 zero` |

完整串口日志：[esp32p4-nsh-smoke-2026-08-09.log](hardware-logs/esp32p4-nsh-smoke-2026-08-09.log)，SHA-256 `ea842146e1497343019c0b355369da9bc642d17bfa8c8cdb316f525eab3c33f8`。

### 11.3 连续复位稳定性

通过 Windows COM3 连续执行 10 次 RTS 硬复位。每一轮都重新运行 `uname -a`，验收条件为返回 `NuttX 13.0.0` 且命令后再次出现 `nsh>`。最终结果为 **10/10 PASS**，未观察到 exception、异常复位、死机或命令通道失联。

循环日志：[esp32p4-reset-stability-10x-2026-08-09.log](hardware-logs/esp32p4-reset-stability-10x-2026-08-09.log)，SHA-256 `4280174843fe97e146053892a97e9ba1392bd09c5a08f62ffa8e693b82e37b65`。

### 11.4 GPIO 阶段（构建和硬件验证通过）

GPIO 阶段没有照搬参考实现中的 GPIO1。2.0 板原理图表明 GPIO0/1
默认通过 R61/R59 连接 32.768 kHz 晶振，通往 J1 的 R199/R197 标为 NC；
GPIO7/8 也分别通过 0 Ω 电阻连接板载共享 I2C SCL/SDA，不能用跳线短接。
因此改用原理图确认只接 J1 排针的 GPIO20/21：

| 设备 | 板级引脚 | J1 | 用途 |
| --- | ---: | ---: | --- |
| `/dev/gpio0` | GPIO20 | pin 13 | 推挽输出 |
| `/dev/gpio1` | GPIO21 | pin 11 | 下拉输入，与 GPIO20 跳线回环 |
| `/dev/gpio2` | GPIO35 | BOOT | 上拉、下降沿按键中断 |

配置已启用 `CONFIG_DEV_GPIO`、`CONFIG_ESPRESSIF_GPIO_IRQ` 和
`CONFIG_EXAMPLES_GPIO`。2026-08-11 在 NuttX commit `b9f8442fa73` 和
团队板 commit `f3fccc570fe` 上完成干净重链接。WSL 当前 Python 环境缺少
`esptool` 包，因此 Make 在最终 MKIMAGE 检查处停止；随后使用已安装的
Windows esptool 5.3.1，按同一构建规则执行 `elf2image --ram-only-header
-fs 4MB -fm dio -ff 80m` 生成最终镜像：

| 产物 | 大小 | SHA-256 |
| --- | ---: | --- |
| `nuttx` | 393876 bytes | `b2b373b8942290d19d90337e2d6e96714c5d2adfc6223158a39eb65fd136e012` |
| `nuttx.hex` | 403610 bytes | `08f20f717aedc64b049162a1ded715acaf7a8fbbbf588b53731e636e7caaafff` |
| `nuttx.bin` | 228172 bytes | `f4c87dcf2c1b34ab22b932c50d7856ac57d380b14b23a5e1adab8d310f61fd91` |

`esptool image-info` 将 `nuttx.bin` 识别为 ESP32-P4、4 MiB、DIO、
80 MHz 镜像，入口为 `0x4ff44638`，checksum `0x3e` 有效。

硬件回环验收步骤：

```text
# 断电后用杜邦线连接 J1 pin 13 (GPIO20) 与 J1 pin 11 (GPIO21)
gpio -o 0 /dev/gpio0
gpio /dev/gpio1              # 期望 Value=0
gpio -o 1 /dev/gpio0
gpio /dev/gpio1              # 期望 Value=1
gpio /dev/gpio2              # 按下并松开 BOOT，期望 poll 返回
```

镜像通过 Windows COM3 写入地址 `0x2000`，esptool 写后 hash 校验成功。
串口 `uname -a` 返回 `b9f8442fa73`，并确认 `/dev/gpio0`、`gpio1`、
`gpio2` 均已注册。GPIO20 输出 0/1 时 GPIO21 分别读取 0/1；恢复 GPIO20
为低电平后，GPIO35 的阻塞 poll 被一次物理 BOOT 按键下降沿唤醒并返回。

完整日志：[esp32p4-gpio-smoke-2026-08-11.log](hardware-logs/esp32p4-gpio-smoke-2026-08-11.log)，
SHA-256 `2c344393966f6f4a3658f934e9d5acf69f28ab4d9d8ec14eb588e4b024d1490f`。

## 12. 下一最小步骤

1. 将本次 GPIO 硬件日志和移植记录形成专属仓后续 commit，并更新对应 PR 的 Testing 证据。
2. 公共 GPIO commit 已追加到 NuttX PR #340；板级 GPIO commit 已推送到专属仓 fork 分支，后续在专属仓独立 PR 合入。
3. 按 I2C、SPI 的顺序各自完成“依赖差异→最小实现→构建→硬件证据→独立 commit”，不要把三个外设压成一个大提交。
4. 补充既有 `esp_libc_stubs.c::__assert_func` noreturn 告警的独立分析，不阻塞后续外设开发。
