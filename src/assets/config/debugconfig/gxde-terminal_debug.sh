#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
export QT_LOGGING_RULES='org.gxde.terminal*.debug=true;org.gxde.terminal*.info=true'
exec gxde-terminal "$@"
