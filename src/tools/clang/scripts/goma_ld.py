#! /usr/bin/env python
# Copyright (c) 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Linker wrapper that performs distributed ThinLTO on Goma.
#
# Usage: Pass the original link command as parameters to this script.
# E.g. original: clang++ -o foo foo.o
# Becomes: goma-ld clang++ -o foo foo.o

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

import goma_link

import os
import re
import sys


class GomaLinkUnix(goma_link.GomaLinkBase):
  # Target-platform-specific constants.
  WL = '-Wl,'
  TLTO = '-plugin-opt=thinlto'
  SEP = '='
  GROUP_RE = re.compile(WL + '--(?:end|start)-group')
  MACHINE_RE = re.compile('-m([0-9]+)')
  OBJ_PATH = '-plugin-opt=obj-path' + SEP
  OBJ_SUFFIX = '.o'
  PREFIX_REPLACE = TLTO + '-prefix-replace' + SEP
  XIR = '-x ir '

  WHITELISTED_TARGETS = {
      'chrome',
  }

  def analyze_args(self, args, *posargs, **kwargs):
    # TODO(crbug.com/1040196): Builds are unreliable when all targets use
    # distributed ThinLTO, so we only enable it for whitelisted targets.
    # For other targets, we fall back to local ThinLTO. We must use ThinLTO
    # because we build with -fsplit-lto-unit, which requires code generation
    # be done for each object and target.
    if args.output is None or os.path.basename(
        args.output) not in self.WHITELISTED_TARGETS:
      # Returning None causes the original, non-distributed linker command to be
      # invoked.
      return None
    return super(GomaLinkUnix, self).analyze_args(args, *posargs, **kwargs)

  def process_output_param(self, args, i):
    """
    If args[i] is a parameter that specifies the output file,
    returns (output_name, new_i). Else, returns (None, new_i).
    """
    if args[i] == '-o':
      return (os.path.normpath(args[i + 1]), i + 2)
    else:
      return (None, i + 1)


if __name__ == '__main__':
  sys.exit(GomaLinkUnix().main(sys.argv))
