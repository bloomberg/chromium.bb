#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Front end tool to manage .isolate files and corresponding tests.

Run ./isolate.py --help for more detailed information.

See more information at
http://dev.chromium.org/developers/testing/isolated-testing
"""

import copy
import hashlib
import logging
import optparse
import os
import posixpath
import re
import stat
import subprocess
import sys

import trace_inputs
import run_test_from_archive
from run_test_from_archive import get_flavor

# Used by process_input().
NO_INFO, STATS_ONLY, WITH_HASH = range(56, 59)
SHA_1_NULL = hashlib.sha1().hexdigest()

PATH_VARIABLES = ('DEPTH', 'PRODUCT_DIR')
DEFAULT_OSES = ('linux', 'mac', 'win')

# Files that should be 0-length when mapped.
KEY_TOUCHED = 'isolate_dependency_touched'
# Files that should be tracked by the build tool.
KEY_TRACKED = 'isolate_dependency_tracked'
# Files that should not be tracked by the build tool.
KEY_UNTRACKED = 'isolate_dependency_untracked'

_GIT_PATH = os.path.sep + '.git'
_SVN_PATH = os.path.sep + '.svn'


class ExecutionError(Exception):
  """A generic error occurred."""
  def __str__(self):
    return self.args[0]


### Path handling code.


def relpath(path, root):
  """os.path.relpath() that keeps trailing os.path.sep."""
  out = os.path.relpath(path, root)
  if path.endswith(os.path.sep):
    out += os.path.sep
  return out


def normpath(path):
  """os.path.normpath() that keeps trailing os.path.sep."""
  out = os.path.normpath(path)
  if path.endswith(os.path.sep):
    out += os.path.sep
  return out


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


def default_blacklist(f):
  """Filters unimportant files normally ignored."""
  return (
      f.endswith(('.pyc', '.run_test_cases', 'testserver.log')) or
      _GIT_PATH in f or
      _SVN_PATH in f or
      f in ('.git', '.svn'))


def expand_directory_and_symlink(indir, relfile, blacklist):
  """Expands a single input. It can result in multiple outputs.

  This function is recursive when relfile is a directory or a symlink.

  Note: this code doesn't properly handle recursive symlink like one created
  with:
    ln -s .. foo
  """
  if os.path.isabs(relfile):
    raise run_test_from_archive.MappingError(
        'Can\'t map absolute path %s' % relfile)

  infile = normpath(os.path.join(indir, relfile))
  if not infile.startswith(indir):
    raise run_test_from_archive.MappingError(
        'Can\'t map file %s outside %s' % (infile, indir))

  if sys.platform != 'win32':
    # Look if any item in relfile is a symlink.
    base, symlink, rest = trace_inputs.split_at_symlink(indir, relfile)
    if symlink:
      # Append everything pointed by the symlink. If the symlink is recursive,
      # this code blows up.
      symlink_relfile = os.path.join(base, symlink)
      symlink_path = os.path.join(indir, symlink_relfile)
      pointed = os.readlink(symlink_path)
      dest_infile = normpath(
          os.path.join(os.path.dirname(symlink_path), pointed))
      if rest:
        dest_infile = trace_inputs.safe_join(dest_infile, rest)
      if not dest_infile.startswith(indir):
        raise run_test_from_archive.MappingError(
            'Can\'t map symlink reference %s (from %s) ->%s outside of %s' %
            (symlink_relfile, relfile, dest_infile, indir))
      if infile.startswith(dest_infile):
        raise run_test_from_archive.MappingError(
            'Can\'t map recursive symlink reference %s->%s' %
            (symlink_relfile, dest_infile))
      dest_relfile = dest_infile[len(indir)+1:]
      logging.info('Found symlink: %s -> %s' % (symlink_relfile, dest_relfile))
      out = expand_directory_and_symlink(indir, dest_relfile, blacklist)
      # Add the symlink itself.
      out.append(symlink_relfile)
      return out

  if relfile.endswith(os.path.sep):
    if not os.path.isdir(infile):
      raise run_test_from_archive.MappingError(
          '%s is not a directory but ends with "%s"' % (infile, os.path.sep))

    outfiles = []
    for filename in os.listdir(infile):
      inner_relfile = os.path.join(relfile, filename)
      if blacklist(inner_relfile):
        continue
      if os.path.isdir(os.path.join(indir, inner_relfile)):
        inner_relfile += os.path.sep
      outfiles.extend(
          expand_directory_and_symlink(indir, inner_relfile, blacklist))
    return outfiles
  else:
    # Always add individual files even if they were blacklisted.
    if os.path.isdir(infile):
      raise run_test_from_archive.MappingError(
          'Input directory %s must have a trailing slash' % infile)

    if not os.path.isfile(infile):
      raise run_test_from_archive.MappingError(
          'Input file %s doesn\'t exist' % infile)

    return [relfile]


def expand_directories_and_symlinks(indir, infiles, blacklist):
  """Expands the directories and the symlinks, applies the blacklist and
  verifies files exist.

  Files are specified in os native path separatro.
  """
  outfiles = []
  for relfile in infiles:
    outfiles.extend(expand_directory_and_symlink(indir, relfile, blacklist))
  return outfiles


def recreate_tree(outdir, indir, infiles, action, as_sha1):
  """Creates a new tree with only the input files in it.

  Arguments:
    outdir:    Output directory to create the files in.
    indir:     Root directory the infiles are based in.
    infiles:   dict of files to map from |indir| to |outdir|.
    action:    See assert below.
    as_sha1:   Output filename is the sha1 instead of relfile.
  """
  logging.info(
      'recreate_tree(outdir=%s, indir=%s, files=%d, action=%s, as_sha1=%s)' %
      (outdir, indir, len(infiles), action, as_sha1))

  assert action in (
      run_test_from_archive.HARDLINK,
      run_test_from_archive.SYMLINK,
      run_test_from_archive.COPY)
  outdir = os.path.normpath(outdir)
  if not os.path.isdir(outdir):
    logging.info ('Creating %s' % outdir)
    os.makedirs(outdir)
  # Do not call abspath until the directory exists.
  outdir = os.path.abspath(outdir)

  for relfile, metadata in infiles.iteritems():
    infile = os.path.join(indir, relfile)
    if as_sha1:
      # Do the hashtable specific checks.
      if 'link' in metadata:
        # Skip links when storing a hashtable.
        continue
      outfile = os.path.join(outdir, metadata['sha-1'])
      if os.path.isfile(outfile):
        # Just do a quick check that the file size matches. No need to stat()
        # again the input file, grab the value from the dict.
        if metadata['size'] == os.stat(outfile).st_size:
          continue
        else:
          logging.warn('Overwritting %s' % metadata['sha-1'])
          os.remove(outfile)
    else:
      outfile = os.path.join(outdir, relfile)
      outsubdir = os.path.dirname(outfile)
      if not os.path.isdir(outsubdir):
        os.makedirs(outsubdir)

    if metadata.get('touched_only') == True:
      open(outfile, 'ab').close()
    elif 'link' in metadata:
      pointed = metadata['link']
      logging.debug('Symlink: %s -> %s' % (outfile, pointed))
      os.symlink(pointed, outfile)
    else:
      run_test_from_archive.link_file(outfile, infile, action)


def process_input(filepath, prevdict, level, read_only):
  """Processes an input file, a dependency, and return meta data about it.

  Arguments:
  - filepath: File to act on.
  - prevdict: the previous dictionary. It is used to retrieve the cached sha-1
              to skip recalculating the hash.
  - level: determines the amount of information retrieved.
  - read_only: If True, the file mode is manipulated. In practice, only save
               one of 4 modes: 0755 (rwx), 0644 (rw), 0555 (rx), 0444 (r). On
               windows, mode is not set since all files are 'executable' by
               default.

  Behaviors:
  - NO_INFO retrieves no information.
  - STATS_ONLY retrieves the file mode, file size, file timestamp, file link
    destination if it is a file link.
  - WITH_HASH retrieves all of STATS_ONLY plus the sha-1 of the content of the
    file.
  """
  assert level in (NO_INFO, STATS_ONLY, WITH_HASH)
  out = {}
  if prevdict.get('touched_only') == True:
    # The file's content is ignored. Skip the time and hard code mode.
    if get_flavor() != 'win':
      out['mode'] = stat.S_IRUSR | stat.S_IRGRP
    out['size'] = 0
    out['sha-1'] = SHA_1_NULL
    out['touched_only'] = True
    return out

  if level >= STATS_ONLY:
    filestats = os.lstat(filepath)
    is_link = stat.S_ISLNK(filestats.st_mode)
    if get_flavor() != 'win':
      # Ignore file mode on Windows since it's not really useful there.
      filemode = stat.S_IMODE(filestats.st_mode)
      # Remove write access for group and all access to 'others'.
      filemode &= ~(stat.S_IWGRP | stat.S_IRWXO)
      if read_only:
        filemode &= ~stat.S_IWUSR
      if filemode & stat.S_IXUSR:
        filemode |= stat.S_IXGRP
      else:
        filemode &= ~stat.S_IXGRP
      out['mode'] = filemode
    if not is_link:
      out['size'] = filestats.st_size
    # Used to skip recalculating the hash. Use the most recent update time.
    out['timestamp'] = int(round(filestats.st_mtime))
    # If the timestamp wasn't updated, carry on the sha-1.
    if prevdict.get('timestamp') == out['timestamp']:
      if 'sha-1' in prevdict:
        # Reuse the previous hash.
        out['sha-1'] = prevdict['sha-1']
      if 'link' in prevdict:
        # Reuse the previous link destination.
        out['link'] = prevdict['link']
    if is_link and not 'link' in out:
      # A symlink, store the link destination.
      out['link'] = os.readlink(filepath)

  if level >= WITH_HASH and not out.get('sha-1') and not out.get('link'):
    if not is_link:
      with open(filepath, 'rb') as f:
        out['sha-1'] = hashlib.sha1(f.read()).hexdigest()
  return out


### Variable stuff.


def result_to_state(filename):
  """Replaces the file's extension."""
  return filename.rsplit('.', 1)[0] + '.state'


def determine_root_dir(relative_root, infiles):
  """For a list of infiles, determines the deepest root directory that is
  referenced indirectly.

  All arguments must be using os.path.sep.
  """
  # The trick used to determine the root directory is to look at "how far" back
  # up it is looking up.
  deepest_root = relative_root
  for i in infiles:
    x = relative_root
    while i.startswith('..' + os.path.sep):
      i = i[3:]
      assert not i.startswith(os.path.sep)
      x = os.path.dirname(x)
    if deepest_root.startswith(x):
      deepest_root = x
  logging.debug(
      'determine_root_dir(%s, %d files) -> %s' % (
          relative_root, len(infiles), deepest_root))
  return deepest_root


def replace_variable(part, variables):
  m = re.match(r'<\(([A-Z_]+)\)', part)
  if m:
    if m.group(1) not in variables:
      raise ExecutionError(
        'Variable "%s" was not found in %s.\nDid you forget to specify '
        '--variable?' % (m.group(1), variables))
    return variables[m.group(1)]
  return part


def process_variables(variables, relative_base_dir):
  """Processes path variables as a special case and returns a copy of the dict.

  For each 'path' variable: first normalizes it, verifies it exists, converts it
  to an absolute path, then sets it as relative to relative_base_dir.
  """
  variables = variables.copy()
  for i in PATH_VARIABLES:
    if i not in variables:
      continue
    variable = os.path.normpath(variables[i])
    if not os.path.isdir(variable):
      raise ExecutionError('%s=%s is not a directory' % (i, variable))
    # Variables could contain / or \ on windows. Always normalize to
    # os.path.sep.
    variable = os.path.abspath(variable.replace('/', os.path.sep))
    # All variables are relative to the .isolate file.
    variables[i] = os.path.relpath(variable, relative_base_dir)
  return variables


def eval_variables(item, variables):
  """Replaces the .isolate variables in a string item.

  Note that the .isolate format is a subset of the .gyp dialect.
  """
  return ''.join(
      replace_variable(p, variables) for p in re.split(r'(<\([A-Z_]+\))', item))


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


def union(lhs, rhs):
  """Merges two compatible datastructures composed of dict/list/set."""
  assert lhs is not None or rhs is not None
  if lhs is None:
    return copy.deepcopy(rhs)
  if rhs is None:
    return copy.deepcopy(lhs)
  assert type(lhs) == type(rhs), (lhs, rhs)
  if hasattr(lhs, 'union'):
    # Includes set, OSSettings and Configs.
    return lhs.union(rhs)
  if isinstance(lhs, dict):
    return dict((k, union(lhs.get(k), rhs.get(k))) for k in set(lhs).union(rhs))
  elif isinstance(lhs, list):
    # Do not go inside the list.
    return lhs + rhs
  assert False, type(lhs)


def extract_comment(content):
  """Extracts file level comment."""
  out = []
  for line in content.splitlines(True):
    if line.startswith('#'):
      out.append(line)
    else:
      break
  return ''.join(out)


def eval_content(content):
  """Evaluates a python file and return the value defined in it.

  Used in practice for .isolate files.
  """
  globs = {'__builtins__': None}
  locs = {}
  value = eval(content, globs, locs)
  assert locs == {}, locs
  assert globs == {'__builtins__': None}, globs
  return value


def verify_variables(variables):
  """Verifies the |variables| dictionary is in the expected format."""
  VALID_VARIABLES = [
    KEY_TOUCHED,
    KEY_TRACKED,
    KEY_UNTRACKED,
    'command',
    'read_only',
  ]
  assert isinstance(variables, dict), variables
  assert set(VALID_VARIABLES).issuperset(set(variables)), variables.keys()
  for name, value in variables.iteritems():
    if name == 'read_only':
      assert value in (True, False, None), value
    else:
      assert isinstance(value, list), value
      assert all(isinstance(i, basestring) for i in value), value


def verify_condition(condition):
  """Verifies the |condition| dictionary is in the expected format."""
  VALID_INSIDE_CONDITION = ['variables']
  assert isinstance(condition, list), condition
  assert 2 <= len(condition) <= 3, condition
  assert re.match(r'OS==\"([a-z]+)\"', condition[0]), condition[0]
  for c in condition[1:]:
    assert isinstance(c, dict), c
    assert set(VALID_INSIDE_CONDITION).issuperset(set(c)), c.keys()
    verify_variables(c.get('variables', {}))


def verify_root(value):
  VALID_ROOTS = ['variables', 'conditions']
  assert isinstance(value, dict), value
  assert set(VALID_ROOTS).issuperset(set(value)), value.keys()
  verify_variables(value.get('variables', {}))

  conditions = value.get('conditions', [])
  assert isinstance(conditions, list), conditions
  for condition in conditions:
    verify_condition(condition)


def remove_weak_dependencies(values, key, item, item_oses):
  """Remove any oses from this key if the item is already under a strong key."""
  if key == KEY_TOUCHED:
    for stronger_key in (KEY_TRACKED, KEY_UNTRACKED):
      oses = values.get(stronger_key, {}).get(item, None)
      if oses:
        item_oses -= oses

  return item_oses


def invert_map(variables):
  """Converts a dict(OS, dict(deptype, list(dependencies)) to a flattened view.

  Returns a tuple of:
    1. dict(deptype, dict(dependency, set(OSes)) for easier processing.
    2. All the OSes found as a set.
  """
  KEYS = (
    KEY_TOUCHED,
    KEY_TRACKED,
    KEY_UNTRACKED,
    'command',
    'read_only',
  )
  out = dict((key, {}) for key in KEYS)
  for os_name, values in variables.iteritems():
    for key in (KEY_TOUCHED, KEY_TRACKED, KEY_UNTRACKED):
      for item in values.get(key, []):
        out[key].setdefault(item, set()).add(os_name)

    # command needs special handling.
    command = tuple(values.get('command', []))
    out['command'].setdefault(command, set()).add(os_name)

    # read_only needs special handling.
    out['read_only'].setdefault(values.get('read_only'), set()).add(os_name)
  return out, set(variables)


def reduce_inputs(values, oses):
  """Reduces the invert_map() output to the strictest minimum list.

  1. Construct the inverse map first.
  2. Look at each individual file and directory, map where they are used and
     reconstruct the inverse dictionary.
  3. Do not convert back to negative if only 2 OSes were merged.

  Returns a tuple of:
    1. the minimized dictionary
    2. oses passed through as-is.
  """
  KEYS = (
    KEY_TOUCHED,
    KEY_TRACKED,
    KEY_UNTRACKED,
    'command',
    'read_only',
  )
  out = dict((key, {}) for key in KEYS)
  assert all(oses), oses
  if len(oses) > 2:
    for key in KEYS:
      for item, item_oses in values.get(key, {}).iteritems():
        item_oses = remove_weak_dependencies(values, key, item, item_oses)
        if not item_oses:
          continue

        # Converts all oses.difference('foo') to '!foo'.
        assert all(item_oses), item_oses
        missing = oses.difference(item_oses)
        if len(missing) == 1:
          # Replace it with a negative.
          out[key][item] = set(['!' + tuple(missing)[0]])
        elif not missing:
          out[key][item] = set([None])
        else:
          out[key][item] = set(item_oses)
  else:
    for key in KEYS:
      for item, item_oses in values.get(key, {}).iteritems():
        item_oses = remove_weak_dependencies(values, key, item, item_oses)
        if not item_oses:
          continue

        # Converts all oses.difference('foo') to '!foo'.
        assert None not in item_oses, item_oses
        out[key][item] = set(item_oses)
  return out, oses


def convert_map_to_isolate_dict(values, oses):
  """Regenerates back a .isolate configuration dict from files and dirs
  mappings generated from reduce_inputs().
  """
  # First, inverse the mapping to make it dict first.
  config = {}
  for key in values:
    for item, oses in values[key].iteritems():
      if item is None:
        # For read_only default.
        continue
      for cond_os in oses:
        cond_key = None if cond_os is None else cond_os.lstrip('!')
        # Insert the if/else dicts.
        condition_values = config.setdefault(cond_key, [{}, {}])
        # If condition is negative, use index 1, else use index 0.
        cond_value = condition_values[int((cond_os or '').startswith('!'))]
        variables = cond_value.setdefault('variables', {})

        if item in (True, False):
          # One-off for read_only.
          variables[key] = item
        else:
          if isinstance(item, tuple) and item:
            # One-off for command.
            # Do not merge lists and do not sort!
            # Note that item is a tuple.
            assert key not in variables
            variables[key] = list(item)
          elif item:
            # The list of items (files or dirs). Append the new item and keep
            # the list sorted.
            l = variables.setdefault(key, [])
            l.append(item)
            l.sort()

  out = {}
  for o in sorted(config):
    d = config[o]
    if o is None:
      assert not d[1]
      out = union(out, d[0])
    else:
      c = out.setdefault('conditions', [])
      if d[1]:
        c.append(['OS=="%s"' % o] + d)
      else:
        c.append(['OS=="%s"' % o] + d[0:1])
  return out


### Internal state files.


class OSSettings(object):
  """Represents the dependencies for an OS. The structure is immutable.

  It's the .isolate settings for a specific file.
  """
  def __init__(self, name, values):
    self.name = name
    verify_variables(values)
    self.touched = sorted(values.get(KEY_TOUCHED, []))
    self.tracked = sorted(values.get(KEY_TRACKED, []))
    self.untracked = sorted(values.get(KEY_UNTRACKED, []))
    self.command = values.get('command', [])[:]
    self.read_only = values.get('read_only')

  def union(self, rhs):
    assert self.name == rhs.name
    assert not (self.command and rhs.command)
    var = {
      KEY_TOUCHED: sorted(self.touched + rhs.touched),
      KEY_TRACKED: sorted(self.tracked + rhs.tracked),
      KEY_UNTRACKED: sorted(self.untracked + rhs.untracked),
      'command': self.command or rhs.command,
      'read_only': rhs.read_only if self.read_only is None else self.read_only,
    }
    return OSSettings(self.name, var)

  def flatten(self):
    out = {}
    if self.command:
      out['command'] = self.command
    if self.touched:
      out[KEY_TOUCHED] = self.touched
    if self.tracked:
      out[KEY_TRACKED] = self.tracked
    if self.untracked:
      out[KEY_UNTRACKED] = self.untracked
    if self.read_only is not None:
      out['read_only'] = self.read_only
    return out


class Configs(object):
  """Represents a processed .isolate file.

  Stores the file in a processed way, split by each the OS-specific
  configurations.

  The self.per_os[None] member contains all the 'else' clauses plus the default
  values. It is not included in the flatten() result.
  """
  def __init__(self, oses, file_comment):
    self.file_comment = file_comment
    self.per_os = {
        None: OSSettings(None, {}),
    }
    self.per_os.update(dict((name, OSSettings(name, {})) for name in oses))

  def union(self, rhs):
    items = list(set(self.per_os.keys() + rhs.per_os.keys()))
    # Takes the first file comment, prefering lhs.
    out = Configs(items, self.file_comment or rhs.file_comment)
    for key in items:
      out.per_os[key] = union(self.per_os.get(key), rhs.per_os.get(key))
    return out

  def add_globals(self, values):
    for key in self.per_os:
      self.per_os[key] = self.per_os[key].union(OSSettings(key, values))

  def add_values(self, for_os, values):
    self.per_os[for_os] = self.per_os[for_os].union(OSSettings(for_os, values))

  def add_negative_values(self, for_os, values):
    """Includes the variables to all OSes except |for_os|.

    This includes 'None' so unknown OSes gets it too.
    """
    for key in self.per_os:
      if key != for_os:
        self.per_os[key] = self.per_os[key].union(OSSettings(key, values))

  def flatten(self):
    """Returns a flat dictionary representation of the configuration.

    Skips None pseudo-OS.
    """
    return dict(
        (k, v.flatten()) for k, v in self.per_os.iteritems() if k is not None)


def load_isolate_as_config(value, file_comment, default_oses):
  """Parses one .isolate file and returns a Configs() instance.

  |value| is the loaded dictionary that was defined in the gyp file.

  The expected format is strict, anything diverting from the format below will
  throw an assert:
  {
    'variables': {
      'command': [
        ...
      ],
      'isolate_dependency_tracked': [
        ...
      ],
      'isolate_dependency_untracked': [
        ...
      ],
      'read_only': False,
    },
    'conditions': [
      ['OS=="<os>"', {
        'variables': {
          ...
        },
      }, {  # else
        'variables': {
          ...
        },
      }],
      ...
    ],
  }
  """
  verify_root(value)

  # Scan to get the list of OSes.
  conditions = value.get('conditions', [])
  oses = set(re.match(r'OS==\"([a-z]+)\"', c[0]).group(1) for c in conditions)
  oses = oses.union(default_oses)
  configs = Configs(oses, file_comment)

  # Global level variables.
  configs.add_globals(value.get('variables', {}))

  # OS specific variables.
  for condition in conditions:
    condition_os = re.match(r'OS==\"([a-z]+)\"', condition[0]).group(1)
    configs.add_values(condition_os, condition[1].get('variables', {}))
    if len(condition) > 2:
      configs.add_negative_values(
          condition_os, condition[2].get('variables', {}))
  return configs


def load_isolate_for_flavor(content, flavor):
  """Loads the .isolate file and returns the information unprocessed.

  Returns the command, dependencies and read_only flag. The dependencies are
  fixed to use os.path.sep.
  """
  # Load the .isolate file, process its conditions, retrieve the command and
  # dependencies.
  configs = load_isolate_as_config(eval_content(content), None, DEFAULT_OSES)
  config = configs.per_os.get(flavor) or configs.per_os.get(None)
  if not config:
    raise ExecutionError('Failed to load configuration for \'%s\'' % flavor)
  # Merge tracked and untracked dependencies, isolate.py doesn't care about the
  # trackability of the dependencies, only the build tool does.
  dependencies = [
    f.replace('/', os.path.sep) for f in config.tracked + config.untracked
  ]
  touched = [f.replace('/', os.path.sep) for f in config.touched]
  return config.command, dependencies, touched, config.read_only


class Flattenable(object):
  """Represents data that can be represented as a json file."""
  MEMBERS = ()

  def flatten(self):
    """Returns a json-serializable version of itself.

    Skips None entries.
    """
    items = ((member, getattr(self, member)) for member in self.MEMBERS)
    return dict((member, value) for member, value in items if value is not None)

  @classmethod
  def load(cls, data):
    """Loads a flattened version."""
    data = data.copy()
    out = cls()
    for member in out.MEMBERS:
      if member in data:
        # Access to a protected member XXX of a client class
        # pylint: disable=W0212
        out._load_member(member, data.pop(member))
    if data:
      raise ValueError(
          'Found unexpected entry %s while constructing an object %s' %
            (data, cls.__name__), data, cls.__name__)
    return out

  def _load_member(self, member, value):
    """Loads a member into self."""
    setattr(self, member, value)

  @classmethod
  def load_file(cls, filename):
    """Loads the data from a file or return an empty instance."""
    out = cls()
    try:
      out = cls.load(trace_inputs.read_json(filename))
      logging.debug('Loaded %s(%s)' % (cls.__name__, filename))
    except (IOError, ValueError):
      logging.warn('Failed to load %s' % filename)
    return out


class Result(Flattenable):
  """Describes the content of a .result file.

  This file is used by run_test_from_archive.py so its content is strictly only
  what is necessary to run the test outside of a checkout.

  It is important to note that the 'files' dict keys are using native OS path
  separator instead of '/' used in .isolate file.
  """
  MEMBERS = (
    'command',
    'files',
    'os',
    'read_only',
    'relative_cwd',
  )

  os = get_flavor()

  def __init__(self):
    super(Result, self).__init__()
    self.command = []
    self.files = {}
    self.read_only = None
    self.relative_cwd = None

  def update(self, command, infiles, touched, read_only, relative_cwd):
    """Updates the result state with new information."""
    self.command = command
    # Add new files.
    for f in infiles:
      self.files.setdefault(f, {})
    for f in touched:
      self.files.setdefault(f, {})['touched_only'] = True
    # Prune extraneous files that are not a dependency anymore.
    for f in set(self.files).difference(set(infiles).union(touched)):
      del self.files[f]
    if read_only is not None:
      self.read_only = read_only
    self.relative_cwd = relative_cwd

  def _load_member(self, member, value):
    if member == 'os':
      if value != self.os:
        raise run_test_from_archive.ConfigError(
            'The .results file was created on another platform')
    else:
      super(Result, self)._load_member(member, value)

  def __str__(self):
    out = '%s(\n' % self.__class__.__name__
    out += '  command: %s\n' % self.command
    out += '  files: %d\n' % len(self.files)
    out += '  read_only: %s\n' % self.read_only
    out += '  relative_cwd: %s)' % self.relative_cwd
    return out


class SavedState(Flattenable):
  """Describes the content of a .state file.

  The items in this file are simply to improve the developer's life and aren't
  used by run_test_from_archive.py. This file can always be safely removed.

  isolate_file permits to find back root_dir, variables are used for stateful
  rerun.
  """
  MEMBERS = (
    'isolate_file',
    'variables',
  )

  def __init__(self):
    super(SavedState, self).__init__()
    self.isolate_file = None
    self.variables = {}

  def update(self, isolate_file, variables):
    """Updates the saved state with new information."""
    self.isolate_file = isolate_file
    self.variables.update(variables)

  @classmethod
  def load(cls, data):
    out = super(SavedState, cls).load(data)
    if out.isolate_file:
      out.isolate_file = trace_inputs.get_native_path_case(out.isolate_file)
    return out

  def __str__(self):
    out = '%s(\n' % self.__class__.__name__
    out += '  isolate_file: %s\n' % self.isolate_file
    out += '  variables: %s' % ''.join(
        '\n    %s=%s' % (k, self.variables[k]) for k in sorted(self.variables))
    out += ')'
    return out


class CompleteState(object):
  """Contains all the state to run the task at hand."""
  def __init__(self, result_file, result, saved_state, out_dir):
    super(CompleteState, self).__init__()
    self.result_file = result_file
    # Contains the data that will be used by run_test_from_archive.py
    self.result = result
    # Contains the data to ease developer's use-case but that is not strictly
    # necessary.
    self.saved_state = saved_state
    self.out_dir = out_dir

  @classmethod
  def load_files(cls, result_file, out_dir):
    """Loads state from disk."""
    assert os.path.isabs(result_file), result_file
    return cls(
        result_file,
        Result.load_file(result_file),
        SavedState.load_file(result_to_state(result_file)),
        out_dir)

  def load_isolate(self, isolate_file, variables):
    """Updates self.result and self.saved_state with information loaded from a
    .isolate file.

    Processes the loaded data, deduce root_dir, relative_cwd.
    """
    # Make sure to not depend on os.getcwd().
    assert os.path.isabs(isolate_file), isolate_file
    logging.info(
        'CompleteState.load_isolate(%s, %s)' % (isolate_file, variables))
    relative_base_dir = os.path.dirname(isolate_file)

    # Processes the variables and update the saved state.
    variables = process_variables(variables, relative_base_dir)
    self.saved_state.update(isolate_file, variables)

    with open(isolate_file, 'r') as f:
      # At that point, variables are not replaced yet in command and infiles.
      # infiles may contain directory entries and is in posix style.
      command, infiles, touched, read_only = load_isolate_for_flavor(
          f.read(), get_flavor())
    command = [eval_variables(i, self.saved_state.variables) for i in command]
    infiles = [eval_variables(f, self.saved_state.variables) for f in infiles]
    touched = [eval_variables(f, self.saved_state.variables) for f in touched]
    # root_dir is automatically determined by the deepest root accessed with the
    # form '../../foo/bar'.
    root_dir = determine_root_dir(relative_base_dir, infiles + touched)
    # The relative directory is automatically determined by the relative path
    # between root_dir and the directory containing the .isolate file,
    # isolate_base_dir.
    relative_cwd = os.path.relpath(relative_base_dir, root_dir)
    # Normalize the files based to root_dir. It is important to keep the
    # trailing os.path.sep at that step.
    infiles = [
      relpath(normpath(os.path.join(relative_base_dir, f)), root_dir)
      for f in infiles
    ]
    touched = [
      relpath(normpath(os.path.join(relative_base_dir, f)), root_dir)
      for f in touched
    ]
    # Expand the directories by listing each file inside. Up to now, trailing
    # os.path.sep must be kept. Do not expand 'touched'.
    infiles = expand_directories_and_symlinks(
        root_dir,
        infiles,
        lambda x: re.match(r'.*\.(git|svn|pyc)$', x))

    # Finally, update the new stuff in the foo.result file, the file that is
    # used by run_test_from_archive.py.
    self.result.update(command, infiles, touched, read_only, relative_cwd)
    logging.debug(self)

  def process_inputs(self, level):
    """Updates self.result.files with the files' mode and hash.

    See process_input() for more information.
    """
    for infile in sorted(self.result.files):
      filepath = os.path.join(self.root_dir, infile)
      self.result.files[infile] = process_input(
          filepath, self.result.files[infile], level, self.result.read_only)

  def save_files(self):
    """Saves both self.result and self.saved_state."""
    logging.debug('Dumping to %s' % self.result_file)
    trace_inputs.write_json(self.result_file, self.result.flatten(), True)
    total_bytes = sum(i.get('size', 0) for i in self.result.files.itervalues())
    if total_bytes:
      logging.debug('Total size: %d bytes' % total_bytes)
    saved_state_file = result_to_state(self.result_file)
    logging.debug('Dumping to %s' % saved_state_file)
    trace_inputs.write_json(saved_state_file, self.saved_state.flatten(), True)

  @property
  def root_dir(self):
    """isolate_file is always inside relative_cwd relative to root_dir."""
    isolate_dir = os.path.dirname(self.saved_state.isolate_file)
    # Special case '.'.
    if self.result.relative_cwd == '.':
      return isolate_dir
    assert isolate_dir.endswith(self.result.relative_cwd), (
        isolate_dir, self.result.relative_cwd)
    return isolate_dir[:-(len(self.result.relative_cwd) + 1)]

  @property
  def resultdir(self):
    """Directory containing the results, usually equivalent to the variable
    PRODUCT_DIR.
    """
    return os.path.dirname(self.result_file)

  def __str__(self):
    def indent(data, indent_length):
      """Indents text."""
      spacing = ' ' * indent_length
      return ''.join(spacing + l for l in str(data).splitlines(True))

    out = '%s(\n' % self.__class__.__name__
    out += '  root_dir: %s\n' % self.root_dir
    out += '  result: %s\n' % indent(self.result, 2)
    out += '  saved_state: %s)' % indent(self.saved_state, 2)
    return out


def load_complete_state(options, level):
  """Loads a CompleteState.

  This includes data from .isolate, .result and .state files.

  Arguments:
    options: Options instance generated with OptionParserIsolate.
    level: Amount of data to fetch.
  """
  if options.result:
    # Load the previous state if it was present. Namely, "foo.result" and
    # "foo.state".
    complete_state = CompleteState.load_files(options.result, options.outdir)
  else:
    # Constructs a dummy object that cannot be saved. Useful for temporary
    # commands like 'run'.
    complete_state = CompleteState(None, Result(), SavedState(), options.outdir)
  options.isolate = options.isolate or complete_state.saved_state.isolate_file
  if not options.isolate:
    raise ExecutionError('A .isolate file is required.')
  if (complete_state.saved_state.isolate_file and
      options.isolate != complete_state.saved_state.isolate_file):
    raise ExecutionError(
        '%s and %s do not match.' % (
          options.isolate, complete_state.saved_state.isolate_file))

  # Then load the .isolate and expands directories.
  complete_state.load_isolate(options.isolate, options.variables)

  # Regenerate complete_state.result.files.
  complete_state.process_inputs(level)
  return complete_state


def read_trace_as_isolate_dict(complete_state):
  """Reads a trace and returns the .isolate dictionary."""
  api = trace_inputs.get_api()
  logfile = complete_state.result_file + '.log'
  if not os.path.isfile(logfile):
    raise ExecutionError(
        'No log file \'%s\' to read, did you forget to \'trace\'?' % logfile)
  try:
    results = trace_inputs.load_trace(
        logfile, complete_state.root_dir, api, default_blacklist)
    tracked, touched = split_touched(results.existent)
    value = generate_isolate(
        tracked,
        [],
        touched,
        complete_state.root_dir,
        complete_state.saved_state.variables,
        complete_state.result.relative_cwd)
    return value
  except trace_inputs.TracingFailure, e:
    raise ExecutionError(
        'Reading traces failed for: %s\n%s' %
          (' '.join(complete_state.result.command), str(e)))


def print_all(comment, data, stream):
  """Prints a complete .isolate file and its top-level file comment into a
  stream.
  """
  if comment:
    stream.write(comment)
  pretty_print(data, stream)


def merge(complete_state):
  """Reads a trace and merges it back into the source .isolate file."""
  value = read_trace_as_isolate_dict(complete_state)

  # Now take that data and union it into the original .isolate file.
  with open(complete_state.saved_state.isolate_file, 'r') as f:
    prev_content = f.read()
  prev_config = load_isolate_as_config(
      eval_content(prev_content),
      extract_comment(prev_content),
      DEFAULT_OSES)
  new_config = load_isolate_as_config(value, '', DEFAULT_OSES)
  config = union(prev_config, new_config)
  # pylint: disable=E1103
  data = convert_map_to_isolate_dict(
      *reduce_inputs(*invert_map(config.flatten())))
  print 'Updating %s' % complete_state.saved_state.isolate_file
  with open(complete_state.saved_state.isolate_file, 'wb') as f:
    print_all(config.file_comment, data, f)


def CMDcheck(args):
  """Checks that all the inputs are present and update .result."""
  parser = OptionParserIsolate(command='check')
  options, _ = parser.parse_args(args)
  complete_state = load_complete_state(options, NO_INFO)

  # Nothing is done specifically. Just store the result and state.
  complete_state.save_files()
  return 0


def CMDhashtable(args):
  """Creates a hash table content addressed object store.

  All the files listed in the .result file are put in the output directory with
  the file name being the sha-1 of the file's content.
  """
  parser = OptionParserIsolate(command='hashtable')
  options, _ = parser.parse_args(args)

  success = False
  try:
    complete_state = load_complete_state(options, WITH_HASH)
    options.outdir = (
        options.outdir or os.path.join(complete_state.resultdir, 'hashtable'))
    recreate_tree(
        outdir=options.outdir,
        indir=complete_state.root_dir,
        infiles=complete_state.result.files,
        action=run_test_from_archive.HARDLINK,
        as_sha1=True)

    complete_state.save_files()

    # Also archive the .result file.
    with open(complete_state.result_file, 'rb') as f:
      result_hash = hashlib.sha1(f.read()).hexdigest()
    logging.info(
        '%s -> %s' %
        (os.path.basename(complete_state.result_file), result_hash))
    outfile = os.path.join(options.outdir, result_hash)
    if os.path.isfile(outfile):
      # Just do a quick check that the file size matches. If they do, skip the
      # archive. This mean the build result didn't change at all.
      out_size = os.stat(outfile).st_size
      in_size = os.stat(complete_state.result_file).st_size
      if in_size == out_size:
        success = True
        return 0

    run_test_from_archive.link_file(
        outfile, complete_state.result_file,
        run_test_from_archive.HARDLINK)
    success = True
    return 0
  finally:
    # If the command failed, delete the .results file if it exists. This is
    # important so no stale swarm job is executed.
    if not success and os.path.isfile(options.result):
      os.remove(options.result)


def CMDnoop(args):
  """Touches --result but does nothing else.

  This mode is to help transition since some builders do not have all the test
  data files checked out. Touch result_file and exit silently.
  """
  parser = OptionParserIsolate(command='noop')
  options, _ = parser.parse_args(args)
  # In particular, do not call load_complete_state().
  open(options.result, 'a').close()
  return 0


def CMDmerge(args):
  """Reads and merges the data from the trace back into the original .isolate.

  Ignores --outdir.
  """
  parser = OptionParserIsolate(command='merge', require_result=False)
  options, _ = parser.parse_args(args)
  complete_state = load_complete_state(options, NO_INFO)
  merge(complete_state)
  return 0


def CMDread(args):
  """Reads the trace file generated with command 'trace'.

  Ignores --outdir.
  """
  parser = OptionParserIsolate(command='read', require_result=False)
  options, _ = parser.parse_args(args)
  complete_state = load_complete_state(options, NO_INFO)
  value = read_trace_as_isolate_dict(complete_state)
  pretty_print(value, sys.stdout)
  return 0


def CMDremap(args):
  """Creates a directory with all the dependencies mapped into it.

  Useful to test manually why a test is failing. The target executable is not
  run.
  """
  parser = OptionParserIsolate(command='remap', require_result=False)
  options, _ = parser.parse_args(args)
  complete_state = load_complete_state(options, STATS_ONLY)

  if not options.outdir:
    options.outdir = run_test_from_archive.make_temp_dir(
        'isolate', complete_state.root_dir)
  else:
    if not os.path.isdir(options.outdir):
      os.makedirs(options.outdir)
  print 'Remapping into %s' % options.outdir
  if len(os.listdir(options.outdir)):
    raise ExecutionError('Can\'t remap in a non-empty directory')
  recreate_tree(
      outdir=options.outdir,
      indir=complete_state.root_dir,
      infiles=complete_state.result.files,
      action=run_test_from_archive.HARDLINK,
      as_sha1=False)
  if complete_state.result.read_only:
    run_test_from_archive.make_writable(options.outdir, True)

  if complete_state.result_file:
    complete_state.save_files()
  return 0


def CMDrun(args):
  """Runs the test executable in an isolated (temporary) directory.

  All the dependencies are mapped into the temporary directory and the
  directory is cleaned up after the target exits. Warning: if -outdir is
  specified, it is deleted upon exit.

  Argument processing stops at the first non-recognized argument and these
  arguments are appended to the command line of the target to run. For example,
  use: isolate.py -r foo.results -- --gtest_filter=Foo.Bar
  """
  parser = OptionParserIsolate(command='run', require_result=False)
  parser.enable_interspersed_args()
  options, args = parser.parse_args(args)
  complete_state = load_complete_state(options, STATS_ONLY)
  cmd = complete_state.result.command + args
  if not cmd:
    raise ExecutionError('No command to run')
  cmd = trace_inputs.fix_python_path(cmd)
  try:
    if not options.outdir:
      options.outdir = run_test_from_archive.make_temp_dir(
          'isolate', complete_state.root_dir)
    else:
      if not os.path.isdir(options.outdir):
        os.makedirs(options.outdir)
    recreate_tree(
        outdir=options.outdir,
        indir=complete_state.root_dir,
        infiles=complete_state.result.files,
        action=run_test_from_archive.HARDLINK,
        as_sha1=False)
    cwd = os.path.normpath(
        os.path.join(options.outdir, complete_state.result.relative_cwd))
    if not os.path.isdir(cwd):
      # It can happen when no files are mapped from the directory containing the
      # .isolate file. But the directory must exist to be the current working
      # directory.
      os.makedirs(cwd)
    if complete_state.result.read_only:
      run_test_from_archive.make_writable(options.outdir, True)
    logging.info('Running %s, cwd=%s' % (cmd, cwd))
    result = subprocess.call(cmd, cwd=cwd)
  finally:
    if options.outdir:
      run_test_from_archive.rmtree(options.outdir)

  if complete_state.result_file:
    complete_state.save_files()
  return result


def CMDtrace(args):
  """Traces the target using trace_inputs.py.

  It runs the executable without remapping it, and traces all the files it and
  its child processes access. Then the 'read' command can be used to generate an
  updated .isolate file out of it.

  Argument processing stops at the first non-recognized argument and these
  arguments are appended to the command line of the target to run. For example,
  use: isolate.py -r foo.results -- --gtest_filter=Foo.Bar
  """
  parser = OptionParserIsolate(command='trace')
  parser.enable_interspersed_args()
  parser.add_option(
      '-m', '--merge', action='store_true',
      help='After tracing, merge the results back in the .isolate file')
  options, args = parser.parse_args(args)
  complete_state = load_complete_state(options, STATS_ONLY)
  cmd = complete_state.result.command + args
  if not cmd:
    raise ExecutionError('No command to run')
  cmd = trace_inputs.fix_python_path(cmd)
  cwd = os.path.normpath(os.path.join(
      complete_state.root_dir, complete_state.result.relative_cwd))
  logging.info('Running %s, cwd=%s' % (cmd, cwd))
  api = trace_inputs.get_api()
  logfile = complete_state.result_file + '.log'
  api.clean_trace(logfile)
  try:
    with api.get_tracer(logfile) as tracer:
      result, _ = tracer.trace(
          cmd,
          cwd,
          'default',
          True)
  except trace_inputs.TracingFailure, e:
    raise ExecutionError('Tracing failed for: %s\n%s' % (' '.join(cmd), str(e)))

  complete_state.save_files()

  if options.merge:
    merge(complete_state)

  return result


class OptionParserIsolate(trace_inputs.OptionParserWithNiceDescription):
  """Adds automatic --isolate, --result, --out and --variables handling."""
  def __init__(self, require_result=True, **kwargs):
    trace_inputs.OptionParserWithNiceDescription.__init__(self, **kwargs)
    default_variables = [('OS', get_flavor())]
    if sys.platform in ('win32', 'cygwin'):
      default_variables.append(('EXECUTABLE_SUFFIX', '.exe'))
    else:
      default_variables.append(('EXECUTABLE_SUFFIX', ''))
    group = optparse.OptionGroup(self, "Common options")
    group.add_option(
        '-r', '--result',
        metavar='FILE',
        help='.result file to store the json manifest')
    group.add_option(
        '-i', '--isolate',
        metavar='FILE',
        help='.isolate file to load the dependency data from')
    group.add_option(
        '-V', '--variable',
        nargs=2,
        action='append',
        default=default_variables,
        dest='variables',
        metavar='FOO BAR',
        help='Variables to process in the .isolate file, default: %default. '
             'Variables are persistent accross calls, they are saved inside '
             '<results>.state')
    group.add_option(
        '-o', '--outdir', metavar='DIR',
        help='Directory used to recreate the tree or store the hash table. '
            'If the environment variable ISOLATE_HASH_TABLE_DIR exists, it '
            'will be used. Otherwise, for run and remap, uses a /tmp '
            'subdirectory. For the other modes, defaults to the directory '
            'containing --result')
    self.add_option_group(group)
    self.require_result = require_result

  def parse_args(self, *args, **kwargs):
    """Makes sure the paths make sense.

    On Windows, / and \ are often mixed together in a path.
    """
    options, args = trace_inputs.OptionParserWithNiceDescription.parse_args(
        self, *args, **kwargs)
    if not self.allow_interspersed_args and args:
      self.error('Unsupported argument: %s' % args)

    options.variables = dict(options.variables)

    if self.require_result and not options.result:
      self.error('--result is required.')
    if options.result and not options.result.endswith('.results'):
      self.error('--result value must end with \'.results\'')

    if options.result:
      options.result = os.path.abspath(options.result.replace('/', os.path.sep))

    if options.isolate:
      options.isolate = trace_inputs.get_native_path_case(
          os.path.abspath(
              options.isolate.replace('/', os.path.sep)))

    if options.outdir:
      options.outdir = os.path.abspath(
          options.outdir.replace('/', os.path.sep))

    return options, args


### Glue code to make all the commands works magically.


CMDhelp = trace_inputs.CMDhelp


def main(argv):
  try:
    return trace_inputs.main_impl(argv)
  except (
      ExecutionError,
      run_test_from_archive.MappingError,
      run_test_from_archive.ConfigError) as e:
    sys.stderr.write('\nError: ')
    sys.stderr.write(str(e))
    sys.stderr.write('\n')
    return 1


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
