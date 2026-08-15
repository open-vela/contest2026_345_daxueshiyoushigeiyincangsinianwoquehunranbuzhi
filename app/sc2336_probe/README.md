# SC2336 read-only sensor probe

The AG638A32M2 module contains a SmartSens SC2336 image sensor.  This command
performs only two read-only SCCB transactions through `/dev/i2c0`:

```text
nsh> sc2336_probe
SC2336 ID PASS address=0x30 id=0xcb3a
```

The expected 7-bit address is `0x30`; registers `0x3107` and `0x3108` must
return `0xcb` and `0x3a`.  Register addresses are 16-bit big-endian and values
are 8-bit.  The command neither writes sensor registers nor enables streaming,
MIPI CSI, DMA, or frame buffers.
