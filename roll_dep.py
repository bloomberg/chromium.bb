#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Rolls DEPS controlled dependency.

Works only with git checkout and git dependencies.  Currently this
script will always roll to the tip of to origin/master.
"""

import argparse
import os
import re
import subprocess
import sys

NEED_SHELL = sys.platform.startswith('win')


class Error(Exception):
  pass


def check_output(*args, **kwargs):
  """subprocess.check_output() passing shell=True on Windows for git."""
  kwargs.setdefault('shell', NEED_SHELL)
  return subprocess.check_output(*args, **kwargs)


def check_call(*args, **kwargs):
  """subprocess.check_call() passing shell=True on Windows for git."""
  kwargs.setdefault('shell', NEED_SHELL)
  subprocess.check_call(*args, **kwargs)


def is_pristine(root, merge_base='origin/master'):
  """Returns True if a git checkout is pristine."""
  cmd = ['git', 'diff', '--ignore-submodules', merge_base]
  return not (check_output(cmd, cwd=root).strip() or
              check_output(cmd + ['--cached'], cwd=root).strip())


def roll(root, deps_dir, key, reviewers, bug):
  deps = os.path.join(root, 'DEPS')
  try:
    with open(deps, 'rb') as f:
      deps_content = f.read()
  except (IOError, OSError):
    raise Error('Ensure the script is run in the directory '
                'containing DEPS file.')

  if not is_pristine(root):
    raise Error('Ensure %s is clean first.' % root)

  full_dir = os.path.normpath(os.path.join(os.path.dirname(root), deps_dir))
  if not os.path.isdir(full_dir):
    raise Error('Directory not found: %s' % deps_dir)
  head = check_output(['git', 'rev-parse', 'HEAD'], cwd=full_dir).strip()

  if not head in deps_content:
    print('Warning: %s is not checked out at the expected revision in DEPS' %
          deps_dir)
    if key is None:
      print("Warning: no key specified.  Using '%s'." % deps_dir)
      key = deps_dir

    # It happens if the user checked out a branch in the dependency by himself.
    # Fall back to reading the DEPS to figure out the original commit.
    for i in deps_content.splitlines():
      m = re.match(r'\s+"' + key + '": "([a-z0-9]{40})",', i)
      if m:
        head = m.group(1)
        break
    else:
      raise Error('Expected to find commit %s for %s in DEPS' % (head, key))

  print('Found old revision %s' % head)

  check_call(['git', 'fetch', 'origin'], cwd=full_dir)
  master = check_output(
      ['git', 'rev-parse', 'origin/master'], cwd=full_dir).strip()
  print('Found new revision %s' % master)

  if master == head:
    raise Error('No revision to roll!')

  commit_range = '%s..%s' % (head[:9], master[:9])

  logs = check_output(
      ['git', 'log', commit_range, '--date=short', '--format=%ad %ae %s'],
      cwd=full_dir).strip()
  logs = re.sub(r'(?m)^(\d\d\d\d-\d\d-\d\d [^@]+)@[^ ]+( .*)$', r'\1\2', logs)
  cmd = 'git log %s --date=short --format=\'%%ad %%ae %%s\'' % commit_range
  reviewer = 'R=%s\n' % ','.join(reviewers) if reviewers else ''
  bug = 'BUG=%s\n' % bug if bug else ''
  msg = (
      'Roll %s/ to %s.\n'
      '\n'
      '$ %s\n'
      '%s\n\n'
      '%s'
      '%s') % (
          deps_dir,
          master,
          cmd,
          logs,
          reviewer,
          bug)

  print('Commit message:')
  print('\n'.join('    ' + i for i in msg.splitlines()))
  deps_content = deps_content.replace(head, master)
  with open(deps, 'wb') as f:
    f.write(deps_content)
  check_call(['git', 'add', 'DEPS'], cwd=root)
  check_call(['git', 'commit', '-m', msg], cwd=root)
  print('')
  if not reviewers:
    print('You forgot to pass -r, make sure to insert a R=foo@example.com line')
    print('to the commit description before emailing.')
    print('')
  print('Run:')
  print('  git cl upload --send-mail')


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('-r', '--reviewer',
      help='To specify multiple reviewers, use comma separated list, e.g. '
           '-r joe,jane,john. Defaults to @chromium.org')
  parser.add_argument('-b', '--bug')
  parser.add_argument('dep_path', help='path to dependency')
  parser.add_argument('key', nargs='?',
      help='regexp for dependency in DEPS file')
  args = parser.parse_args()

  reviewers = None
  if args.reviewer:
    reviewers = args.reviewer.split(',')
    for i, r in enumerate(reviewers):
      if not '@' in r:
        reviewers[i] = r + '@chromium.org'

  try:
    roll(
        os.getcwd(),
        args.dep_path,
        args.key,
        reviewers=reviewers,
        bug=args.bug)

  except Error as e:
    sys.stderr.write('error: %s\n' % e)
    return 1

  return 0


if __name__ == '__main__':
  sys.exit(main())
