#!/usr/bin/env bash

set -u

doc_root=${1:-'/home/uleemos/pdf & md/esp32_P4'}
status=0

required_docs=(
  'ESP32_P4X_C5_Function_EV_board-2.0-schematics.pdf'
  'esp32-p4_datasheet_cn.pdf'
  'esp32-p4_technical_reference_manual_cn.pdf'
  'esp-chip-errata-zh_CN-master-esp32p4.pdf'
)

printf 'ESP32P4_HARDWARE_DOC_ROOT=%s\n' "$doc_root"

if [[ ! -d "$doc_root" ]]; then
  printf 'ERROR document root is missing\n' >&2
  exit 2
fi

printf '\n[required documents]\n'
for name in "${required_docs[@]}"; do
  path="$doc_root/$name"
  if [[ ! -r "$path" ]]; then
    printf 'MISSING %s\n' "$path" >&2
    status=1
    continue
  fi

  printf 'PRESENT %s\n' "$path"
  file "$path"
  sha256sum "$path"
done

printf '\n[PDF inspection tools]\n'
for tool in pdfinfo pdftotext mutool pymupdf; do
  if command -v "$tool" >/dev/null 2>&1; then
    printf 'AVAILABLE %s=%s\n' "$tool" "$(command -v "$tool")"
  else
    printf 'NOT_IN_PATH %s\n' "$tool"
  fi
done

if [[ "$status" -ne 0 ]]; then
  printf '\nFAIL required local hardware evidence is incomplete.\n' >&2
  exit "$status"
fi

printf '\nPASS required local hardware documents are readable.\n'
printf 'Read references/hardware-documents.md before choosing pins or registers.\n'
