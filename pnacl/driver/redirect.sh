#!/bin/sh

# Keep this script compatible with different shells.
# It has been tested to work with: bash, zsh, ksh93, and dash.

DRIVER_BIN="`dirname "$0"`"
TOOLNAME="`basename "$0"`"
PYDIR="${DRIVER_BIN}/pydir"
. "${DRIVER_BIN}"/findpython.sh
${PYTHON} "${PYDIR}/loader.py" "${TOOLNAME}" "$@"
