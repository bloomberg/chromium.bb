#!/usr/bin/env vpython
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import sys

import common
import sizes_common


def main_compile_targets(args):
  json.dump(['cronet'], args.output)


if __name__ == '__main__':
  funcs = {
      'run': sizes_common.main_run,
      'compile_targets': main_compile_targets
  }
  sys.exit(common.run_script(sys.argv[1:], funcs))
