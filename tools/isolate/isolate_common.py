# coding=utf-8
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common code to manage .isolate format.
"""

import logging
import os
import posixpath
import re
import sys

import trace_inputs


PATH_VARIABLES = ('DEPTH', 'PRODUCT_DIR')

KEY_TRACKED = 'isolate_dependency_tracked'
KEY_UNTRACKED = 'isolate_dependency_untracked'

_GIT_PATH = os.path.sep + '.git'
_SVN_PATH = os.path.sep + '.svn'


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
      f.endswith(('.pyc', 'testserver.log')) or
      _GIT_PATH in f or
      _SVN_PATH in f or
      f in ('.git', '.svn'))


def classify_files(files):
  """Converts the list of files into a .isolate 'variables' dictionary.

  Arguments:
  - files: list of files names to generate a dictionary out of.
  """
  # These directories are not guaranteed to be always present on every builder.
  OPTIONAL_DIRECTORIES = (
    'test/data/plugin',
    'third_party/WebKit/LayoutTests',
  )

  tracked = []
  untracked = []
  for filepath in sorted(files):
    if (not filepath.endswith('/') and
        ' ' not in filepath and
        not any(i in filepath for i in OPTIONAL_DIRECTORIES)):
      # A file, without whitespace, in a non-optional directory.
      tracked.append(filepath)
    else:
      # Anything else.
      untracked.append(filepath)

  variables = {}
  if tracked:
    variables[KEY_TRACKED] = tracked
  if untracked:
    variables[KEY_UNTRACKED] = untracked
  return variables


def generate_simplified(files, root_dir, variables, relative_cwd):
  """Generates a clean and complete .isolate 'variables' dictionary.

  Cleans up and extracts only files from within root_dir then processes
  variables and relative_cwd.
  """
  logging.info(
      'generate_simplified(%d files, %s, %s, %s)' %
      (len(files), root_dir, variables, relative_cwd))
  # Constants.
  # Skip log in PRODUCT_DIR. Note that these are applied on '/' style path
  # separator.
  LOG_FILE = re.compile(r'^\<\(PRODUCT_DIR\)\/[^\/]+\.log$')
  EXECUTABLE = re.compile(
      r'^(\<\(PRODUCT_DIR\)\/[^\/\.]+)' +
      re.escape(variables.get('EXECUTABLE_SUFFIX', '')) +
      r'$')

  # Preparation work.
  relative_cwd = cleanup_path(relative_cwd)
  # Creates the right set of variables here. We only care about PATH_VARIABLES.
  variables = dict(
      ('<(%s)' % k, variables[k]) for k in PATH_VARIABLES if k in variables)

  # Actual work: Process the files.
  files = trace_inputs.extract_directories(root_dir, files, default_blacklist)

  def fix(f):
    """Bases the file on the most restrictive variable."""
    logging.debug('fix(%s)' % f)
    # Important, GYP stores the files with / and not \.
    f = f.replace(os.path.sep, '/')
    # If it's not already a variable.
    if not f.startswith('<'):
      # relative_cwd is usually the directory containing the gyp file. It may be
      # empty if the whole directory containing the gyp file is needed.
      f = posix_relpath(f, relative_cwd) or './'

    for variable, root_path in variables.iteritems():
      if f.startswith(root_path):
        f = variable + f[len(root_path):]
        break

    # Now strips off known files we want to ignore and to any specific mangling
    # as necessary. It's easier to do it here than generate a blacklist.
    match = EXECUTABLE.match(f)
    if match:
      return match.group(1) + '<(EXECUTABLE_SUFFIX)'
    if LOG_FILE.match(f):
      return None

    if sys.platform == 'darwin':
      # On OSX, the name of the output is dependent on gyp define, it can be
      # 'Google Chrome.app' or 'Chromium.app', same for 'XXX
      # Framework.framework'. Furthermore, they are versioned with a gyp
      # variable.  To lower the complexity of the .isolate file, remove all the
      # individual entries that show up under any of the 4 entries and replace
      # them with the directory itself. Overall, this results in a bit more
      # files than strictly necessary.
      OSX_BUNDLES = (
        '<(PRODUCT_DIR)/Chromium Framework.framework/',
        '<(PRODUCT_DIR)/Chromium.app/',
        '<(PRODUCT_DIR)/Google Chrome Framework.framework/',
        '<(PRODUCT_DIR)/Google Chrome.app/',
      )
      for prefix in OSX_BUNDLES:
        if f.startswith(prefix):
          # Note this result in duplicate values, so the a set() must be used to
          # remove duplicates.
          return prefix

    return f

  return classify_files(set(filter(None, (fix(f.path) for f in files))))


def generate_isolate(files, root_dir, variables, relative_cwd):
  """Generates a clean and complete .isolate file."""
  result = generate_simplified(files, root_dir, variables, relative_cwd)
  return {
    'conditions': [
      ['OS=="%s"' % get_flavor(), {
        'variables': result,
      }],
    ],
  }


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
