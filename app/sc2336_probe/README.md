# SC2336 Sensor Probe and Control Plane Utility

The AG638A32M2 module contains a SmartSens SC2336 image sensor.  This utility
provides SCCB control-plane tests over `/dev/i2c0` at 100 kHz and staged MIPI
CSI-2/DW-GDMA capture tests into guarded PSRAM frame buffers.

## Commands

- `sc2336_probe probe`
  Read-only ID probe for `0x3107/0x3108`, expected `0xcb3a`.
- `sc2336_probe reset`
  Perform sensor software reset (`0x0103 = 0x01`) and verify chip recovery.
- `sc2336_probe init [720p|1080p|1080p25]`
  Load 2-lane 24 MHz XVCLK initialization table and verify key mode registers.
- `sc2336_probe stream-on`
  Enable stream output (`0x0100 = 0x01`) and verify status.
- `sc2336_probe stream-off`
  Return sensor to standby mode (`0x0100 = 0x00`) and verify status.
- `sc2336_probe test [720p|1080p|1080p25]`
  Run sequential control plane workflow (Probe -> Reset -> Init -> Verify -> Stream-ON -> Hold -> Stream-OFF).
- `sc2336_probe cycle <N> [720p|1080p|1080p25]`
  Execute N stream transitions to verify control-plane stability across repeated stream state changes.
- `sc2336_probe csi-test [720p|1080p|1080p25]`
  Run the sensor and CSI Host/D-PHY/Bridge link smoke test without allocating
  frame buffers.
- `sc2336_probe dma-capture <N> [720p|1080p|1080p25]`
  Capture N packed RAW10 frames into PSRAM.  Each frame reports the actual DMA
  byte count, CRC32, sequence number, and guard status.
- `sc2336_probe dma-stability <seconds> [720p|1080p|1080p25]`
  Repeat guarded frame capture for the requested duration and fail on DMA
  timeout, decoder/slave/LLI error, guard corruption, or a static multi-frame
  CRC stream.

## Acceptance baseline (2026-09-12)

- NuttX driver commit: `d3b28596e8d2d02bf2a41967103c2edb6ca1d8e5`
- Final firmware SHA-256:
  `120e4e4cdc89c91ef25624a2cb2f9fa11ba2e945ec7765b09916ecec5c80c0dc`
- 720p, 1080p30, and 1080p25 first-frame capture: PASS.
- 720p 100-frame capture: PASS, 99 CRC changes and zero DMA/guard errors.
- 720p 300-second capture: PASS, 2,132 frames, 2,131 CRC changes and zero
  DMA/guard errors.
- A power-cycle followed by a COM3 720p first-frame retest also passed.

See `hardware-logs/csi-dw-gdma-validation.md` for the exact result ledger and
raw COM3 transcript index.  This is currently a board validation API; standard
NuttX `/dev/video0` integration remains a separate follow-up.
