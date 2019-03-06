#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs a python script under an isolate

This script attempts to emulate the contract of gtest-style tests
invoked via recipes. The main contract is that the caller passes the
argument:

  --isolated-script-test-output=[FILENAME]

json is written to that file in the format produced by
common.parse_common_test_results.

This script is intended to be the base command invoked by the isolate,
followed by a subsequent Python script."""

import argparse
import json
import os
import sys

import common

# Add src/testing/ into sys.path for importing xvfb and test_env.
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import xvfb

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--isolated-script-test-output', type=str,
                      required=True)
  args, _ = parser.parse_known_args()

  if sys.platform == 'win32':
    exe = os.path.join('.', 'flatbuffers_unittests.exe')
  else:
    exe = os.path.join('.', 'flatbuffers_unittests')

  env = os.environ.copy()
  with common.temporary_file() as tempfile_path:
    rc = xvfb.run_executable([exe], env, stdoutfile=tempfile_path)

    # The flatbuffer tests do not really conform to anything parsable, except
    # that they will succeed with "ALL TESTS PASSED".
    with open(tempfile_path) as f:
      failures = f.read()
      if failures == "ALL TESTS PASSED\n":
        failures = []

  with open(args.isolated_script_test_output, 'w') as fp:
    json.dump({'valid': True,'failures': failures}, fp)

  return rc


def main_compile_targets(args):
  json.dump(['flatbuffers_unittests'], args.output)


if __name__ == '__main__':
  # Conform minimally to the protocol defined by ScriptTest.
  if 'compile_targets' in sys.argv:
    funcs = {
      'run': None,
      'compile_targets': main_compile_targets,
    }
    sys.exit(common.run_script(sys.argv[1:], funcs))
  sys.exit(main())
