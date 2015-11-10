#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys


import common


def main_run(args):
  rc = common.run_runtest(args, [
      '--test-type', 'sizes',
      '--run-python-script',
      os.path.join(
          common.SRC_DIR, 'infra', 'scripts', 'legacy', 'scripts', 'slave',
          'chromium', 'sizes.py')])

  # TODO(phajdan.jr): Implement more granular failures.
  json.dump({
      'valid': True,
      'failures': ['sizes_failed'] if rc else [],
  }, args.output)

  return rc


def main_compile_targets(args):
  json.dump(['chrome'], args.output)


if __name__ == '__main__':
  funcs = {
    'run': main_run,
    'compile_targets': main_compile_targets,
  }
  sys.exit(common.run_script(sys.argv[1:], funcs))
