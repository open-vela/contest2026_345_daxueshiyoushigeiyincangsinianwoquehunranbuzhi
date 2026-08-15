# G1 platform smoke test

`g1_smoke` verifies the foundational ESP32-P4 platform services without
enabling another peripheral subsystem.

Quick validation:

```text
nsh> g1_smoke 10
```

G1 stability run:

```text
nsh> g1_smoke 1800
```

The test uses a 10 Hz POSIX timer, checks monotonic elapsed time, repeatedly
allocates/fills/verifies/frees heap blocks, and reports heap usage once per
minute.  With the contest board configuration, the user heap is the package
PSRAM.  It also reads and advances `/data/g1-persist.bin`; a `PERSIST PASS`
line after reset proves that the dedicated SPI flash partition retained the
previous record.

Exit status is zero only when all checks pass.  The 30-minute run does not
replace the separate 20 cold-power-cycle and 10 `reboot` tests.
