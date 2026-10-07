#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check a CMake DESTDIR tree for paths that prevent Deepin Terminal coexistence.

Usage: python3 tests/packaging/check-layout.py /path/to/staging/root
"""
import sys
from pathlib import Path

root = Path(sys.argv[1])
files = {p.relative_to(root).as_posix() for p in root.rglob('*')
         if p.is_file() or p.is_symlink()}
required = {
    'usr/bin/gxde-terminal',
    'usr/share/applications/gxde-terminal.desktop',
    'usr/share/icons/hicolor/scalable/apps/gxde-terminal.svg',
    'usr/share/gxde-terminal/translations/deepin-terminal_zh_CN.qm',
    'usr/share/dsg/configs/org.gxde.terminal/org.gxde.terminal.json',
}
missing = required - files
conflicts = {name for name in files if any(part in name for part in (
    '/deepin-terminal/', '/org.deepin.terminal', '/libterminalwidget',
    '/share/terminalwidget', '/include/terminalwidget', '/cmake/terminalwidget',
    '/pkgconfig/terminalwidget',
)) or name.endswith(('/deepin-terminal.desktop', '/deepin-terminal.svg', '/bin/deepin-terminal'))}
if missing or conflicts:
    raise SystemExit(f'Missing expected files: {sorted(missing)}\nConflicting paths: {sorted(conflicts)}')
if not any('/libgxde-terminalwidget' in name and '.so' in name for name in files):
    raise SystemExit('Missing GXDE-specific terminal widget library')
print(f'PASS: {len(files)} installed files use independent GXDE paths')
