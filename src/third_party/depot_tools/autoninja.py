#!/usr/bin/env vpython
# Copyright (c) 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This script (intended to be invoked by autoninja or autoninja.bat) detects
whether a build is using goma. If so it runs with a large -j value, and
otherwise it chooses a small one. This auto-adjustment makes using goma simpler
and safer, and avoids errors that can cause slow goma builds or swap-storms
on non-goma builds.
"""

# [VPYTHON:BEGIN]
# wheel: <
#   name: "infra/python/wheels/psutil/${vpython_platform}"
#   version: "version:5.6.2"
# >
# [VPYTHON:END]

from __future__ import print_function

import os
import psutil
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))

# The -t tools are incompatible with -j
t_specified = False
j_specified = False
output_dir = '.'
input_args = sys.argv
# On Windows the autoninja.bat script passes along the arguments enclosed in
# double quotes. This prevents multiple levels of parsing of the special '^'
# characters needed when compiling a single file but means that this script gets
# called with a single argument containing all of the actual arguments,
# separated by spaces. When this case is detected we need to do argument
# splitting ourselves. This means that arguments containing actual spaces are
# not supported by autoninja, but that is not a real limitation.
if (sys.platform.startswith('win') and len(sys.argv) == 2 and
    input_args[1].count(' ') > 0):
  input_args = sys.argv[:1] + sys.argv[1].split()

# Ninja uses getopt_long, which allow to intermix non-option arguments.
# To leave non supported parameters untouched, we do not use getopt.
for index, arg in enumerate(input_args[1:]):
  if arg.startswith('-j'):
    j_specified = True
  if arg.startswith('-t'):
    t_specified = True
  if arg == '-C':
    # + 1 to get the next argument and +1 because we trimmed off input_args[0]
    output_dir = input_args[index + 2]
  elif arg.startswith('-C'):
    # Support -Cout/Default
    output_dir = arg[2:]

use_goma = False
use_jumbo_build = False

# Attempt to auto-detect goma usage.  We support gn-based builds, where we
# look for args.gn in the build tree, and cmake-based builds where we look for
# rules.ninja.
if os.path.exists(os.path.join(output_dir, 'args.gn')):
  with open(os.path.join(output_dir, 'args.gn')) as file_handle:
    for line in file_handle:
      # This regex pattern copied from create_installer_archive.py
      if re.match(r'^\s*use_goma\s*=\s*true(\s*$|\s*#.*$)', line):
        use_goma = True
        continue
      match_use_jumbo_build = re.match(
          r'^\s*use_jumbo_build\s*=\s*true(\s*$|\s*#.*$)', line)
      if match_use_jumbo_build:
        use_jumbo_build = True
        continue
elif os.path.exists(os.path.join(output_dir, 'rules.ninja')):
  with open(os.path.join(output_dir, 'rules.ninja')) as file_handle:
    for line in file_handle:
      if re.match(r'^\s*command\s*=\s*\S+gomacc', line):
        use_goma = True
        break

# If GOMA_DISABLED is set to "true", "t", "yes", "y", or "1" (case-insensitive)
# then gomacc will use the local compiler instead of doing a goma compile. This
# is convenient if you want to briefly disable goma. It avoids having to rebuild
# the world when transitioning between goma/non-goma builds. However, it is not
# as fast as doing a "normal" non-goma build because an extra process is created
# for each compile step. Checking this environment variable ensures that
# autoninja uses an appropriate -j value in this situation.
goma_disabled_env = os.environ.get('GOMA_DISABLED', '0').lower()
if goma_disabled_env in ['true', 't', 'yes', 'y', '1']:
  use_goma = False

# Specify ninja.exe on Windows so that ninja.bat can call autoninja and not
# be called back.
ninja_exe = 'ninja.exe' if sys.platform.startswith('win') else 'ninja'
ninja_exe_path = os.path.join(SCRIPT_DIR, ninja_exe)

# Use absolute path for ninja path,
# or fail to execute ninja if depot_tools is not in PATH.
args = [ninja_exe_path] + input_args[1:]

num_cores = psutil.cpu_count()
if not j_specified and not t_specified:
  if use_goma:
    args.append('-j')
    core_multiplier = int(os.environ.get('NINJA_CORE_MULTIPLIER', '40'))
    j_value = num_cores * core_multiplier

    if sys.platform.startswith('win'):
      # On windows, j value higher than 1000 does not improve build performance.
      j_value = min(j_value, 1000)
    elif sys.platform == 'darwin':
      # On Mac, j value higher than 500 causes 'Too many open files' error
      # (crbug.com/936864).
      j_value = min(j_value, 500)

    args.append('%d' % j_value)
  else:
    j_value = num_cores
    # Ninja defaults to |num_cores + 2|
    j_value += int(os.environ.get('NINJA_CORE_ADDITION', '2'))
    if use_jumbo_build:
      # Compiling a jumbo .o can easily use 1-2GB of memory. Leaving 2GB per
      # process avoids memory swap/compression storms when also considering
      # already in-use memory.
      physical_ram = psutil.virtual_memory().total
      GB = 1024 * 1024 * 1024
      j_value = min(j_value, physical_ram / (2 * GB))
    args.append('-j')
    args.append('%d' % j_value)

# On Windows, fully quote the path so that the command processor doesn't think
# the whole output is the command.
# On Linux and Mac, if people put depot_tools in directories with ' ',
# shell would misunderstand ' ' as a path separation.
# TODO(yyanagisawa): provide proper quating for Windows.
# see https://cs.chromium.org/chromium/src/tools/mb/mb.py
for i in range(len(args)):
  if (i == 0 and sys.platform.startswith('win')) or ' ' in args[i]:
    args[i] = '"%s"' % args[i].replace('"', '\\"')

if os.environ.get('NINJA_SUMMARIZE_BUILD', '0') == '1':
  args += ['-d', 'stats']

print(' '.join(args))
