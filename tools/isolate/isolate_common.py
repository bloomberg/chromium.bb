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

# Files that should be 0-length when mapped.
KEY_TOUCHED = 'isolate_dependency_touched'
# Files that should be tracked by the build tool.
KEY_TRACKED = 'isolate_dependency_tracked'
# Files that should not be tracked by the build tool.
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


def classify_files(root_dir, tracked, untracked):
  """Converts the list of files into a .isolate 'variables' dictionary.

  Arguments:
  - tracked: list of files names to generate a dictionary out of that should
             probably be tracked.
  - untracked: list of files names that must not be tracked.
  """
  # These directories are not guaranteed to be always present on every builder.
  OPTIONAL_DIRECTORIES = (
    'test/data/plugin',
    'third_party/WebKit/LayoutTests',
  )

  new_tracked = []
  new_untracked = list(untracked)

  def should_be_tracked(filepath):
    """Returns True if it is a file without whitespace in a non-optional
    directory that has no symlink in its path.
    """
    if filepath.endswith('/'):
      return False
    if ' ' in filepath:
      return False
    if any(i in filepath for i in OPTIONAL_DIRECTORIES):
      return False
    # Look if any element in the path is a symlink.
    split = filepath.split('/')
    for i in range(len(split)):
      if os.path.islink(os.path.join(root_dir, '/'.join(split[:i+1]))):
        return False
    return True

  for filepath in sorted(tracked):
    if should_be_tracked(filepath):
      new_tracked.append(filepath)
    else:
      # Anything else.
      new_untracked.append(filepath)

  variables = {}
  if new_tracked:
    variables[KEY_TRACKED] = sorted(new_tracked)
  if new_untracked:
    variables[KEY_UNTRACKED] = sorted(new_untracked)
  return variables


def generate_simplified(
    tracked, untracked, touched, root_dir, variables, relative_cwd):
  """Generates a clean and complete .isolate 'variables' dictionary.

  Cleans up and extracts only files from within root_dir then processes
  variables and relative_cwd.
  """
  logging.info(
      'generate_simplified(%d files, %s, %s, %s)' %
      (len(tracked) + len(untracked) + len(touched),
        root_dir, variables, relative_cwd))
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
      ('<(%s)' % k, variables[k].replace(os.path.sep, '/'))
      for k in PATH_VARIABLES if k in variables)

  # Actual work: Process the files.
  # TODO(maruel): if all the files in a directory are in part tracked and in
  # part untracked, the directory will not be extracted. Tracked files should be
  # 'promoted' to be untracked as needed.
  tracked = trace_inputs.extract_directories(
      root_dir, tracked, default_blacklist)
  untracked = trace_inputs.extract_directories(
      root_dir, untracked, default_blacklist)
  # touched is not compressed, otherwise it would result in files to be archived
  # that we don't need.

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

  tracked = set(filter(None, (fix(f.path) for f in tracked)))
  untracked = set(filter(None, (fix(f.path) for f in untracked)))
  touched = set(filter(None, (fix(f.path) for f in touched)))
  out = classify_files(root_dir, tracked, untracked)
  if touched:
    out[KEY_TOUCHED] = sorted(touched)
  return out


def generate_isolate(
    tracked, untracked, touched, root_dir, variables, relative_cwd):
  """Generates a clean and complete .isolate file."""
  result = generate_simplified(
      tracked, untracked, touched, root_dir, variables, relative_cwd)
  return {
    'conditions': [
      ['OS=="%s"' % get_flavor(), {
        'variables': result,
      }],
    ],
  }


def split_touched(files):
  """Splits files that are touched vs files that are read."""
  tracked = []
  touched = []
  for f in files:
    if f.size:
      tracked.append(f)
    else:
      touched.append(f)
  return tracked, touched


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
