#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
r"""Automatically fetch, build, and run clang-tidy from source.

This script seeks to automate the steps detailed in docs/clang_tidy.md.

Example: the following command disables clang-tidy's default checks (-*) and
enables the clang static analyzer checks.

    tools/clang/scripts/clang_tidy_tool.py \\
       --checks='-*,clang-analyzer-*,-clang-analyzer-alpha*' \\
       --header-filter='.*' \\
       out/Release chrome

The same, but checks the changes only.

    git diff -U5 | tools/clang/scripts/clang_tidy_tool.py \\
       --diff \\
       --checks='-*,clang-analyzer-*,-clang-analyzer-alpha*' \\
       --header-filter='.*' \\
       out/Release chrome
"""

import argparse
import os
import subprocess
import sys


def GetCheckoutDir(out_dir):
  """Returns absolute path to the checked-out llvm repo."""
  return os.path.join(out_dir, 'tools', 'clang', 'third_party', 'llvm')


def GetBuildDir(out_dir):
  return os.path.join(GetCheckoutDir(out_dir), 'build')


def FetchClang(out_dir):
  """Clone llvm repo into |out_dir| or update if it already exists."""
  checkout_dir = GetCheckoutDir(out_dir)

  try:
    # Create parent directories of the checkout directory
    os.makedirs(os.path.dirname(checkout_dir))
  except OSError:
    pass

  try:
    # First, try to clone the repo.
    args = [
        'git',
        'clone',
        'https://github.com/llvm/llvm-project.git',
        checkout_dir,
    ]
    subprocess.check_call(args, shell=sys.platform == 'win32')
  except subprocess.CalledProcessError:
    # Otherwise, try to update it.
    print('-- Attempting to update existing repo')
    args = ['git', 'pull', '--rebase', 'origin', 'master']
    subprocess.check_call(args, cwd=checkout_dir)


def BuildClang(out_dir):
  """Build clang from llvm repo at |GetCheckoutDir(out_dir)|."""
  # Make <checkout>/build directory
  build_dir = GetBuildDir(out_dir)
  try:
    os.mkdir(build_dir)
  except OSError as e:
    # Ignore errno 17 'File Exists'
    if e.errno != 17:
      raise e

  # From that dir, run cmake
  cmake_args = [
      'cmake',
      '-GNinja',
      '-DLLVM_ENABLE_PROJECTS=clang;clang-tools-extra',
      '-DCMAKE_BUILD_TYPE=Release',
      '../llvm',
  ]
  subprocess.check_call(cmake_args, cwd=build_dir)

  ninja_args = [
      'ninja',
      'clang-tidy',
      'clang-apply-replacements',
  ]
  subprocess.check_call(ninja_args, cwd=build_dir)


def BuildNinjaTarget(out_dir, ninja_target):
  args = ['ninja', '-C', out_dir, ninja_target]
  subprocess.check_call(args)


def GenerateCompDb(out_dir):
  gen_compdb_script = os.path.join(
      os.path.dirname(__file__), 'generate_compdb.py')
  comp_db_file = os.path.join(out_dir, 'compile_commands.json')
  args = [gen_compdb_script, '-p', out_dir, '-o', comp_db_file]
  subprocess.check_call(args)


def RunClangTidy(checks, header_filter, auto_fix, out_dir, ninja_target):
  """Invoke the |run-clang-tidy.py| script."""
  run_clang_tidy_script = os.path.join(
      GetCheckoutDir(out_dir), 'clang-tools-extra', 'clang-tidy', 'tool',
      'run-clang-tidy.py')

  clang_tidy_binary = os.path.join(GetBuildDir(out_dir), 'bin', 'clang-tidy')
  clang_apply_rep_binary = os.path.join(
      GetBuildDir(out_dir), 'bin', 'clang-apply-replacements')

  args = [
      run_clang_tidy_script,
      '-quiet',
      '-p',
      out_dir,
      '-clang-tidy-binary',
      clang_tidy_binary,
      '-clang-apply-replacements-binary',
      clang_apply_rep_binary,
  ]

  if checks:
    args.append('-checks={}'.format(checks))

  if header_filter:
    args.append('-header-filter={}'.format(header_filter))

  if auto_fix:
    args.append('-fix')

  args.append(ninja_target)
  subprocess.check_call(args)


def RunClangTidyDiff(checks, header_filter, auto_fix, out_dir):
  """Invoke the |clang-tidy-diff.py| script over the diff from stdin."""
  clang_tidy_diff_script = os.path.join(
      GetCheckoutDir(out_dir), 'clang-tools-extra', 'clang-tidy', 'tool',
      'clang-tidy-diff.py')

  clang_tidy_binary = os.path.join(GetBuildDir(out_dir), 'bin', 'clang-tidy')

  args = [
      clang_tidy_diff_script,
      '-quiet',
      '-p1',
      '-path',
      out_dir,
      '-clang-tidy-binary',
      clang_tidy_binary,
  ]

  if checks:
    args.append('-checks={}'.format(checks))

  if header_filter:
    args.append('-header-filter={}'.format(header_filter))

  if auto_fix:
    args.append('-fix')

  subprocess.check_call(args)


def main():
  script_name = sys.argv[0]

  parser = argparse.ArgumentParser(
      formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
  parser.add_argument(
      '--fetch', action='store_true', help='Fetch and build clang sources')
  parser.add_argument(
      '--build',
      action='store_true',
      help='build clang sources to get clang-tidy')
  parser.add_argument(
      '--diff',
      action='store_true',
      default=False,
      help ='read diff from the stdin and check it')
  parser.add_argument('--checks', help='passed to clang-tidy')
  parser.add_argument('--header-filter', help='passed to clang-tidy')
  parser.add_argument(
      '--auto-fix',
      action='store_true',
      help='tell clang-tidy to auto-fix errors')
  parser.add_argument('OUT_DIR', help='where we are building Chrome')
  parser.add_argument('NINJA_TARGET', help='ninja target')
  args = parser.parse_args()

  steps = []

  if args.fetch:
    steps.append(('Fetching clang sources', lambda: FetchClang(args.OUT_DIR)))

  if args.build:
    steps.append(('Building clang', lambda: BuildClang(args.OUT_DIR)))

  steps += [
      ('Building ninja target: %s' % args.NINJA_TARGET,
      lambda: BuildNinjaTarget(args.OUT_DIR, args.NINJA_TARGET)),
      ('Generating compilation DB', lambda: GenerateCompDb(args.OUT_DIR))
    ]
  if args.diff:
    steps += [
        ('Running clang-tidy on diff',
         lambda: RunClangTidyDiff(args.checks, args.header_filter,
                                  args.auto_fix,
                                  args.OUT_DIR, args.NINJA_TARGET)),
    ]
  else:
    steps += [
        ('Running clang-tidy',
        lambda: RunClangTidy(args.checks, args.header_filter,
                             args.auto_fix,
                             args.OUT_DIR, args.NINJA_TARGET)),
    ]

  # Run the steps in sequence.
  for i, (msg, step_func) in enumerate(steps):
    # Print progress message
    print '-- %s %s' % (script_name, '-' * (80 - len(script_name) - 4))
    print '-- [%d/%d] %s' % (i + 1, len(steps), msg)
    print 80 * '-'

    step_func()

  return 0


if __name__ == '__main__':
  sys.exit(main())
