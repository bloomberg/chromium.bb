#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Does one of the following depending on the --mode argument:
  check     Verifies all the inputs exist, touches the file specified with
            --result and exits.
  hashtable Puts a manifest file and hard links each of the inputs into the
            output directory.
  noop      Do nothing, used for transition purposes.
  remap     Stores all the inputs files in a directory without running the
            executable.
  run       Recreates a tree with all the inputs files and run the executable
            in it.
  trace     Runs the executable without remapping it but traces all the files
            it and its child processes access.

See more information at
http://dev.chromium.org/developers/testing/isolated-testing
"""

import hashlib
import json
import logging
import optparse
import os
import re
import stat
import subprocess
import sys
import tempfile

import merge_isolate
import trace_inputs
import run_test_from_archive

# Used by process_input().
NO_INFO, STATS_ONLY, WITH_HASH = range(56, 59)


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


def expand_directories(indir, infiles, blacklist):
  """Expands the directories, applies the blacklist and verifies files exist."""
  logging.debug('expand_directories(%s, %s, %s)' % (indir, infiles, blacklist))
  outfiles = []
  for relfile in infiles:
    if os.path.isabs(relfile):
      raise run_test_from_archive.MappingError(
          'Can\'t map absolute path %s' % relfile)
    infile = normpath(os.path.join(indir, relfile))
    if not infile.startswith(indir):
      raise run_test_from_archive.MappingError(
          'Can\'t map file %s outside %s' % (infile, indir))

    if relfile.endswith(os.path.sep):
      if not os.path.isdir(infile):
        raise run_test_from_archive.MappingError(
            '%s is not a directory' % infile)
      for dirpath, dirnames, filenames in os.walk(infile):
        # Convert the absolute path to subdir + relative subdirectory.
        reldirpath = dirpath[len(indir)+1:]
        files_to_add = (os.path.join(reldirpath, f) for f in filenames)
        outfiles.extend(f for f in files_to_add if not blacklist(f))
        for index, dirname in enumerate(dirnames):
          # Do not process blacklisted directories.
          if blacklist(os.path.join(reldirpath, dirname)):
            del dirnames[index]
    else:
      # Always add individual files even if they were blacklisted.
      if os.path.isdir(infile):
        raise run_test_from_archive.MappingError(
            'Input directory %s must have a trailing slash' % infile)
      if not os.path.isfile(infile):
        raise run_test_from_archive.MappingError(
            'Input file %s doesn\'t exist' % infile)
      outfiles.append(relfile)
  return outfiles


def replace_variable(part, variables):
  m = re.match(r'<\(([A-Z_]+)\)', part)
  if m:
    assert m.group(1) in variables, (
        '%s was not found in %s' % (m.group(1), variables))
    return variables[m.group(1)]
  return part


def eval_variables(item, variables):
  """Replaces the gyp variables in a string item."""
  return ''.join(
      replace_variable(p, variables) for p in re.split(r'(<\([A-Z_]+\))', item))


def indent(data, indent_length):
  """Indents text."""
  spacing = ' ' * indent_length
  return ''.join(spacing + l for l in str(data).splitlines(True))


def load_isolate(content, error):
  """Loads the .isolate file and returns the information unprocessed.

  Returns the command, dependencies and read_only flag. The dependencies are
  fixed to use os.path.sep.
  """
  # Load the .isolate file, process its conditions, retrieve the command and
  # dependencies.
  configs = merge_isolate.load_gyp(merge_isolate.eval_content(content))
  flavor = trace_inputs.get_flavor()
  config = configs.per_os.get(flavor) or configs.per_os.get(None)
  if not config:
    error('Failed to load configuration for \'%s\'' % flavor)
  # Merge tracked and untracked dependencies, isolate.py doesn't care about the
  # trackability of the dependencies, only the build tool does.
  dependencies = [
    f.replace('/', os.path.sep) for f in config.tracked + config.untracked
  ]
  return config.command, dependencies, config.read_only


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
  """
  assert level in (NO_INFO, STATS_ONLY, WITH_HASH)
  out = {}
  if level >= STATS_ONLY:
    filestats = os.stat(filepath)
    if trace_inputs.get_flavor() != 'win':
      filemode = stat.S_IMODE(filestats.st_mode)
      # Remove write access for non-owner.
      filemode &= ~(stat.S_IWGRP | stat.S_IWOTH)
      if read_only:
        filemode &= ~stat.S_IWUSR
      if filemode & stat.S_IXUSR:
        filemode |= (stat.S_IXGRP | stat.S_IXOTH)
      else:
        filemode &= ~(stat.S_IXGRP | stat.S_IXOTH)
      out['mode'] = filemode
    out['size'] = filestats.st_size
    # Used to skip recalculating the hash. Use the most recent update time.
    out['timestamp'] = int(round(filestats.st_mtime))
    # If the timestamp wasn't updated, carry on the sha-1.
    if (prevdict.get('timestamp') == out['timestamp'] and
        'sha-1' in prevdict):
      # Reuse the previous hash.
      out['sha-1'] = prevdict['sha-1']

  if level >= WITH_HASH and not out.get('sha-1'):
    h = hashlib.sha1()
    with open(filepath, 'rb') as f:
      h.update(f.read())
    out['sha-1'] = h.hexdigest()
  return out


def recreate_tree(outdir, indir, infiles, action):
  """Creates a new tree with only the input files in it.

  Arguments:
    outdir:    Output directory to create the files in.
    indir:     Root directory the infiles are based in.
    infiles:   List of files to map from |indir| to |outdir|.
    action:    See assert below.
  """
  logging.debug(
      'recreate_tree(%s, %s, %s, %s)' % (outdir, indir, infiles, action))
  logging.info('Mapping from %s to %s' % (indir, outdir))

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

  for relfile in infiles:
    infile = os.path.join(indir, relfile)
    outfile = os.path.join(outdir, relfile)
    outsubdir = os.path.dirname(outfile)
    if not os.path.isdir(outsubdir):
      os.makedirs(outsubdir)
    run_test_from_archive.link_file(outfile, infile, action)


def result_to_state(filename):
  """Replaces the file's extension."""
  return filename.rsplit('.', 1)[0] + '.state'


def write_json(stream, data):
  """Writes data to a stream as json."""
  json.dump(data, stream, indent=2, sort_keys=True)
  stream.write('\n')


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
      'determine_root_dir(%s, %s) -> %s' % (
          relative_root, infiles, deepest_root))
  return deepest_root


def process_variables(variables, relative_base_dir, error):
  """Processes path variables as a special case and returns a copy of the dict.

  For each 'path' varaible: first normalizes it, verifies it exists, converts it
  to an absolute path, then sets it as relative to relative_base_dir.
  """
  variables = variables.copy()
  for i in ('DEPTH', 'PRODUCT_DIR'):
    if i not in variables:
      continue
    variable = os.path.normpath(variables[i])
    if not os.path.isdir(variable):
      error('%s=%s is not a directory' % (i, variable))
    # Variables could contain / or \ on windows. Always normalize to
    # os.path.sep.
    variable = os.path.abspath(variable.replace('/', os.path.sep))
    # All variables are relative to the .isolate file.
    variables[i] = os.path.relpath(variable, relative_base_dir)
  return variables


class Flattenable(object):
  """Represents data that can be represented as a json file."""
  MEMBERS = ()

  def flatten(self):
    """Returns a json-serializable version of itself."""
    return dict((member, getattr(self, member)) for member in self.MEMBERS)

  @classmethod
  def load(cls, data):
    """Loads a flattened version."""
    data = data.copy()
    out = cls()
    for member in out.MEMBERS:
      if member in data:
        value = data.pop(member)
        setattr(out, member, value)
    assert not data, data
    return out

  @classmethod
  def load_file(cls, filename):
    """Loads the data from a file or return an empty instance."""
    out = cls()
    try:
      with open(filename, 'r') as f:
        out = cls.load(json.load(f))
      logging.debug('Loaded %s(%s)' % (cls.__name__, filename))
    except IOError:
      pass
    return out


class Result(Flattenable):
  """Describes the content of a .result file.

  This file is used by run_test_from_archive.py so its content is strictly only
  what is necessary to run the test outside of a checkout.
  """
  MEMBERS = (
    'command',
    'files',
    'read_only',
    'relative_cwd',
  )

  def __init__(self):
    super(Result, self).__init__()
    self.command = []
    self.files = {}
    self.read_only = None
    self.relative_cwd = None

  def update(self, command, infiles, read_only, relative_cwd):
    """Updates the result state with new information."""
    self.command = command
    # Add new files.
    for f in infiles:
      self.files.setdefault(f, {})
    # Prune extraneous files that are not a dependency anymore.
    for f in set(infiles).difference(self.files.keys()):
      del self.files[f]
    if read_only is not None:
      self.read_only = read_only
    self.relative_cwd = relative_cwd

  def __str__(self):
    out = '%s(\n' % self.__class__.__name__
    out += '  command: %s\n' % self.command
    out += '  files: %s\n' % ', '.join(sorted(self.files))
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
    assert result_file.rsplit('.', 1)[1] == 'result', result_file
    return cls(
        result_file,
        Result.load_file(result_file),
        SavedState.load_file(result_to_state(result_file)),
        out_dir)

  def load_isolate(self, isolate_file, variables, error):
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
    variables = process_variables(variables, relative_base_dir, error)
    self.saved_state.update(isolate_file, variables)

    with open(isolate_file, 'r') as f:
      # At that point, variables are not replaced yet in command and infiles.
      # infiles may contain directory entries and is in posix style.
      command, infiles, read_only = load_isolate(f.read(), error)
    command = [eval_variables(i, variables) for i in command]
    infiles = [eval_variables(f, variables) for f in infiles]
    # root_dir is automatically determined by the deepest root accessed with the
    # form '../../foo/bar'.
    root_dir = determine_root_dir(relative_base_dir, infiles)
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
    # Expand the directories by listing each file inside. Up to now, trailing
    # os.path.sep must be kept.
    infiles = expand_directories(
        root_dir,
        infiles,
        lambda x: re.match(r'.*\.(git|svn|pyc)$', x))

    # Finally, update the new stuff in the foo.result file, the file that is
    # used by run_test_from_archive.py.
    self.result.update(command, infiles, read_only, relative_cwd)
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
    with open(self.result_file, 'wb') as f:
      write_json(f, self.result.flatten())
    total_bytes = sum(i.get('size', 0) for i in self.result.files.itervalues())
    if total_bytes:
      logging.debug('Total size: %d bytes' % total_bytes)
    with open(result_to_state(self.result_file), 'wb') as f:
      write_json(f, self.saved_state.flatten())

  @property
  def root_dir(self):
    """isolate_file is always inside relative_cwd relative to root_dir."""
    isolate_dir = os.path.dirname(self.saved_state.isolate_file)
    # Special case '.'.
    if self.result.relative_cwd == '.':
      return isolate_dir
    assert isolate_dir.endswith(self.result.relative_cwd), (
        isolate_dir, self.result.relative_cwd)
    return isolate_dir[:-len(self.result.relative_cwd)]

  @property
  def resultdir(self):
    """Directory containing the results, usually equivalent to the variable
    PRODUCT_DIR.
    """
    return os.path.dirname(self.result_file)

  def __str__(self):
    out = '%s(\n' % self.__class__.__name__
    out += '  root_dir: %s\n' % self.root_dir
    out += '  result: %s\n' % indent(self.result, 2)
    out += '  saved_state: %s)' % indent(self.saved_state, 2)
    return out


def MODEcheck(_outdir, _state):
  """No-op."""
  return 0


def MODEhashtable(outdir, state):
  outdir = (
      outdir or os.path.join(os.path.dirname(state.resultdir), 'hashtable'))
  if not os.path.isdir(outdir):
    os.makedirs(outdir)
  for relfile, properties in state.result.files.iteritems():
    infile = os.path.join(state.root_dir, relfile)
    outfile = os.path.join(outdir, properties['sha-1'])
    if os.path.isfile(outfile):
      # Just do a quick check that the file size matches. No need to stat()
      # again the input file, grab the value from the dict.
      out_size = os.stat(outfile).st_size
      in_size = (
          state.result.files[infile].get('size') or
          os.stat(infile).st_size)
      if in_size == out_size:
        continue
      # Otherwise, an exception will be raised.
    run_test_from_archive.link_file(
        outfile, infile, run_test_from_archive.HARDLINK)
  return 0


def MODEremap(outdir, state):
  if not outdir:
    outdir = tempfile.mkdtemp(prefix='isolate')
  else:
    if not os.path.isdir(outdir):
      os.makedirs(outdir)
  print 'Remapping into %s' % outdir
  if len(os.listdir(outdir)):
    print 'Can\'t remap in a non-empty directory'
    return 1
  recreate_tree(
      outdir,
      state.root_dir,
      state.result.files.keys(),
      run_test_from_archive.HARDLINK)
  if state.result.read_only:
    run_test_from_archive.make_writable(outdir, True)
  return 0


def MODErun(_outdir, state):
  """Always uses a temporary directory."""
  try:
    outdir = tempfile.mkdtemp(prefix='isolate')
    recreate_tree(
        outdir,
        state.root_dir,
        state.result.files.keys(),
        run_test_from_archive.HARDLINK)
    cwd = os.path.join(outdir, state.result.relative_cwd)
    if not os.path.isdir(cwd):
      os.makedirs(cwd)
    if state.result.read_only:
      run_test_from_archive.make_writable(outdir, True)
    if not state.result.command:
      print 'No command to run'
      return 1
    cmd = trace_inputs.fix_python_path(state.result.command)
    logging.info('Running %s, cwd=%s' % (cmd, cwd))
    return subprocess.call(cmd, cwd=cwd)
  finally:
    run_test_from_archive.rmtree(outdir)


def MODEtrace(_outdir, state):
  """Shortcut to use trace_inputs.py properly.

  It constructs the equivalent of dictfiles. It is hardcoded to base the
  checkout at src/.
  """
  logging.info(
      'Running %s, cwd=%s' % (
          state.result.command,
          os.path.join(state.root_dir, state.result.relative_cwd)))
  product_dir = None
  if state.resultdir and state.root_dir:
    # Defaults to none if both are the same directory.
    try:
      product_dir = os.path.relpath(state.resultdir, state.root_dir) or None
    except ValueError:
      # This happens on Windows if state.resultdir is one drive, let's say
      # 'C:\' and state.root_dir on another one like 'D:\'.
      product_dir = None
  if not state.result.command:
    print 'No command to run'
    return 1
  return trace_inputs.trace_inputs(
      state.result_file + '.log',
      state.result.command,
      state.root_dir,
      state.result.relative_cwd,
      product_dir,
      False)


# Must be declared after all the functions.
VALID_MODES = {
  'check': MODEcheck,
  'hashtable': MODEhashtable,
  'remap': MODEremap,
  'run': MODErun,
  'trace': MODEtrace,
}


# Only hashtable mode really needs the sha-1.
LEVELS = {
  'check': NO_INFO,
  'hashtable': WITH_HASH,
  'remap': STATS_ONLY,
  'run': STATS_ONLY,
  'trace': STATS_ONLY,
}


assert (
  sorted(i[4:] for i in dir(sys.modules[__name__]) if i.startswith('MODE')) ==
  sorted(VALID_MODES))


def isolate(result_file, isolate_file, mode, variables, out_dir, error):
  """Main function to isolate a target with its dependencies.

  Arguments:
  - result_file: File to load or save state from.
  - isolate_file: File to load data from. Can be None if result_file contains
                  the necessary information.
  - mode: Action to do. See file level docstring.
  - variables: Variables to process, if necessary.
  - out_dir: Output directory where the result is stored. It's use depends on
             |mode|.

  Some arguments are optional, dependending on |mode|. See the corresponding
  MODE<mode> function for the exact behavior.
  """
  # First, load the previous stuff if it was present. Namely, "foo.result" and
  # "foo.state".
  complete_state = CompleteState.load_files(result_file, out_dir)
  isolate_file = isolate_file or complete_state.saved_state.isolate_file
  if not isolate_file:
    error('A .isolate file is required.')
  if (complete_state.saved_state.isolate_file and
      isolate_file != complete_state.saved_state.isolate_file):
    error(
        '%s and %s do not match.' % (
          isolate_file, complete_state.saved_state.isolate_file))

  try:
    # Then process options and expands directories.
    complete_state.load_isolate(isolate_file, variables, error)

    # Regenerate complete_state.result.files.
    complete_state.process_inputs(LEVELS[mode])

    # Finally run the mode-specific code.
    result = VALID_MODES[mode](out_dir, complete_state)
  except run_test_from_archive.MappingError, e:
    error(str(e))

  # Then store the result and state.
  complete_state.save_files()
  return result


def main():
  """Handles CLI and normalizes the input arguments to pass them to isolate().
  """
  default_variables = [('OS', trace_inputs.get_flavor())]
  if sys.platform in ('win32', 'cygwin'):
    default_variables.append(('EXECUTABLE_SUFFIX', '.exe'))
  else:
    default_variables.append(('EXECUTABLE_SUFFIX', ''))
  valid_modes = sorted(VALID_MODES.keys() + ['noop'])
  parser = optparse.OptionParser(
      usage='%prog [options] [.isolate file]',
      description=sys.modules[__name__].__doc__)
  parser.format_description = lambda *_: parser.description
  parser.add_option(
      '-v', '--verbose',
      action='count',
      default=int(os.environ.get('ISOLATE_DEBUG', 0)),
      help='Use multiple times')
  parser.add_option(
      '-m', '--mode',
      choices=valid_modes,
      help='Determines the action to be taken: %s' % ', '.join(valid_modes))
  parser.add_option(
      '-r', '--result',
      metavar='FILE',
      help='Result file to store the json manifest')
  parser.add_option(
      '-V', '--variable',
      nargs=2,
      action='append',
      default=default_variables,
      dest='variables',
      metavar='FOO BAR',
      help='Variables to process in the .isolate file, default: %default')
  parser.add_option(
      '-o', '--outdir', metavar='DIR',
      help='Directory used to recreate the tree or store the hash table. '
           'If the environment variable ISOLATE_HASH_TABLE_DIR exists, it will '
           'be used. Otherwise, for run and remap, uses a /tmp subdirectory. '
           'For the other modes, defaults to the directory containing --result')

  options, args = parser.parse_args()
  level = [logging.ERROR, logging.INFO, logging.DEBUG][min(2, options.verbose)]
  logging.basicConfig(
      level=level,
      format='%(levelname)5s %(module)15s(%(lineno)3d): %(message)s')

  if not options.mode:
    parser.error('--mode is required')
  if not options.result:
    parser.error('--result is required.')

  if len(args) > 1:
    logging.debug('%s' % sys.argv)
    parser.error('Use only one argument which should be a .isolate file')

  if options.mode == 'noop':
    # This undocumented mode is to help transition since some builders do not
    # have all the test data files checked out. Exit silently.
    return 0

  # Make sure the paths make sense. On Windows, / and \ are often mixed together
  # in a path.
  result_file = os.path.abspath(options.result.replace('/', os.path.sep))
  # input_file may be None.
  input_file = (
      os.path.abspath(args[0].replace('/', os.path.sep)) if args else None)
  # out_dir may be None.
  out_dir = (
      os.path.abspath(options.outdir.replace('/', os.path.sep))
      if options.outdir else None)
  # Fix variables.
  variables = dict(options.variables)

  # After basic validation, pass this to isolate().
  return isolate(
      result_file,
      input_file,
      options.mode,
      variables,
      out_dir,
      parser.error)


if __name__ == '__main__':
  sys.exit(main())
