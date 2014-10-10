#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import contextlib
import json
import os
import subprocess
import sys
import tempfile


SCRIPT_DIR = os.path.abspath(os.path.dirname(__file__))
SRC_DIR = os.path.abspath(
    os.path.join(SCRIPT_DIR, os.path.pardir, os.path.pardir))


def run_command(argv):
  print 'Running %r' % argv
  rc = subprocess.call(argv)
  print 'Command %r returned exit code %d' % (argv, rc)
  return rc


@contextlib.contextmanager
def temporary_file():
  fd, path = tempfile.mkstemp()
  os.close(fd)
  try:
    yield path
  finally:
    os.remove(path)


def mode_run(args):
  with temporary_file() as tempfile_path:
    rc = run_command([
        os.path.join(SRC_DIR, 'buildtools', 'checkdeps', 'checkdeps.py'),
        '--json', tempfile_path
    ])

    with open(tempfile_path) as f:
      checkdeps_results = json.load(f)

  result_set = set()
  for result in checkdeps_results:
    for violation in result['violations']:
      result_set.add((result['dependee_path'], violation['include_path']))

  with open(args.output, 'w') as f:
    json.dump({
        'valid': True,
        'failures': ['%s: %s' % (r[0], r[1]) for r in result_set],
    }, f)

  return rc


def main(argv):
  parser = argparse.ArgumentParser()

  subparsers = parser.add_subparsers()

  run_parser = subparsers.add_parser('run')
  run_parser.add_argument('--output', required=True)
  run_parser.set_defaults(func=mode_run)

  args = parser.parse_args(argv)
  return args.func(args)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
