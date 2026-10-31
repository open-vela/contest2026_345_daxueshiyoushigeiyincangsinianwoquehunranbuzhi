#!/usr/bin/env python3
"""Read-only verification of essential nested TFLM changes before building."""
import hashlib
from pathlib import Path
import subprocess
import sys

TEAM = Path(__file__).resolve().parents[2]
SOURCE = TEAM.parent / 'apps/mlearning/tflite-micro/tflite-micro'
PATCHES = {
    'tflm-movenet-integer-postprocess.patch':
        '68a632f337bd94a38fdd6305bd52f77cc85b9c88180a145451f8d2adf9fed4d5',
    'tflm-esp-nn-existing.patch':
        '2b4e7d2a655900bdfafb7d6d71f0a406458d5e1c8777115ad12eb5872f350208',
}


def main():
    failed = False
    for name, expected in PATCHES.items():
        patch = TEAM / 'tools/patches' / name
        if hashlib.sha256(patch.read_bytes()).hexdigest() != expected:
            print('FAIL: saved patch hash changed:', name)
            failed = True
            continue
        result = subprocess.run(
            ['git', '-C', str(SOURCE), 'apply', '--reverse', '--check', str(patch)],
            capture_output=True, text=True, timeout=15)
        if result.returncode:
            print('FAIL: nested dependency patch absent or changed:', name)
            print('Inspect source and saved patch; no automatic overwrite performed.')
            failed = True
        else:
            print('PASS: nested TFLM source matches saved patch:', name)
    print('Source checks only; no inference or ESP-NN activation claimed.')
    return int(failed)


if __name__ == '__main__':
    sys.exit(main())
