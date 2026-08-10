---
name: openvela-esp32p4-porting
description: Port, build, debug, and review openvela/NuttX support for the ESP32-P4 SoC and ESP32-P4X/C5 Function EV Board. Use for ESP32-P4 BSP work, board pins and connectors, pinmux or strapping, register-level development, clocks and interrupts, peripheral drivers, RISC-V architecture code, Espressif common-layer backports, board initialization, Kconfig/defconfig, linker and image generation, UART/NSH bring-up, build failures, hardware smoke tests, or PR preparation in the openvela contest workspace. Do not use for unrelated ESP-IDF applications or non-P4 boards.
---

# Port openvela to ESP32-P4

Work toward the smallest verified milestone. Preserve boundaries between repo-managed projects and never treat Apache NuttX `master` as directly merge-compatible with the openvela contest baseline.

## Enforce the local hardware-document gate

For any task involving a board pin, connector, peripheral function, IO MUX,
strapping pin, register, interrupt source, clock, DMA path, power rail, or
silicon revision, read [hardware-documents.md](references/hardware-documents.md)
and run:

```bash
scripts/hardware-docs-preflight.sh
```

Do this before choosing pins, copying a reference driver, or editing code.
Use the exact board schematic first, then the SoC datasheet, technical
reference manual, and errata. Do not substitute a similarly named Function
EV Board revision for the local ESP32-P4X-C5 board.

Record the following evidence in the task notes or review:

- Exact board and schematic revision.
- Schematic sheet/page, connector pins, and net names.
- Datasheet PDF/printed pages for pin capabilities, IO MUX, strapping, and
  electrical constraints.
- TRM chapter/page and register/field names for register-level behavior.
- Applicable errata item and affected silicon revisions.
- Conflicts with boot mode, flash/PSRAM, console, JTAG, camera, display,
  audio, storage, networking, or another planned subsystem.

If a required local document is missing or unreadable, stop the affected
pin/register work and report the evidence gap. Online documentation may
supplement the local set, but must not silently replace the exact-board
schematic.

## Start every task

1. Apply the local hardware-document gate when the task matches it.
2. Locate the `repo` workspace root and the relevant Git project.
3. Run `repo status` at the workspace root and `git status -sb` in every project that may change.
4. Inspect the current implementation and build configuration before proposing edits.
5. Classify the change using [repository-map.md](references/repository-map.md).
6. State the next observable milestone and its verification command before editing.
7. Run `scripts/preflight.sh` in a new environment, after `repo sync`, or when branch/toolchain state is uncertain.

Do not overwrite, clean, rebase, or discard existing work without explicit approval.

## Preserve repository ownership

- Put reusable ESP32-P4 architecture and SoC support in the `nuttx` project, normally under `nuttx/arch/risc-v/src/esp32p4` plus the smallest required common Espressif and build-system changes.
- Put contest board sources in the team repository under `board/contest_board`. Treat `vendor/openvela/boards/contest2026_345_board` as a manifest-generated mapping; edit the source in the team repository, not the mapped destination.
- Keep applications, documentation, and AI logs in their designated team-repository directories.
- Never commit build outputs, downloaded toolchains, `.config`, generated images, or `.repo` metadata.
- Keep NuttX public-repository changes and team board changes in separate commits and PRs.

## Use the Apache implementation as a reference

Use Apache commit `cda4af9f0026a25275953392ea63245ce339b82b` as the first known ESP32-P4 support reference, then verify its descendants when a needed feature was added later.

Do not bulk-merge Apache `master` or blindly cherry-pick the initial P4 commit. The known initial commit changes 98 files and depends on a substantially different common layer. Instead:

1. List the files and symbols needed for the current milestone.
2. Compare each file against the openvela equivalent and its APIs.
3. Port the smallest coherent dependency slice.
4. Build immediately.
5. Record each incompatibility and the adaptation made.

Preserve license headers and provenance. Do not import Espressif SDK blobs, generated headers, or third-party code until the exact version and license are verified.

## Follow the bring-up order

Read [milestones.md](references/milestones.md) before implementation or review.

Prioritize:

1. Toolchain and image generation.
2. Reset/startup, linker layout, clock, interrupt controller, timer, heap, and UART console.
3. Minimal board initialization and `nsh` defconfig.
4. Boot to a stable `nsh>` prompt.
5. GPIO, I2C, SPI, storage, PSRAM, Ethernet, USB, display, touch, camera, and audio as separate increments.

Do not enable multiple unverified peripherals in the minimal defconfig. Avoid hiding initialization failures; return or log meaningful error codes.

## Build and validate

Prefer the project-provided toolchain selected by the defconfig. Report the resolved compiler path and version in build evidence.

For the contest board, build from the workspace root after `configs/nsh/defconfig` exists:

```bash
./build.sh vendor/openvela/boards/contest2026_345_board/configs/nsh -j"$(nproc)"
```

For the standalone Apache reference, use its out-of-tree CMake build. Do not confuse a successful Apache build with a successful openvela port.

After every meaningful change:

1. Verify the build command exits successfully.
2. Confirm ELF and flashable images exist.
3. Inspect warnings and image size.
4. Check `repo status` for unintended modifications.
5. On hardware, capture reset-to-prompt serial logs.
6. Run `uname -a`, `free`, `ps`, `ls`, and `help` at `nsh>`.

Call a change complete only when its claimed milestone has reproducible evidence. Clearly separate compile-only, boot-tested, and peripheral-tested results.

## Review changes

Check for:

- Wrong repository or edits made through a manifest mapping.
- Missing Kconfig dependencies or stale symbol names.
- Incorrect register addresses, interrupt IDs, clock assumptions, pin mappings, memory regions, or revision handling.
- Missing schematic sheet/net evidence or register-field evidence from the local hardware-document set.
- Reliance on a mismatched ESP-IDF/HAL/toolchain version.
- Silent failures, unsafe ISR behavior, unchecked allocation, and unbounded waits.
- Defconfig bloat unrelated to the milestone.
- Missing build commands, serial logs, attribution, or hardware-test evidence.

Conclude reviews with blockers, required fixes, verification performed, and remaining untested hardware.
