#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs isolate bundled Telemetry unittests.

This script attempts to emulate the contract of gtest-style tests
invoked via recipes. The main contract is that the caller passes the
argument:

  --isolated-script-test-output=[FILENAME]

json is written to that file in the format produced by
common.parse_common_test_results.

This script is intended to be the base command invoked by the isolate,
followed by a subsequent Python script. It could be generalized to
invoke an arbitrary executable.
"""

import argparse
import json
import os
import sys


import common


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
    '--isolated-script-test-output',
    type=argparse.FileType('w'),
    required=True)
  args, rest_args = parser.parse_known_args()
  with common.temporary_file() as tempfile_path:
    rc = common.run_command([sys.executable] + rest_args + [
      '--write-full-results-to', tempfile_path,
    ])
    with open(tempfile_path) as f:
      results = json.load(f)
    parsed_results = common.parse_common_test_results(results,
                                                      test_separator='.')
    failures = parsed_results['unexpected_failures']

    json.dump({
        'valid': bool(rc <= common.MAX_FAILURES_EXIT_STATUS and
                      ((rc == 0) or failures)),
        'failures': failures.keys(),
    }, args.isolated_script_test_output)

  return rc


# This is not really a "script test" so does not need to manually add
# any additional compile targets.
def main_compile_targets(args):
  json.dump([], args.output)


if __name__ == '__main__':
  # Conform minimally to the protocol defined by ScriptTest.
  if 'compile_targets' in sys.argv:
    funcs = {
      'run': None,
      'compile_targets': main_compile_targets,
    }
    sys.exit(common.run_script(sys.argv[1:], funcs))
  sys.exit(main())
