#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Unit tests for extractor.py.
"""

from __future__ import print_function

import argparse
import glob
import os
import unittest
import re
import subprocess


def run_extractor(file, *extra_args):
  script_path = os.path.join('..', 'extractor.py')
  cmd_line = ("python", script_path, '--no-filter', file) + extra_args
  return subprocess.Popen(
      cmd_line, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def get_expected_files(source_file):
  stdout_file = re.sub(r'\.cc$', '-stdout.txt', source_file)
  stderr_file = re.sub(r'\.cc$', '-stderr.txt', source_file)
  return (stdout_file, stderr_file)


def dos2unix(str):
  """Convers CRLF to LF."""
  return str.replace('\r\n', '\n')


def remove_tracebacks(str):
  """Removes python tracebacks from the string."""
  regex = re.compile(
      r'''
       # A line that says "Traceback (...):"
       ^Traceback[^\n]*:\n
       # Followed by lines that begin with whitespace.
       ((\s.*)?\n)*''',
      re.MULTILINE | re.VERBOSE)
  return re.sub(regex, '', str)


class ExtractorTest(unittest.TestCase):
  def testExtractor(self):
    for source_file in glob.glob('*.cc'):
      print("Running test on %s..." % source_file)
      (stdout_file, stderr_file) = get_expected_files(source_file)
      with open(stdout_file) as f:
        expected_stdout = dos2unix(f.read())
      with open(stderr_file) as f:
        expected_stderr = dos2unix(f.read())

      proc = run_extractor(source_file)
      (stdout, stderr) = map(dos2unix, proc.communicate())

      self.assertEqual(expected_stderr, remove_tracebacks(stderr))
      expected_returncode = 2 if expected_stderr else 0
      self.assertEqual(expected_returncode, proc.returncode)
      self.assertEqual(expected_stdout, stdout)


def generate_expected_files():
  for source_file in glob.glob('*.cc'):
    proc = run_extractor(source_file)
    (stdout, stderr) = proc.communicate()

    (stdout_file, stderr_file) = get_expected_files(source_file)
    with open(stdout_file, "w") as f:
      f.write(stdout)
    with open(stderr_file, "w") as f:
      f.write(remove_tracebacks(stderr))


if __name__ == '__main__':
  parser = argparse.ArgumentParser(
      description="Network Traffic Annotation Extractor Unit Tests")
  parser.add_argument(
      '--generate-expected-files', action='store_true',
      help='Generate "-stdout.txt" and "-stderr.txt" for file in test_data')
  args = parser.parse_args()

  # Set directory for both test and gen command to the test_data folder.
  os.chdir(os.path.join(os.path.dirname(__file__), 'test_data'))

  # Run the extractor script with --generate-compdb to ensure the
  # compile_commands.json file exists in the default output directory.
  proc = run_extractor(os.devnull, '--generate-compdb')
  proc.communicate()  # Wait until extractor finishes running.

  if args.generate_expected_files:
    generate_expected_files()
  else:
    unittest.main()
