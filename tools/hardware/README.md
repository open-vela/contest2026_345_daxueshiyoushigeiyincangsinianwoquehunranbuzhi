# ESP32-P4 hardware validation helpers

These host-side scripts require Python 3 and `pyserial` on the host that owns
the USB Serial/JTAG COM port.  On the current Windows/WSL setup they are run
with Windows Python and `--port COM3`.

Run one or more NSH commands and retain the transcript:

```powershell
python run_nsh.py --port COM3 --timeout 1840 `
  --log g1-stability.log --expect "G1 PASS" "g1_smoke 1800"
```

Capture and validate a guarded RAW10 frame after a power cycle:

```powershell
python run_nsh.py --port COM3 --timeout 25 `
  --log csi-dw-gdma-post-power-com3.log `
  --expect "CSI DMA STABILITY PASS" `
  "sc2336_probe dma-capture 1 720p"
```

Run bounded camera stability validation:

```powershell
python run_nsh.py --port COM3 --timeout 330 `
  --log csi-dw-gdma-300s-com3.log `
  --expect "CSI DMA STABILITY PASS" `
  "sc2336_probe dma-stability 300 720p"
```

`run_nsh.py` retries the same COM port if USB Serial/JTAG briefly disappears
during re-enumeration.  A physical power removal/restoration must still be
recorded separately; reopening COM3 is not evidence of a cold power cycle.

Run ten software warm resets:

```powershell
python reset_cycles.py --port COM3 --mode reboot --cycles 10 `
  --log g1-warm-reset-10x.log
```

Record twenty true cold power cycles.  Disconnect and reconnect board power
only when prompted; changing only DTR/RTS or restarting the COM device is not
a cold-power-cycle substitute:

```powershell
python reset_cycles.py --port COM3 --mode manual-power --cycles 20 `
  --log g1-cold-boot-20x.log
```

The optional `--mode rts` exercises the external reset pin but must be labeled
as a hard-reset test rather than a cold-power-cycle test.
