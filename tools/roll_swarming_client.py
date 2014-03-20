#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Rolls swarming_client.

While it is currently hard coded for swarming_client/, it is potentially
modifiable to allow different dependencies. Works only with git checkout and git
dependencies.
"""

import optparse
import os
import re
import subprocess
import sys

SRC_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def is_pristine(root, merge_base='origin/master'):
  """Returns True is a git checkout is pristine."""
  cmd = ['git', 'diff', '--ignore-submodules', merge_base]
  return not (
      subprocess.check_output(cmd, cwd=root).strip() or
      subprocess.check_output(cmd + ['--cached'], cwd=root).strip())


def roll(deps_dir, key, reviewer, bug):
  if not is_pristine(SRC_ROOT):
    print >> sys.stderr, 'Ensure %s is clean first.' % SRC_ROOT
    return 1

  full_dir = os.path.join(SRC_ROOT, deps_dir)
  head = subprocess.check_output(
      ['git', 'rev-parse', 'HEAD'], cwd=full_dir).strip()
  deps = os.path.join(SRC_ROOT, 'DEPS')
  with open(deps, 'rb') as f:
    deps_content = f.read()

  if not head in deps_content:
    print('Warning: %s is not checked out at the expected revision in DEPS' %
          deps_dir)
    # It happens if the user checked out a branch in the dependency by himself.
    # Fall back to reading the DEPS to figure out the original commit.
    for i in deps_content.splitlines():
      m = re.match(r'\s+"' + key + '": "([a-z0-9]{40})",', i)
      if m:
        head = m.group(1)
        break
    else:
      print >> sys.stderr, 'Expected to find commit %s for %s in DEPS' % (
          head, key)
      return 1

  print('Found old revision %s' % head)

  subprocess.check_call(['git', 'fetch', 'origin'], cwd=full_dir)
  master = subprocess.check_output(
      ['git', 'rev-parse', 'origin/master'], cwd=full_dir).strip()
  print('Found new revision %s' % master)

  if master == head:
    print('No revision to roll!')
    return 1

  commit_range = '%s..%s' % (head[:9], master[:9])
  logs = subprocess.check_output(
      ['git', 'log', commit_range, '--date=short', '--format=%ad %ae %s'],
      cwd=full_dir).strip()
  logs = logs.replace('@chromium.org', '')
  cmd = (
      'git log %s --date=short --format=\'%%ad %%ae %%s\' | '
      'sed \'s/@chromium\.org//\'') % commit_range

  msg = (
      'Roll %s/ to %s.\n'
      '\n'
      '$ %s\n'
      '%s\n\n'
      'R=%s\n'
      'BUG=%s') % (
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
  subprocess.check_call(['git', 'add', 'DEPS'], cwd=SRC_ROOT)
  subprocess.check_call(['git', 'commit', '-m', msg], cwd=SRC_ROOT)
  print('Run:')
  print('  git cl upl --send-mail')
  return 0


def main():
  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  parser.add_option(
      '-r', '--reviewer', default='',
      help='To specify multiple reviewers, use comma separated list, e.g. '
           '-r joe,jack,john. Defaults to @chromium.org')
  parser.add_option('-b', '--bug', default='')
  options, args = parser.parse_args()
  if args:
    parser.error('Unknown argument %s' % args)
  if not options.reviewer:
    parser.error('Pass a reviewer right away with -r/--reviewer')

  reviewers = options.reviewer.split(',')
  for i, r in enumerate(reviewers):
    if not '@' in r:
      reviewers[i] = r + '@chromium.org'

  return roll(
      'tools/swarming_client',
      'swarming_revision',
      ','.join(reviewers),
      options.bug)


if __name__ == '__main__':
  sys.exit(main())
