# ESP32-P4 bring-up milestones

## M0: Reproducible reference

- Build Apache NuttX `esp32p4-function-ev-board:nsh` out of tree.
- Record the Apache commit, compiler path/version, configuration command, build command, image names, and sizes.
- Flash the reference image and capture the complete serial boot log.

Exit criterion: the official reference reaches a stable `nsh>` prompt on the same physical board.

## M1: openvela compile baseline

- Add only the Kconfig, Make/CMake integration, headers, linker layout, and source set needed for a minimal image.
- Resolve openvela API differences explicitly instead of copying unrelated current-master common-layer code.
- Keep a written dependency ledger mapping Apache source symbols to openvela equivalents.

Exit criterion: a clean openvela build produces an ESP32-P4 ELF and flashable image without unexplained warnings.

## M2: First serial execution

- Validate reset vector, stack, memory regions, clock initialization, interrupt setup, timer tick, heap, and low-level UART.
- Add the earliest possible progress markers without depending on the full syslog stack.
- Decode resets and exceptions; do not treat repeated boot output as progress.

Exit criterion: deterministic serial progress reaches the NuttX start path across repeated cold resets.

## M3: Minimal NSH

- Add minimal Function EV Board initialization and `configs/nsh/defconfig`.
- Disable unrelated peripherals and services.
- Validate scheduler, idle task, console, procfs/basic filesystem behavior, and heap stability.

Exit criterion: ten repeated cold boots reach `nsh>` and `uname -a`, `free`, `ps`, `ls`, and `help` behave consistently.

## M4: Foundational peripherals

Add one subsystem per commit, with build and hardware evidence for each:

1. GPIO loopback and button interrupt using exact-board schematic evidence.
2. I2C controller and one known device transaction.
3. SPI controller and loopback or known peripheral.
4. Flash/storage and mount/read/write test.

## M5: Board capabilities

Add PSRAM, Ethernet, USB, display/touch, camera, and audio only after M3 is stable. For each subsystem document pins, clocks, DMA/cache constraints, device nodes, commands, expected output, and untested cases.

## Required evidence for every milestone

- Exact Git commit IDs for every modified repo.
- Exact build command and exit status.
- Toolchain identity.
- Artifact names and sizes.
- Full relevant serial log, including reset cause and failures.
- `repo status` output proving no unintended project changes.
- A concise list of hardware actually tested.
