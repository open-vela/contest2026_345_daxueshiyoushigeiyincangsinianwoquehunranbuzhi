#!/usr/bin/env python3
"""把一个新同步的 nuttx-apps 树打上 VelaFit 覆盖补丁（LVGL 与 TFLM 构建钩子）。

用法：
    python tools/hardware/apply_apps_overlay.py [--apps <apps 目录>]

重复执行安全：若补丁已应用则直接 PASS 退出。
"""
import argparse
import subprocess
from pathlib import Path

TEAM = Path(__file__).resolve().parents[2]
DEFAULT_APPS = TEAM.parent / 'apps'
PATCH = TEAM / 'tools' / 'patches' / 'apps-velafit-tflm-runner.patch'


def git(args, cwd):
    return subprocess.run(['git', '-C', str(cwd), *args],
                          capture_output=True, text=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--apps', type=Path, default=DEFAULT_APPS)
    args = parser.parse_args()
    apps = args.apps

    if not (apps / '.git').exists():
        print('SKIP: 未找到 apps 仓库:', apps)
        return 1

    # 已应用：反向检查通过
    done = git(['apply', '--reverse', '--check', str(PATCH)], apps)
    if done.returncode == 0:
        print('PASS: apps 覆盖补丁已应用，无需重复')
        return 0

    check = git(['apply', '--check', str(PATCH)], apps)
    if check.returncode:
        print('FAIL: 补丁无法应用（源码版本不匹配）:', check.stderr.strip())
        return 1

    applied = git(['apply', str(PATCH)], apps)
    if applied.returncode:
        print('FAIL: 应用补丁失败:', applied.stderr.strip())
        return 1

    print('PASS: apps 覆盖补丁已应用')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
