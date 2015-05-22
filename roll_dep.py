#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Rolls DEPS controlled dependency.

Works only with git checkout and git dependencies.
"""

import optparse
import os
import re
import subprocess
import sys


def is_pristine(root, merge_base='origin/master'):
  """Returns True if a git checkout is pristine."""
  cmd = ['git', 'diff', '--ignore-submodules', merge_base]
  return not (
      subprocess.check_output(cmd, cwd=root).strip() or
      subprocess.check_output(cmd + ['--cached'], cwd=root).strip())


def roll(root, deps_dir, key, reviewers, bug):
  deps = os.path.join(root, 'DEPS')
  try:
    with open(deps, 'rb') as f:
      deps_content = f.read()
  except (IOError, OSError):
    print >> sys.stderr, (
        'Ensure the script is run in the directory containing DEPS file.')
    return 1

  if not is_pristine(root):
    print >> sys.stderr, 'Ensure %s is clean first.' % root
    return 1

  full_dir = os.path.join(os.path.dirname(root), deps_dir)
  head = subprocess.check_output(
      ['git', 'rev-parse', 'HEAD'], cwd=full_dir).strip()

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
  subprocess.check_call(['git', 'add', 'DEPS'], cwd=root)
  subprocess.check_call(['git', 'commit', '-m', msg], cwd=root)
  print('')
  if not reviewers:
    print('You forgot to pass -r, make sure to insert a R=foo@example.com line')
    print('to the commit description before emailing.')
    print('')
  print('Run:')
  print('  git cl upload --send-mail')
  return 0


def main():
  parser = optparse.OptionParser(
      description=sys.modules[__name__].__doc__,
      usage='roll-dep [flags] <dependency path> <variable>')
  parser.add_option(
      '-r', '--reviewer', default='',
      help='To specify multiple reviewers, use comma separated list, e.g. '
           '-r joe,jack,john. Defaults to @chromium.org')
  parser.add_option('-b', '--bug', default='')
  options, args = parser.parse_args()
  if not len(args) or len(args) > 2:
    parser.error('Expect one or two arguments' % args)

  reviewers = None
  if options.reviewer:
    reviewers = options.reviewer.split(',')
    for i, r in enumerate(reviewers):
      if not '@' in r:
        reviewers[i] = r + '@chromium.org'

  return roll(
      os.getcwd(),
      args[0],
      args[1] if len(args) > 1 else None,
      reviewers,
      options.bug)


if __name__ == '__main__':
  sys.exit(main())
