#!/usr/bin/python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compares two orderfiles, from filenames or a commit.

This shows some statistics about two orderfiles, possibly extracted from an
updating commit made by the orderfile bot.
"""

import argparse
import logging
import os
import subprocess
import sys


def ParseOrderfile(filename):
  """Parses an orderfile into a list of symbols.

  Args:
    filename: (str) Path to the orderfile.

  Returns:
    [str] List of symbols.
  """
  symbols = []
  lines = []
  already_seen = set()
  with open(filename, 'r') as f:
    lines = [line.strip() for line in f]

  for entry in lines:
    # Example: .text.startup.BLA
    symbol_name = entry[entry.rindex('.'):]
    if symbol_name in already_seen or symbol_name == '*' or entry == '.text':
      continue
    already_seen.add(symbol_name)
    symbols.append(symbol_name)
  return symbols


def Compare(first_filename, second_filename):
  """Outputs a comparison of two orderfiles to stdout.

  Args:
    first_filename: (str) First orderfile.
    second_filename: (str) Second orderfile.
  """
  first_symbols = ParseOrderfile(first_filename)
  second_symbols = ParseOrderfile(second_filename)
  print 'Symbols count:\n\tfirst:\t%d\n\tsecond:\t%d' % (
      len(first_symbols), len(second_symbols))
  first_symbols = set(first_symbols)
  second_symbols = set(second_symbols)
  new_symbols = second_symbols - first_symbols
  removed_symbols = first_symbols - second_symbols
  print 'New symbols = %d' % len(new_symbols)
  print 'Removed symbols = %d' % len(removed_symbols)


def CheckOrderfileCommit(commit_hash, clank_path):
  """Asserts that a commit is an orderfile update from the bot.

  Args:
    commit_hash: (str) Git hash of the orderfile roll commit.
    clank_path: (str) Path to the clank repository.
  """
  output = subprocess.check_output(
      ['git', 'show', r'--format=%an %s', commit_hash], cwd=clank_path)
  first_line = output.split('\n')[0]
  assert first_line == 'clank-autoroller Update Orderfile.', (
      'Not an orderfile commit')


def GetBeforeAfterOrderfileHashes(commit_hash, clank_path):
  """Downloads the orderfiles before and afer an orderfile roll.

  Args:
    commit_hash: (str) Git hash of the orderfile roll commit.
    clank_path: (str) Path to the clank repository.

  Returns:
    (str, str) Path to the before and after commit orderfiles.
  """
  orderfile_hash_relative_path = 'orderfiles/orderfile.arm.out.sha1'
  before_output = subprocess.check_output(
      ['git', 'show', '%s^:%s' % (commit_hash, orderfile_hash_relative_path)],
      cwd=clank_path)
  before_hash = before_output.split('\n')[0]
  after_output = subprocess.check_output(
      ['git', 'show', '%s:%s' % (commit_hash, orderfile_hash_relative_path)],
      cwd=clank_path)
  after_hash = after_output.split('\n')[0]
  assert before_hash != after_hash
  return (before_hash, after_hash)


def DownloadOrderfile(orderfile_hash, output_filename):
  """Downloads an orderfile with a given hash to a given destination."""
  cloud_storage_path = (
      'gs://clank-archive/orderfile-clankium/%s' % orderfile_hash)
  subprocess.check_call(
      ['gsutil.py', 'cp', cloud_storage_path, output_filename])


def GetOrderfilesFromCommit(commit_hash):
  """Returns paths to the before and after orderfiles for a commit."""
  clank_path = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
                            'clank')
  logging.info('Checking that the commit is an orderfile')
  CheckOrderfileCommit(commit_hash, clank_path)
  (before_hash, after_hash) = GetBeforeAfterOrderfileHashes(
      commit_hash, clank_path)
  logging.info('Before / after hashes: %s %s', before_hash, after_hash)
  before_filename = os.path.join('/tmp/', before_hash)
  after_filename = os.path.join('/tmp/', after_hash)
  logging.info('Downloading files')
  DownloadOrderfile(before_hash, before_filename)
  DownloadOrderfile(after_hash, after_filename)
  return (before_filename, after_filename)


def CreateArgumentParser():
  """Returns the argumeng parser."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--first', help='First orderfile')
  parser.add_argument('--second', help='Second orderfile')
  parser.add_argument('--from-commit', help='Analyze the difference in the '
                      'orderfile from an orderfile bot commit.')
  return parser


def main():
  logging.basicConfig(level=logging.INFO)
  parser = CreateArgumentParser()
  args = parser.parse_args()
  if args.first or args.second:
    assert args.first and args.second, 'Need both files.'
    Compare(args.first, args.second)
  elif args.from_commit:
    first, second = GetOrderfilesFromCommit(args.from_commit)
    try:
      logging.info('Comparing the orderfiles')
      Compare(first, second)
    finally:
      os.remove(first)
      os.remove(second)
  else:
    return False
  return True


if __name__ == '__main__':
  sys.exit(0 if main() else 1)
