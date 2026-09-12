# CSI DW-GDMA PSRAM capture validation

- Date: 2026-09-12
- Board: ESP32-P4X Function EV Board V1.8
- SoC: ESP32-P4 rev v3.2
- Sensor: SC2336, chip ID `0xcb3a`
- Console / flash port: `COM3` (USB Serial/JTAG)
- NuttX driver commit: `d3b28596e8d2d02bf2a41967103c2edb6ca1d8e5`
- Final firmware SHA-256: `120e4e4cdc89c91ef25624a2cb2f9fa11ba2e945ec7765b09916ecec5c80c0dc`
- Post-commit rebuild SHA-256: `ff23c28962b864c2ecafdec017023ce39840523a22e379e5f4873e03dd9a1774`
- 300-second firmware SHA-256: `a3361b523eeb85a4f3ad0d1820fa910a7a5d00d9f27dd4a6f40735d0c79f13be`

The final build generalizes the bridge burst calculation and avoids a shared
global DW-GDMA reset.  Its 720p result remains 128 words, identical to the
long-run build.  All three modes and the 100-frame case were then
regression-tested on the final build.

## Results

| Test | Result |
| --- | --- |
| 720p RAW10 first frame | PASS, 1,152,000 bytes, CRC32 `776b58c3`, guard PASS |
| 1080p30 RAW10 first frame | PASS, 2,592,000 bytes, CRC32 `846ce9bb`, guard PASS |
| 1080p25 RAW10 first frame | PASS, 2,592,000 bytes, CRC32 `d0afc06a`, guard PASS |
| 720p 100 frames | PASS, 100 frames, 99 CRC changes, zero DMA/guard errors |
| 720p 300 seconds | PASS, 2,132 frames, 2,131 CRC changes, zero DMA/guard errors |
| Power-cycle then 720p first frame | PASS, 1,152,000 bytes, CRC32 `4f707140`, guard PASS, zero DMA errors |

The 300-second run used the immediately preceding firmware identified above.
The final firmware removes the shared global DW-GDMA reset and computes the
largest power-of-two CSI bridge burst that exactly divides each packed RAW10
frame.  Its 720p burst remains 128 words, so the capture data path exercised by
the long run is unchanged.  The final image was separately regression-tested
with all three modes, 100 frames, and the post-power-cycle first frame.

Raw transcripts:

- `csi-dw-gdma-modes-com3.log`
- `csi-dw-gdma-100frames-com3.log`
- `csi-dw-gdma-final-com3.log`
- `csi-dw-gdma-300s-com3.log`
- `csi-dw-gdma-post-power-com3.log`

## Build and scope

The final image was built successfully with GCC 13.4.0 using:

```sh
env PATH=/home/uleemos/openvela-contest/.pip_packages/bin:/home/uleemos/openvela-contest/prebuilts/gcc/linux-x86_64/riscv-none-elf/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  make -C nuttx EXTRAFLAGS='-Wno-cpp -Wno-deprecated-declarations' -j8
```

This closes CAM-005 and the staged 1-frame/100-frame/5-minute CAM-007 checks.
The standard `/dev/video0` consumer interface and the documented 30-minute
full-system soak are not claimed by this result.

The post-commit rebuild completed and was written to COM3 with flash hash
verification.  After the esptool hard reset, Windows could enumerate COM3 but
even a plain `uname -a` write timed out and caused a transient disconnect.  No
camera command ran in that attempt, so this is tracked as a host/USB console
re-enumeration issue rather than a DMA test failure.  Repeat the serial
regression after another physical power cycle; do not count this attempt as a
pass or fail of the camera data path.
