# Local ESP32-P4 hardware documents

## Required source set

Use `/home/uleemos/pdf & md/esp32_P4` as the default document root. Run
`scripts/hardware-docs-preflight.sh` before pin- or register-dependent work.

| Source | Role | Current SHA-256 |
| --- | --- | --- |
| `SCH_ESP32-P4X_FUNCTION_EV_BOARD_V1.8_20260805.pdf` | Exact V1.8 board wiring and fitted ESP32-C6-MINI-1 module, connector pins, nets, pull resistors, rails, fitted/NC parts | `a87457fdcc4c8b3b3b5461603ce87a82b2ce9e99d851e4e615d4f5732aa649a5` |
| `esp32-p4_datasheet_cn.pdf` | Pin capabilities, IO/LP IO MUX, strapping, electrical and package limits | `ef4ae149dd391ce7e2c520415492cef537ab662bb5710fb495efedf34dbc73a5` |
| `esp32-p4_technical_reference_manual_cn.pdf` | Peripheral architecture, programming sequence, interrupts, clocks, DMA and register fields | `9e2d9d36b0e2c058e37bf581c1da527a88943f522e61c4ccd16cbe153b20c0e0` |
| `esp-chip-errata-zh_CN-master-esp32p4.pdf` | Revision-specific hardware restrictions and workarounds | `d11bd62f1274999c60180d774e46351987482d040ed6d94d6522ded0062251a1` |

Treat hashes as document identity evidence, not as a reason to reject a newer
intentional revision. When a file changes, re-read it and update this table.

The former `ESP32_P4X_C5_Function_EV_board-2.0-schematics.pdf` is not an
authoritative source for this board. Do not reuse its C5 module, component
reference, or routing conclusions without re-verifying them in the V1.8
schematic above.

## Source priority

1. Use the exact board schematic for physical connectivity and fitted parts.
2. Use the datasheet for pad capabilities, mux choices, strapping, voltage,
   drive strength, and electrical restrictions.
3. Use the TRM for controller behavior and register programming.
4. Apply errata to the detected silicon revision.
5. Compare official code, Apache NuttX, and ESP-IDF only after the hardware
   facts above are established.

If sources conflict, do not average them. Identify the board revision,
silicon revision, document revision, and exact conflict before proceeding.

## Search and inspection workflow

1. List and identify the PDFs with `rg --files`, `file`, and `sha256sum`.
2. Search extracted text for the exact net, GPIO, peripheral, register, and
   field names. Preserve both PDF page number and printed manual page number.
3. Render the relevant schematic sheet at high resolution and visually trace
   the net through connectors, resistors, jumpers, and device pins. Text
   extraction alone is insufficient for a schematic.
4. Cross-check every selected GPIO in the datasheet pin overview, IO MUX
   table, strapping chapter, and electrical limits.
5. For register-level work, cite the TRM controller chapter plus register and
   bit-field names. Confirm addresses and field definitions against the HAL
   headers pinned by the current build.
6. Search errata for the peripheral and detected chip revision before
   declaring the design valid.
7. Write a short evidence ledger before code changes.

Use `pdftotext -layout`/`pdfinfo` when available. A temporary isolated PDF
tool environment is acceptable when they are unavailable. Do not commit
extracted manuals, rendered pages, or temporary tool environments.

## Topic routing

- GPIO/pinmux: schematic sheets 2-3; datasheet pin overview, IO MUX, LP IO
  MUX, analog functions, strapping and electrical chapters; TRM GPIO/IO MUX.
- I2C: schematic sheets 3-4 for shared `ESP_I2C_SCL`/`ESP_I2C_SDA`; datasheet
  pin matrix; TRM I2C and clock/interrupt chapters; I2C errata.
- SPI: schematic sheets 2, 3 and 5 for flash, headers and SD interfaces;
  datasheet SPI2/SPI3 mux; TRM GP-SPI, DMA, clock and interrupt chapters;
  SPI errata.
- Camera/display: schematic sheet 3 CSI/DSI connectors and shared I2C;
  datasheet MIPI pins; TRM MIPI, DMA/cache and clock chapters.
- C6/network/storage/audio: schematic sheets 4-6 and every shared rail,
  reset, wakeup, SDIO/RMII/I2S/I2C net involved.

## Verified board V1.8 facts

Board identity: `ESP32-P4X_FUNCTION_EV_BOARD`, revision 1.8, dated
2026-08-05, with U1 `ESP32-C6-MINI-1` on schematic sheet 5 of 6.

From schematic sheet `03_FLASH_DBG_CNN`, sheet 3 of 6:

- J1 pin 3 is `CNN_GPIO7`; J1 pin 5 is `CNN_GPIO8`.
- J1 pin 13 is `CNN_GPIO20`; J1 pin 11 is `CNN_GPIO21`.
- BOOT button SW2 directly pulls `GPIO35_BOOTMODE` low; R208 is the populated
  10 kΩ pull-up, and sheet 2 connects that net through populated 0 Ω R207
  to GPIO35. The former `ESP_BOOT` net name came from the wrong board record.
- GPIO35 is a boot-mode strap and must not be driven to the download-mode
  level during reset. It also routes through populated 0 Ω R135 to
  `RMII_TXD1`, so the BOOT-button GPIO device cannot be assumed independent
  when Ethernet is enabled.
- The CSI and DSI connectors share `ESP_I2C_SCL` and `ESP_I2C_SDA`; audio on
  sheet 4 uses those same I2C nets.

From schematic sheet `02_ESP32-P4`, sheet 2 of 6:

- GPIO7 connects through populated 0 Ω R194 to `ESP_I2C_SDA`; GPIO8 connects
  through populated 0 Ω R190 to `ESP_I2C_SCL`. Do not short them for a GPIO
  loopback even though both also appear on J1.
- GPIO20/GPIO21 connect through R33/R39 to `CNN_GPIO20`/`CNN_GPIO21` and J1,
  with no other on-board consumer shown. Use GPIO20-to-GPIO21 for the current
  loopback test.
- GPIO0/GPIO1 connect through R61/R59 to the 32.768 kHz crystal Y1; their J1
  branches R199/R197 are NC. Do not use them without explicit board rework.

From schematic sheet `04_Audio`, sheet 4 of 6:

- `ESP_I2C_SDA`/`ESP_I2C_SCL` connect through populated 0 Ω resistors
  R62/R52 to ES8311 `CDATA`/`CCLK`.
- The main board shows only 22 pF shunt capacitors C24/C27 on those I2C nets;
  no populated discrete pull-up resistors are shown. The verified I2C smoke
  firmware therefore uses open-drain pads and the ESP32-P4 internal pull-ups.

From schematic sheet `05_Ethernet_SDMMC_WiFi`, sheet 5 of 6:

- U1 is `ESP32-C6-MINI-1`, not an ESP32-C5 module.
- ESP32-P4 GPIO14/GPIO15/GPIO16/GPIO17/GPIO18/GPIO19 route through
  R25/R42/R24/R41/R23/R40 to `SD2_D0`/`SD2_D1`/`SD2_D2`/`SD2_D3`/
  `SD2_CLK`/`SD2_CMD`, which connect to the C6 module.
- ESP32-P4 GPIO54 routes through R76 to `C6_EN`; GPIO6 routes through R30 to
  `C6_WAKEUP`. These pins are reserved when the C6 module is enabled.

## Evidence ledger template

```text
Board/schematic revision:
Silicon revision:
Feature and observable milestone:
Schematic sheet/page, connector pins, nets, fitted parts:
Datasheet PDF/printed pages and relevant tables:
TRM PDF/printed pages, chapter, registers and fields:
Errata item/revisions:
Pin/peripheral conflicts checked:
Selected configuration and rejected alternatives:
Remaining assumptions and hardware tests:
```
