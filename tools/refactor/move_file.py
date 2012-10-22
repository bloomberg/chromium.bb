#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Moves a C++ file to a new location, updating any include paths that
point to it.  Updates include guards in moved header files.  Assumes
Chromium coding style.

Does not reorder headers (you can use tools/sort-headers.py), and does
not update .gypi files.

Relies on git for a fast way to find files that include the moved file.
"""


import os
import subprocess
import sys


HANDLED_EXTENSIONS = ['.cc', '.mm', '.h', '.hh']


def MoveFile(from_path, to_path):
  """Moves a file from |from_path| to |to_path|, updating its include
  guard to match the new path and updating all #includes and #imports
  of the file in other files in the same git repository, with the
  assumption that they include it using the Chromium style
  guide-standard full path from root.
  """
  extension = os.path.splitext(from_path)[1]
  if extension not in HANDLED_EXTENSIONS:
    raise Exception('Only intended to move individual source files.')

  dest_extension = os.path.splitext(to_path)[1]
  if dest_extension not in HANDLED_EXTENSIONS:
    if to_path.endswith('/') or to_path.endswith('\\'):
      to_path += os.path.basename(from_path)
    else:
      raise Exception('Destination must be either full path or end with /.')

  if not os.system('git mv %s %s' % (from_path, to_path)) == 0:
    raise Exception('Fatal: Failed to run git mv command.')

  if extension in ['.h', '.hh']:
    UpdateIncludeGuard(from_path, to_path)
    UpdateIncludes(from_path, to_path)


def MakeIncludeGuardName(path_from_root):
  """Returns an include guard name given a path from root."""
  guard = path_from_root.replace('/', '_')
  guard = guard.replace('\\', '_')
  guard = guard.replace('.', '_')
  guard += '_'
  return guard.upper()


def UpdateIncludeGuard(old_path, new_path):
  """Updates the include guard in a file now residing at |new_path|,
  previously residing at |old_path|, with an up-to-date include guard.

  Errors out if an include guard per Chromium style guide cannot be
  found for the old path.
  """
  old_guard = MakeIncludeGuardName(old_path)
  new_guard = MakeIncludeGuardName(new_path)

  with open(new_path) as f:
    contents = f.read()

  new_contents = contents.replace(old_guard, new_guard)
  if new_contents == contents:
    raise Exception(
      'Error updating include guard; perhaps old guard is not per style guide?')

  with open(new_path, 'w') as f:
    f.write(new_contents)


def UpdateIncludes(old_path, new_path):
  """Given the |old_path| and |new_path| of a file being moved, update
  #include and #import statements in all files in the same git
  repository referring to the moved file.
  """
  # Include paths always use forward slashes.
  old_path = old_path.replace('\\', '/')
  new_path = new_path.replace('\\', '/')

  out, err = subprocess.Popen(
    ['git', 'grep', '--name-only',
     r'#\(include\|import\)\s*["<]%s[>"]' % old_path],
    stdout=subprocess.PIPE).communicate()
  includees = out.splitlines()

  for includee in includees:
    with open(includee) as f:
      contents = f.read()
    new_contents = contents.replace('"%s"' % old_path, '"%s"' % new_path)
    new_contents = new_contents.replace('<%s>' % old_path, '<%s>' % new_path)
    if new_contents == contents:
      raise Exception('Error updating include in file %s' % includee)
    with open(includee, 'w') as f:
      f.write(new_contents)


def main():
  if not os.path.isdir('.git'):
    print 'Fatal: You must run from the root of a git checkout.'
    return 1
  args = sys.argv[1:]
  if len(args) != 2:
    print 'Usage: move_file.py FROM_PATH TO_PATH\n\n%s' % __doc__
    return 1
  MoveFile(args[0], args[1])
  return 0


if __name__ == '__main__':
  sys.exit(main())
