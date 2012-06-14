# coding=utf-8
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common code to manage .isolate format.
"""

import logging
import os
import posixpath
import sys


KEY_TRACKED = 'isolate_dependency_tracked'
KEY_UNTRACKED = 'isolate_dependency_untracked'

_GIT_PATH = os.path.sep + '.git' + os.path.sep
_SVN_PATH = os.path.sep + '.svn' + os.path.sep


def posix_relpath(path, root):
  """posix.relpath() that keeps trailing slash."""
  out = posixpath.relpath(path, root)
  if path.endswith('/'):
    out += '/'
  return out


def cleanup_path(x):
  """Cleans up a relative path. Converts any os.path.sep to '/' on Windows."""
  if x:
    x = x.rstrip(os.path.sep).replace(os.path.sep, '/')
  if x == '.':
    x = ''
  if x:
    x += '/'
  return x


def get_flavor():
  """Returns the system default flavor. Copied from gyp/pylib/gyp/common.py."""
  flavors = {
    'cygwin': 'win',
    'win32': 'win',
    'darwin': 'mac',
    'sunos5': 'solaris',
    'freebsd7': 'freebsd',
    'freebsd8': 'freebsd',
  }
  return flavors.get(sys.platform, 'linux')


def default_blacklist(f):
  """Filters unimportant files normally ignored."""
  return (
      f.endswith('.pyc') or
      _GIT_PATH in f or
      _SVN_PATH in f)


def generate_dict(files, cwd_dir, product_dir):
  """Converts the list of files into a .isolate dictionary.

  Arguments:
  - files: list of files to generate a dictionary out of.
  - cwd_dir: directory to base all the files from, relative to root_dir.
  - product_dir: directory to replace with <(PRODUCT_DIR), relative to root_dir.
  """
  cwd_dir = cleanup_path(cwd_dir)
  product_dir = cleanup_path(product_dir)

  def fix(f):
    """Bases the file on the most restrictive variable."""
    logging.debug('fix(%s)' % f)
    # Important, GYP stores the files with / and not \.
    f = f.replace(os.path.sep, '/')
    if product_dir and f.startswith(product_dir):
      return '<(PRODUCT_DIR)/%s' % f[len(product_dir):]
    else:
      # cwd_dir is usually the directory containing the gyp file. It may be
      # empty if the whole directory containing the gyp file is needed.
      return posix_relpath(f, cwd_dir) or './'

  corrected = [fix(f) for f in files]
  tracked = [f for f in corrected if not f.endswith('/') and ' ' not in f]
  untracked = [f for f in corrected if f.endswith('/') or ' ' in f]
  variables = {}
  if tracked:
    variables[KEY_TRACKED] = tracked
  if untracked:
    variables[KEY_UNTRACKED] = untracked
  return variables


def pretty_print(variables, stdout):
  """Outputs a gyp compatible list from the decoded variables.

  Similar to pprint.print() but with NIH syndrome.
  """
  # Order the dictionary keys by these keys in priority.
  ORDER = (
      'variables', 'condition', 'command', 'relative_cwd', 'read_only',
      KEY_TRACKED, KEY_UNTRACKED)

  def sorting_key(x):
    """Gives priority to 'most important' keys before the others."""
    if x in ORDER:
      return str(ORDER.index(x))
    return x

  def loop_list(indent, items):
    for item in items:
      if isinstance(item, basestring):
        stdout.write('%s\'%s\',\n' % (indent, item))
      elif isinstance(item, dict):
        stdout.write('%s{\n' % indent)
        loop_dict(indent + '  ', item)
        stdout.write('%s},\n' % indent)
      elif isinstance(item, list):
        # A list inside a list will write the first item embedded.
        stdout.write('%s[' % indent)
        for index, i in enumerate(item):
          if isinstance(i, basestring):
            stdout.write(
                '\'%s\', ' % i.replace('\\', '\\\\').replace('\'', '\\\''))
          elif isinstance(i, dict):
            stdout.write('{\n')
            loop_dict(indent + '  ', i)
            if index != len(item) - 1:
              x = ', '
            else:
              x = ''
            stdout.write('%s}%s' % (indent, x))
          else:
            assert False
        stdout.write('],\n')
      else:
        assert False

  def loop_dict(indent, items):
    for key in sorted(items, key=sorting_key):
      item = items[key]
      stdout.write("%s'%s': " % (indent, key))
      if isinstance(item, dict):
        stdout.write('{\n')
        loop_dict(indent + '  ', item)
        stdout.write(indent + '},\n')
      elif isinstance(item, list):
        stdout.write('[\n')
        loop_list(indent + '  ', item)
        stdout.write(indent + '],\n')
      elif isinstance(item, basestring):
        stdout.write(
            '\'%s\',\n' % item.replace('\\', '\\\\').replace('\'', '\\\''))
      elif item in (True, False, None):
        stdout.write('%s\n' % item)
      else:
        assert False, item

  stdout.write('{\n')
  loop_dict('  ', variables)
  stdout.write('}\n')
