#!/usr/bin/env python3
"""给新同步的 TFLM 源码应用 VelaFit 的两个依赖补丁。

顺序固定：先 tflm-esp-nn-existing，再 tflm-movenet-integer-postprocess。
用法：
    python tools/hardware/apply_tflm_patches.py [--source <tflite-micro 目录>]

重复执行安全：已应用的补丁跳过，失败时明确报错、不做任何覆盖。
"""
import argparse
import subprocess
from pathlib import Path

TEAM = Path(__file__).resolve().parents[2]
DEFAULT_SOURCE = (TEAM.parent / 'apps' / 'mlearning' / 'tflite-micro'
                  / 'tflite-micro')
PATCHES = [
    'tflm-esp-nn-existing.patch',
    'tflm-movenet-integer-postprocess.patch',
]


def git(args, cwd):
    return subprocess.run(['git', '-C', str(cwd), *args],
                          capture_output=True, text=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, default=DEFAULT_SOURCE)
    args = parser.parse_args()
    source = args.source

    if not (source / '.git').exists():
        print('SKIP: 未找到 TFLM 源码仓库:', source)
        return 1

    failed = False
    for name in PATCHES:
        patch = TEAM / 'tools' / 'patches' / name
        if not patch.exists():
            print('FAIL: 缺少补丁:', patch)
            failed = True
            continue

        if git(['apply', '--reverse', '--check', str(patch)],
               source).returncode == 0:
            print('PASS: 已应用，跳过:', name)
            continue

        if git(['apply', '--check', str(patch)], source).returncode:
            print('FAIL: 补丁无法应用（源码版本不匹配）:', name)
            failed = True
            continue

        applied = git(['apply', str(patch)], source)
        if applied.returncode:
            print('FAIL: 应用失败:', name, applied.stderr.strip())
            failed = True
        else:
            print('PASS: 已应用:', name)

    print('仅源码检查，未做推理验证。')
    return 1 if failed else 0


if __name__ == '__main__':
    raise SystemExit(main())
