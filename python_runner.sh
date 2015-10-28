# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

## This file is designed to be sourced from a bash script whose name takes the
## form 'command-name'. This script will then instead invoke
## '[depot_tools]/command_name.py' correctly under mingw as well
## as posix-ey systems, passing along all other command line flags.

## Example:
## echo ". python_runner.sh" > git-foo-command
## ./git-foo-command  #=> runs `python git_foo_command.py`

## Constants
PYTHONDONTWRITEBYTECODE=1

## "Input parameters".
# If set before the script is sourced, then we'll use the pre-set values.
#
# SCRIPT defaults to the basename of $0, with dashes replaced with underscores

DEPOT_TOOLS="${0%/*}"
# Sometimes commands will run with no path (e.g. a git command run from within
# the depot_tools dir itself). In that case, treat it like it was run like:
# "./command"
if [[ "$DEPOT_TOOLS" = "$0" ]]; then
  DEPOT_TOOLS="."
fi
BASENAME="${0##*/}"
SCRIPT="${SCRIPT-${BASENAME//-/_}.py}"

if [[ -e "$DEPOT_TOOLS/python.bat" && $OSTYPE = msys ]]; then
  cmd.exe //c "$DEPOT_TOOLS\\python.bat" "$DEPOT_TOOLS\\$SCRIPT" "$@"
else
  exec "$DEPOT_TOOLS/$SCRIPT" "$@"
fi
