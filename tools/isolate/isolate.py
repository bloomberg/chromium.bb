#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Front end tool to manage .isolate files and corresponding tests.

Run ./isolate.py --help for more detailed information.

See more information at
http://dev.chromium.org/developers/testing/isolated-testing
"""

import hashlib
import logging
import optparse
import os
import re
import stat
import subprocess
import sys
import tempfile

import isolate_common
import merge_isolate
import trace_inputs
import run_test_from_archive

# Used by process_input().
NO_INFO, STATS_ONLY, WITH_HASH = range(56, 59)


class ExecutionError(Exception):
  """A generic error occurred."""
  def __str__(self):
    return self.args[0]


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


def split_at_symlink(indir, relfile):
  """Scans each component of relfile and cut the string at the symlink if there
  is any.

  Returns a tuple (symlink, rest), with rest == None if not symlink was found.
  """
  parts = relfile.rstrip(os.path.sep).split(os.path.sep)
  for i in range(len(parts)):
    if os.path.islink(os.path.join(indir, *parts[:i])):
      # A symlink! Keep the trailing os.path.sep if there was any.
      out = os.path.join(*parts[:i])
      rest = os.path.sep.join(parts[i:])
      logging.debug('split_at_symlink(%s) -> (%s, %s)' % (relfile, out, rest))
      return out, rest
  return relfile, None


def expand_directory_and_symlink(indir, relfile, blacklist):
  """Expands a single input. It can result in multiple outputs.

  This function is recursive when relfile is a directory or a symlink.

  Note: this code doesn't properly handle recursive symlink like one created
  with:
    ln -s .. foo
  """
  logging.debug(
      'expand_directory_and_symlink(%s, %s, %s)' % (indir, relfile, blacklist))
  if os.path.isabs(relfile):
    raise run_test_from_archive.MappingError(
        'Can\'t map absolute path %s' % relfile)

  infile = normpath(os.path.join(indir, relfile))
  if not infile.startswith(indir):
    raise run_test_from_archive.MappingError(
        'Can\'t map file %s outside %s' % (infile, indir))

  # Look if any item in relfile is a symlink.
  symlink_relfile, rest = split_at_symlink(indir, relfile)
  if rest is not None:
    # Append everything pointed by the symlink. If the symlink is recursive,
    # this code blows up.
    pointed = os.readlink(os.path.join(indir, symlink_relfile))
    # Override infile with the new destination.
    dest_infile = normpath(
        os.path.join(indir, os.path.dirname(symlink_relfile), pointed, rest))
    if os.path.isdir(dest_infile):
      dest_infile += os.path.sep
    if not dest_infile.startswith(indir):
      raise run_test_from_archive.MappingError(
          'Can\'t map symlink reference %s->%s outside of %s' %
          (symlink_relfile, dest_infile, indir))
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
  logging.debug(
      'expand_directories_and_symlinks(%s, %s, %s)' %
        (indir, infiles, blacklist))
  outfiles = []
  for relfile in infiles:
    outfiles.extend(expand_directory_and_symlink(indir, relfile, blacklist))
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


def load_isolate(content):
  """Loads the .isolate file and returns the information unprocessed.

  Returns the command, dependencies and read_only flag. The dependencies are
  fixed to use os.path.sep.
  """
  # Load the .isolate file, process its conditions, retrieve the command and
  # dependencies.
  configs = merge_isolate.load_gyp(
      merge_isolate.eval_content(content), None, merge_isolate.DEFAULT_OSES)
  flavor = isolate_common.get_flavor()
  config = configs.per_os.get(flavor) or configs.per_os.get(None)
  if not config:
    raise ExecutionError('Failed to load configuration for \'%s\'' % flavor)
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
    filestats = os.lstat(filepath)
    is_link = stat.S_ISLNK(filestats.st_mode)
    if isolate_common.get_flavor() != 'win':
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

  if level >= WITH_HASH and not out.get('sha-1') and not out.get('link'):
    if is_link:
      # A symlink, store the link destination instead.
      out['link'] = os.readlink(filepath)
    else:
      with open(filepath, 'rb') as f:
        out['sha-1'] = hashlib.sha1(f.read()).hexdigest()
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
    if os.path.islink(infile):
      pointed = os.readlink(infile)
      logging.debug('Symlink: %s -> %s' % (outfile, pointed))
      os.symlink(pointed, outfile)
    else:
      run_test_from_archive.link_file(outfile, infile, action)


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
      'determine_root_dir(%s, %s) -> %s' % (
          relative_root, infiles, deepest_root))
  return deepest_root


def process_variables(variables, relative_base_dir):
  """Processes path variables as a special case and returns a copy of the dict.

  For each 'path' variable: first normalizes it, verifies it exists, converts it
  to an absolute path, then sets it as relative to relative_base_dir.
  """
  variables = variables.copy()
  for i in isolate_common.PATH_VARIABLES:
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
    if data:
      raise ValueError(
          'Found unexpected entry %s while constructing an object %s' %
            (data, cls.__name__), data, cls.__name__)
    return out

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
    for f in set(self.files).difference(infiles):
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
      command, infiles, read_only = load_isolate(f.read())
    command = [eval_variables(i, self.saved_state.variables) for i in command]
    infiles = [eval_variables(f, self.saved_state.variables) for f in infiles]
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
    infiles = expand_directories_and_symlinks(
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
    logging.debug('Dumping to %s' % self.result_file)
    trace_inputs.write_json(self.result_file, self.result.flatten(), False)
    total_bytes = sum(i.get('size', 0) for i in self.result.files.itervalues())
    if total_bytes:
      logging.debug('Total size: %d bytes' % total_bytes)
    saved_state_file = result_to_state(self.result_file)
    logging.debug('Dumping to %s' % saved_state_file)
    trace_inputs.write_json(saved_state_file, self.saved_state.flatten(), False)

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
  complete_state = load_complete_state(options, WITH_HASH)

  options.outdir = (
      options.outdir or os.path.join(complete_state.resultdir, 'hashtable'))
  if not os.path.isdir(options.outdir):
    os.makedirs(options.outdir)

  # Map each of the file.
  for relfile, properties in complete_state.result.files.iteritems():
    infile = os.path.join(complete_state.root_dir, relfile)
    if not 'sha-1' in properties:
      # It's a symlink.
      continue
    outfile = os.path.join(options.outdir, properties['sha-1'])
    if os.path.isfile(outfile):
      # Just do a quick check that the file size matches. No need to stat()
      # again the input file, grab the value from the dict.
      out_size = os.stat(outfile).st_size
      in_size = (
          complete_state.result.files[relfile].get('size') or
          os.stat(infile).st_size)
      if in_size == out_size:
        continue
      # Otherwise, an exception will be raised.
    logging.info('%s -> %s' % (relfile, properties['sha-1']))
    run_test_from_archive.link_file(
        outfile, infile, run_test_from_archive.HARDLINK)

  complete_state.save_files()

  # Also archive the .result file.
  with open(complete_state.result_file, 'rb') as f:
    result_hash = hashlib.sha1(f.read()).hexdigest()
  logging.info(
      '%s -> %s' % (os.path.basename(complete_state.result_file), result_hash))
  run_test_from_archive.link_file(
      os.path.join(options.outdir, result_hash),
      complete_state.result_file,
      run_test_from_archive.HARDLINK)
  return 0


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


def CMDread(args):
  """Reads the trace file generated with command 'trace'.

  Ignores --outdir.
  """
  parser = OptionParserIsolate(command='read', require_result=False)
  options, _ = parser.parse_args(args)
  complete_state = load_complete_state(options, NO_INFO)

  api = trace_inputs.get_api()
  logfile = complete_state.result_file + '.log'
  if not os.path.isfile(logfile):
    raise ExecutionError(
        'No log file \'%s\' to read, did you forget to \'trace\'?' % logfile)
  try:
    results = trace_inputs.load_trace(
        logfile, complete_state.root_dir, api, isolate_common.default_blacklist)
    value = isolate_common.generate_isolate(
        results.existent,
        complete_state.root_dir,
        complete_state.saved_state.variables,
        complete_state.result.relative_cwd)
    isolate_common.pretty_print(value, sys.stdout)
  except trace_inputs.TracingFailure, e:
    raise ExecutionError(
        'Reading traces failed for: %s\n%s' %
          (' '.join(complete_state.result.command, str(e))))

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
    options.outdir = tempfile.mkdtemp(prefix='isolate')
  else:
    if not os.path.isdir(options.outdir):
      os.makedirs(options.outdir)
  print 'Remapping into %s' % options.outdir
  if len(os.listdir(options.outdir)):
    raise ExecutionError('Can\'t remap in a non-empty directory')
  recreate_tree(
      options.outdir,
      complete_state.root_dir,
      complete_state.result.files.keys(),
      run_test_from_archive.HARDLINK)
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
  """
  parser = OptionParserIsolate(command='run', require_result=False)
  options, _ = parser.parse_args(args)
  complete_state = load_complete_state(options, STATS_ONLY)
  try:
    if not options.outdir:
      options.outdir = tempfile.mkdtemp(prefix='isolate')
    else:
      if not os.path.isdir(options.outdir):
        os.makedirs(options.outdir)
    recreate_tree(
        options.outdir,
        complete_state.root_dir,
        complete_state.result.files.keys(),
        run_test_from_archive.HARDLINK)
    cwd = os.path.join(options.outdir, complete_state.result.relative_cwd)
    if not os.path.isdir(cwd):
      os.makedirs(cwd)
    if complete_state.result.read_only:
      run_test_from_archive.make_writable(options.outdir, True)
    if not complete_state.result.command:
      raise ExecutionError('No command to run')
    cmd = trace_inputs.fix_python_path(complete_state.result.command)
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
  """
  parser = OptionParserIsolate(command='trace')
  options, _ = parser.parse_args(args)
  complete_state = load_complete_state(options, STATS_ONLY)

  cwd = os.path.join(
      complete_state.root_dir, complete_state.result.relative_cwd)
  logging.info('Running %s, cwd=%s' % (complete_state.result.command, cwd))
  if not complete_state.result.command:
    raise ExecutionError('No command to run')
  api = trace_inputs.get_api()
  logfile = complete_state.result_file + '.log'
  try:
    with api.get_tracer(logfile) as tracer:
      result, _ = tracer.trace(
          complete_state.result.command,
          cwd,
          'default',
          True)
  except trace_inputs.TracingFailure, e:
    raise ExecutionError(
        'Tracing failed for: %s\n%s' %
          (' '.join(complete_state.result.command, str(e))))

  complete_state.save_files()
  return result


class OptionParserWithNiceDescription(optparse.OptionParser):
  """Generates the description with the command's docstring."""
  def __init__(self, *args, **kwargs):
    """Sets 'description' and 'usage' if not already specified."""
    command = kwargs.pop('command', None)
    if not 'description' in kwargs:
      kwargs['description'] = re.sub(
          '[\r\n ]{2,}', ' ', get_command_handler(command).__doc__)
    kwargs.setdefault('usage', '%%prog %s [options]' % command)
    optparse.OptionParser.__init__(self, *args, **kwargs)


class OptionParserWithLogging(OptionParserWithNiceDescription):
  """Adds automatic --verbose handling."""
  def __init__(self, *args, **kwargs):
    OptionParserWithNiceDescription.__init__(self, *args, **kwargs)
    self.add_option(
        '-v', '--verbose',
        action='count',
        default=int(os.environ.get('ISOLATE_DEBUG', 0)),
        help='Use multiple times to increase verbosity')

  def parse_args(self, *args, **kwargs):
    options, args = OptionParserWithNiceDescription.parse_args(
        self, *args, **kwargs)
    level = [
      logging.ERROR, logging.INFO, logging.DEBUG,
    ][min(2, options.verbose)]
    logging.basicConfig(
          level=level,
          format='%(levelname)5s %(module)15s(%(lineno)3d):%(message)s')
    return options, args


class OptionParserIsolate(OptionParserWithLogging):
  """Adds automatic --isolate, --result, --out and --variables handling."""
  def __init__(self, require_result=True, *args, **kwargs):
    OptionParserWithLogging.__init__(self, *args, **kwargs)
    default_variables = [('OS', isolate_common.get_flavor())]
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
        help='Variables to process in the .isolate file, default: %default')
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
    options, args = OptionParserWithLogging.parse_args(self, *args, **kwargs)
    if args:
      self.error('Unsupported argument: %s' % args)

    options.variables = dict(options.variables)

    if self.require_result and not options.result:
      self.error('--result is required.')
    if not options.result.endswith('.results'):
      self.error('--result value must end with \'.results\'')

    if options.result:
      options.result = os.path.abspath(options.result.replace('/', os.path.sep))

    if options.isolate:
      options.isolate = os.path.abspath(
          options.isolate.replace('/', os.path.sep))

    if options.outdir:
      options.outdir = os.path.abspath(
          options.outdir.replace('/', os.path.sep))

    # Still returns args to stay consistent even if guaranteed to be empty.
    return options, args


### Glue code to make all the commands works magically.


def extract_documentation():
  """Returns a dict {command: description} for each of documented command."""
  commands = (
      fn[3:]
      for fn in dir(sys.modules[__name__])
      if fn.startswith('CMD') and get_command_handler(fn[3:]).__doc__)
  return dict((fn, get_command_handler(fn).__doc__) for fn in commands)


def CMDhelp(args):
  """Prints list of commands or help for a specific command."""
  doc = extract_documentation()
  # Calculates the optimal offset.
  offset = max(len(cmd) for cmd in doc)
  format_str = '  %-' + str(offset + 2) + 's %s'
  # Generate a one-liner documentation of each commands.
  commands_description = '\n'.join(
       format_str % (cmd, doc[cmd].split('\n')[0]) for cmd in sorted(doc))

  parser = OptionParserWithLogging(
      usage='%prog <command> [options]',
      description='Commands are:\n%s\n' % commands_description)
  parser.format_description = lambda _: parser.description

  # Strip out any -h or --help argument.
  _, args = parser.parse_args([i for i in args if not i in ('-h', '--help')])
  if len(args) == 1:
    if not get_command_handler(args[0]):
      parser.error('Unknown command %s' % args[0])
    # The command was "%prog help command", replaces ourself with
    # "%prog command --help" so help is correctly printed out.
    return main(args + ['--help'])
  elif args:
    parser.error('Unknown argument "%s"' % ' '.join(args))
  parser.print_help()
  return 0


def get_command_handler(name):
  """Returns the command handler or CMDhelp if it doesn't exist."""
  return getattr(sys.modules[__name__], 'CMD%s' % name, None)


def main(argv):
  command = get_command_handler(argv[0] if argv else 'help')
  if not command:
    return CMDhelp(argv)
  try:
    return command(argv[1:])
  except (ExecutionError, run_test_from_archive.MappingError), e:
    sys.stderr.write('\nError: ')
    sys.stderr.write(str(e))
    sys.stderr.write('\n')
    return 1


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
