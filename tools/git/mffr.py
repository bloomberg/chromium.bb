#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Usage: mffr.py [-d] [-g *.h] [-g *.cc] REGEXP REPLACEMENT

This tool performs a fast find-and-replace operation on files in
the current git repository.

The -d flag selects a default set of globs (C++ and Objective-C/C++
source files). The -g flag adds a single glob to the list and may
be used multiple times. If neither -d nor -g is specified, the tool
searches all files (*.*).

REGEXP uses full Python regexp syntax. REPLACEMENT can use
back-references.
"""

import re
import subprocess
import sys


# We need to use shell=True with subprocess on Windows so that it
# finds 'git' from the path, but can lead to undesired behavior on
# Linux.
_USE_SHELL = (sys.platform == 'win32')


def MultiFileFindReplace(original, replacement, file_globs):
  """Implements fast multi-file find and replace.

  Given an |original| string and a |replacement| string, find matching
  files by running git grep on |original| in files matching any
  pattern in |file_globs|.

  Once files are found, |re.sub| is run to replace |original| with
  |replacement|.  |replacement| may use capture group back-references.

  Args:
    original: '(#(include|import)\s*["<])chrome/browser/ui/browser.h([>"])'
    replacement: '\1chrome/browser/ui/browser/browser.h\3'
    file_globs: ['*.cc', '*.h', '*.m', '*.mm']

  Returns the list of files modified.

  Raises an exception on error.
  """
  # Posix extended regular expressions do not reliably support the "\s"
  # shorthand.
  posix_ere_original = re.sub(r"\\s", "[[:space:]]", original)
  out, err = subprocess.Popen(
      ['git', 'grep', '-E', '--name-only', posix_ere_original,
       '--'] + file_globs,
      stdout=subprocess.PIPE,
      shell=_USE_SHELL).communicate()
  referees = out.splitlines()

  for referee in referees:
    with open(referee) as f:
      original_contents = f.read()
    contents = re.sub(original, replacement, original_contents)
    if contents == original_contents:
      raise Exception('No change in file %s although matched in grep' %
                      referee)
    with open(referee, 'wb') as f:
      f.write(contents)

  return referees


def main():
  file_globs = []
  force_unsafe_run = False
  args = sys.argv[1:]
  while args and args[0].startswith('-'):
    if args[0] == '-d':
      file_globs = ['*.cc', '*.h', '*.m', '*.mm']
      args = args[1:]
    elif args[0] == '-g':
      file_globs.append(args[1])
      args = args[2:]
    elif args[0] == '-f':
      force_unsafe_run = True
      args = args[1:]
  if not file_globs:
    file_globs = ['*.*']
  if not args:
    print globals()['__doc__']
    return 1
  if not force_unsafe_run:
    out, err = subprocess.Popen(['git', 'status', '--porcelain'],
                                stdout=subprocess.PIPE,
                                shell=_USE_SHELL).communicate()
    if out:
      print 'ERROR: This tool does not print any confirmation prompts,'
      print 'so you should only run it with a clean staging area and cache'
      print 'so that reverting a bad find/replace is as easy as running'
      print '  git checkout -- .'
      print ''
      print 'To override this safeguard, pass the -f flag.'
      return 1
  original = args[0]
  replacement = args[1]
  print 'File globs:  %s' % file_globs
  print 'Original:    %s' % original
  print 'Replacement: %s' % replacement
  MultiFileFindReplace(original, replacement, file_globs)
  return 0


if __name__ == '__main__':
  sys.exit(main())
