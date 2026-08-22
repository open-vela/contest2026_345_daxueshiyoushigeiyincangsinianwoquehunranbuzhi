# ES8311 I2S Audio CLI Test Utility (`es8311_audio`)

This application exercises the ES8311 Audio Codec and ESP32-P4 I2S0 peripheral driver on the ESP32-P4X Function EV Board V1.8.

## Hardware Pinout
- **I2C Control**: SDA = GPIO7, SCL = GPIO8 (I2C0, Address 0x18)
- **I2S0 Data**:
  - MCLK: GPIO13
  - BCLK: GPIO12
  - WS / LRCK: GPIO10
  - DOUT (Playback): GPIO9 (ES8311 SDIN)
  - DIN (Recording): GPIO11 (ES8311 SDOUT)
- **Power Amplifier**: PA_EN = GPIO53 (Active High, Onboard Speaker)

## Commands
```bash
# 1. Probe ES8311 Chip ID and verify /dev/audio/pcm0 & /dev/audio/pcm_in0 nodes
nsh> es8311_audio probe

# 2. Dump all ES8311 hardware registers (0x00 .. 0x47)
nsh> es8311_audio dump

# 3. Generate 1000 Hz sine wave tone on speaker (3 seconds)
nsh> es8311_audio tone 1000 3

# 4. Record audio from onboard analog microphone and compute RMS energy
nsh> es8311_audio record 3 /data/rec.raw
```
