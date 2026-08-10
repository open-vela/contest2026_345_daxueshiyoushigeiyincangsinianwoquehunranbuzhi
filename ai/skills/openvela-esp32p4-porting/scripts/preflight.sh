#!/usr/bin/env bash

set -u

start_dir=${1:-$PWD}
search_dir=$(readlink -f "$start_dir")

while [[ "$search_dir" != "/" && ! -d "$search_dir/.repo" ]]; do
  search_dir=$(dirname "$search_dir")
done

if [[ ! -d "$search_dir/.repo" ]]; then
  printf 'ERROR workspace root with .repo not found from %s\n' "$start_dir" >&2
  exit 2
fi

workspace=$search_dir
printf 'WORKSPACE=%s\n' "$workspace"

printf '\n[repo status]\n'
(cd "$workspace" && repo status) || true

for project in nuttx vendor/espressif; do
  if [[ -d "$workspace/$project/.git" || -f "$workspace/$project/.git" ]]; then
    printf '\n[%s]\n' "$project"
    git -C "$workspace/$project" status -sb
    git -C "$workspace/$project" log -1 --oneline --decorate
  fi
done

team_repo=$(find "$workspace" -maxdepth 1 -type d -name 'contest2026_345_*' -print -quit)
if [[ -n "$team_repo" ]]; then
  printf '\n[team repository]\n'
  git -C "$team_repo" status -sb
  git -C "$team_repo" log -1 --oneline --decorate
else
  printf '\nWARN team repository contest2026_345_* not found\n'
fi

printf '\n[ESP32-P4 source presence]\n'
for path in \
  nuttx/arch/risc-v/src/esp32p4 \
  nuttx/boards/risc-v/esp32p4 \
  vendor/espressif/boards/esp32p4 \
  vendor/openvela/boards/contest2026_345_board/configs/nsh/defconfig; do
  if [[ -e "$workspace/$path" ]]; then
    printf 'PRESENT %s\n' "$path"
  else
    printf 'MISSING %s\n' "$path"
  fi
done

printf '\n[toolchain]\n'
for compiler in riscv-none-elf-gcc riscv32-esp-elf-gcc; do
  if command -v "$compiler" >/dev/null 2>&1; then
    printf '%s=%s\n' "$compiler" "$(command -v "$compiler")"
    "$compiler" --version | sed -n '1p'
  else
    printf 'NOT_IN_PATH %s\n' "$compiler"
  fi
done

project_compiler="$workspace/prebuilts/gcc/linux-x86_64/riscv-none-elf/bin/riscv-none-elf-gcc"
if [[ -x "$project_compiler" ]]; then
  printf 'PROJECT_PREBUILT=%s\n' "$project_compiler"
  "$project_compiler" --version | sed -n '1p'
fi

printf '\nPreflight is read-only. MISSING entries describe current port state; they are not automatically errors.\n'
