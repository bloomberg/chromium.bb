# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import sys

import common

PERF_DASHBOARD_URL = 'https://chromeperf.appspot.com'


def create_argparser():
  parser = argparse.ArgumentParser()
  parser.add_argument('--perf-id')
  parser.add_argument('--results-url', default=PERF_DASHBOARD_URL)
  return parser


def main_run(script_args):
  parser = create_argparser()
  args, sizes_args = parser.parse_known_args(script_args.args)

  runtest_args = [
      '--test-type',
      'sizes',
      '--run-python-script',
  ]
  if args.perf_id:
    runtest_args.extend([
        '--perf-id',
        args.perf_id,
        '--results-url=%s' % args.results_url,
        '--perf-dashboard-id=sizes',
        '--annotate=graphing',
    ])
  sizes_cmd = [
      os.path.join(common.SRC_DIR, 'infra', 'scripts', 'legacy', 'scripts',
                   'slave', 'chromium', 'sizes.py')
  ]
  sizes_cmd.extend(sizes_args)
  rc = common.run_runtest(script_args, runtest_args + sizes_cmd)

  json.dump({
      'valid': rc == 0,
      'failures': [],
  }, script_args.output)

  return rc
