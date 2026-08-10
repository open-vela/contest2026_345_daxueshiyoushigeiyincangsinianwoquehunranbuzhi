# Repository map and contribution boundaries

## Workspace projects

| Responsibility | Development location | Submission path |
| --- | --- | --- |
| ESP32-P4 architecture, startup, IRQ, clocks, timers, UART, generic SoC drivers, common Espressif integration | `nuttx/` feature branch based on `openvela/dev-ai-contest-2026` | Personal `open-vela/nuttx` fork, then PR to the contest branch when required |
| ESP32-P4X-Function-EV-Board pins, board initialization, defconfig, board drivers | `contest2026_345_.../board/contest_board/` | Team repository during the contest; later upstream to `vendor_espressif` when requested |
| Applications and demos | Team repository `app/` | Team repository |
| Reproduction notes and AI Coding logs | Team repository `README.md`, `logs/` | Team repository |

The manifest maps the team board directory to `vendor/openvela/boards/contest2026_345_board`. Always edit the team source path so changes are tracked by the correct Git repository.

## Known baseline facts to re-verify

- Previously observed openvela NuttX baseline: `dd92bcf42573` on `dev-ai-contest-2026`.
- Previously observed baseline contained no `arch/risc-v/src/esp32p4` or `boards/risc-v/esp32p4` tree.
- Apache initial ESP32-P4 support commit: `cda4af9f0026a25275953392ea63245ce339b82b`.
- That Apache commit was previously measured as 98 files, 9668 insertions, and 46 deletions.
- The Git histories have a very old merge base and tens of thousands of unique commits on both sides. Treat this as evidence against wholesale merge, not evidence that the openvela source itself is frozen at the merge-base date.

Re-run these checks after any `repo sync`; do not assume the hashes remain current.

## Branch rules

- Expect repo-managed projects to start in detached HEAD.
- Create a named feature branch before editing a project.
- Push each project to the corresponding personal fork and set its upstream.
- Do not mix `nuttx` changes into the team repository or board changes into the `nuttx` repository.
- Before `repo sync`, commit or otherwise safely preserve all local work and inspect `repo status`.
