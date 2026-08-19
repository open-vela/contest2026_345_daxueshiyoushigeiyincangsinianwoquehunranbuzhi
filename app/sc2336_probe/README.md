# SC2336 Sensor Probe and Control Plane Utility

The AG638A32M2 module contains a SmartSens SC2336 image sensor.  This utility
provides control-plane operations over `/dev/i2c0` at 100 kHz without requiring
MIPI CSI reception or DMA buffers.

## Commands

- `sc2336_probe probe`
  Read-only ID probe for `0x3107/0x3108`, expected `0xcb3a`.
- `sc2336_probe reset`
  Perform sensor software reset (`0x0103 = 0x01`) and verify chip recovery.
- `sc2336_probe init [720p|1080p]`
  Load 2-lane 24 MHz XVCLK initialization table and verify key mode registers.
- `sc2336_probe stream-on`
  Enable stream output (`0x0100 = 0x01`) and verify status.
- `sc2336_probe stream-off`
  Return sensor to standby mode (`0x0100 = 0x00`) and verify status.
- `sc2336_probe test [720p|1080p]`
  Run sequential control plane workflow (Probe -> Reset -> Init -> Verify -> Stream-ON -> Hold -> Stream-OFF).
- `sc2336_probe cycle <N> [720p|1080p]`
  Execute N stream transitions to verify control-plane stability across repeated stream state changes.
