#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Does one of the following depending on the --mode argument:
  check     Verifies all the inputs exist, touches the file specified with
            --result and exits.
  hashtable Puts a manifest file and hard links each of the inputs into the
            output directory.
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
import posixpath
import re
import stat
import subprocess
import sys
import tempfile

import merge_isolate
import trace_inputs
import run_test_from_archive

# Used by process_inputs().
NO_INFO, STATS_ONLY, WITH_HASH = range(56, 59)


def relpath(path, root):
  """os.path.relpath() that keeps trailing slash."""
  out = os.path.relpath(path, root)
  if path.endswith(os.path.sep):
    out += os.path.sep
  elif sys.platform == 'win32' and path.endswith('/'):
    # TODO(maruel): Temporary.
    out += os.path.sep
  return out


def normpath(path):
  """os.path.normpath() that keeps trailing slash."""
  out = os.path.normpath(path)
  if path.endswith(('/', os.path.sep)):
    out += os.path.sep
  return out


def to_relative(path, root, relative):
  """Converts any absolute path to a relative path, only if under root."""
  if sys.platform == 'win32':
    path = path.lower()
    root = root.lower()
    relative = relative.lower()
  if path.startswith(root):
    logging.info('%s starts with %s' % (path, root))
    path = os.path.relpath(path, relative)
  else:
    logging.info('%s not under %s' % (path, root))
  return path


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
    return variables[m.group(1)]
  return part


def eval_variables(item, variables):
  return ''.join(
      replace_variable(p, variables) for p in re.split(r'(<\([A-Z_]+\))', item))


def load_isolate(content, variables, error):
  """Loads the .isolate file. Returns the command, dependencies and read_only
  flag.
  """
  # Load the .isolate file, process its conditions, retrieve the command and
  # dependencies.
  configs = merge_isolate.load_gyp(merge_isolate.eval_content(content))
  flavor = trace_inputs.get_flavor()
  config = configs.per_os.get(flavor) or configs.per_os.get(None)
  if not config:
    error('Failed to load configuration for \'%s\'' % flavor)

  # Convert the variables and merge tracked and untracked dependencies.
  # isolate.py doesn't care about the trackability of the dependencies.
  infiles = [
    eval_variables(f, variables) for f in config.tracked
  ] + [
    eval_variables(f, variables) for f in config.untracked
  ]
  command = [eval_variables(i, variables) for i in config.command]
  return command, infiles, config.read_only


def process_inputs(prevdict, indir, infiles, level, read_only):
  """Returns a dictionary of input files, populated with the files' mode and
  hash.

  |prevdict| is the previous dictionary. It is used to retrieve the cached sha-1
  to skip recalculating the hash.

  |level| determines the amount of information retrieved.
  1 loads no information.  2 loads minimal stat() information. 3 calculates the
  sha-1 of the file's content.

  The file mode is manipulated if read_only is True. In practice, we only save
  one of 4 modes: 0755 (rwx), 0644 (rw), 0555 (rx), 0444 (r). On windows, mode
  is not set since all files are 'executable' by default.
  """
  assert level in (NO_INFO, STATS_ONLY, WITH_HASH)
  outdict = {}
  for infile in infiles:
    filepath = os.path.join(indir, infile)
    outdict[infile] = {}
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
        outdict[infile]['mode'] = filemode
      outdict[infile]['size'] = filestats.st_size
      # Used to skip recalculating the hash. Use the most recent update time.
      outdict[infile]['timestamp'] = int(round(filestats.st_mtime))
      # If the timestamp wasn't updated, carry on the sha-1.
      if (prevdict.get(infile, {}).get('timestamp') ==
          outdict[infile]['timestamp'] and
          'sha-1' in prevdict[infile]):
        # Reuse the previous hash.
        outdict[infile]['sha-1'] = prevdict[infile]['sha-1']

    if level >= WITH_HASH and not outdict[infile].get('sha-1'):
      h = hashlib.sha1()
      with open(filepath, 'rb') as f:
        h.update(f.read())
      outdict[infile]['sha-1'] = h.hexdigest()
  return outdict


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


def load_results(resultfile):
  """Loads the previous results as an optimization."""
  data = {}
  if resultfile and os.path.isfile(resultfile):
    resultfile = os.path.abspath(resultfile)
    with open(resultfile, 'r') as f:
      data = json.load(f)
  else:
    resultfile = os.path.abspath(resultfile)

  # Works with native os.path.sep but stores as '/'.
  if 'files' in data and os.path.sep != '/':
    data['files'] = dict(
        (k.replace('/', os.path.sep), v)
        for k, v in data['files'].iteritems())
  return data


def save_results(resultfile, data):
  data = data.copy()

  # Works with native os.path.sep but stores as '/'.
  if os.path.sep != '/':
    data['files'] = dict(
        (k.replace(os.path.sep, '/'), v) for k, v in data['files'].iteritems())

  f = None
  try:
    if resultfile:
      f = open(resultfile, 'wb')
    else:
      f = sys.stdout
    json.dump(data, f, indent=2, sort_keys=True)
    f.write('\n')
  finally:
    if resultfile and f:
      f.close()

  total_bytes = sum(i.get('size', 0) for i in data['files'].itervalues())
  if total_bytes:
    logging.debug('Total size: %d bytes' % total_bytes)


def isolate(outdir, mode, indir, infiles, data):
  """Main function to isolate a target with its dependencies.

  Arguments:
  - outdir: Output directory where the result is stored. Depends on |mode|.
  - indir: Root directory to be used as the base directory for infiles.
  - infiles: List of files, with relative path, to process.
  - mode: Action to do. See file level docstring.
  - data: Contains all the command specific meta-data.

  Some arguments are optional, dependending on |mode|. See the corresponding
  MODE<mode> function for the exact behavior.
  """
  modes = {
    'check': MODEcheck,
    'hashtable': MODEhashtable,
    'remap': MODEremap,
    'run': MODErun,
    'trace': MODEtrace,
  }
  mode_fn = modes[mode]

  infiles = expand_directories(
      indir, infiles, lambda x: re.match(r'.*\.(git|svn|pyc)$', x))

  # Only hashtable mode really needs the sha-1.
  level = {
    'check': NO_INFO,
    'hashtable': WITH_HASH,
    'remap': STATS_ONLY,
    'run': STATS_ONLY,
    'trace': STATS_ONLY,
  }
  # Regenerate data['files'] from infiles.
  data['files'] = process_inputs(
      data.get('files', {}), indir, infiles, level[mode], data.get('read_only'))

  result = mode_fn(outdir, indir, data)
  return result, data


def MODEcheck(_outdir, _indir, _data):
  """No-op."""
  return 0


def MODEhashtable(outdir, indir, data):
  outdir = (
      outdir or os.path.join(os.path.dirname(data['resultdir']), 'hashtable'))
  if not os.path.isdir(outdir):
    os.makedirs(outdir)
  for relfile, properties in data['files'].iteritems():
    infile = os.path.join(indir, relfile)
    outfile = os.path.join(outdir, properties['sha-1'])
    if os.path.isfile(outfile):
      # Just do a quick check that the file size matches. No need to stat()
      # again the input file, grab the value from the dict.
      out_size = os.stat(outfile).st_size
      in_size = (
          data.get('files', {}).get(infile, {}).get('size') or
          os.stat(infile).st_size)
      if in_size == out_size:
        continue
      # Otherwise, an exception will be raised.
    run_test_from_archive.link_file(
        outfile, infile, run_test_from_archive.HARDLINK)
  return 0


def MODEremap(outdir, indir, data):
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
      outdir, indir, data['files'].keys(), run_test_from_archive.HARDLINK)
  if data['read_only']:
    run_test_from_archive.make_writable(outdir, True)
  return 0


def MODErun(_outdir, indir, data):
  """Always uses a temporary directory."""
  try:
    outdir = tempfile.mkdtemp(prefix='isolate')
    recreate_tree(
        outdir, indir, data['files'].keys(), run_test_from_archive.HARDLINK)
    cwd = os.path.join(outdir, data['relative_cwd'])
    if not os.path.isdir(cwd):
      os.makedirs(cwd)
    if data['read_only']:
      run_test_from_archive.make_writable(outdir, True)
    if not data['command']:
      print 'No command to run'
      return 1
    cmd = trace_inputs.fix_python_path(data['command'])
    logging.info('Running %s, cwd=%s' % (cmd, cwd))
    return subprocess.call(cmd, cwd=cwd)
  finally:
    run_test_from_archive.rmtree(outdir)


def MODEtrace(_outdir, indir, data):
  """Shortcut to use trace_inputs.py properly.

  It constructs the equivalent of dictfiles. It is hardcoded to base the
  checkout at src/.
  """
  logging.info(
      'Running %s, cwd=%s' % (
          data['command'], os.path.join(indir, data['relative_cwd'])))
  product_dir = None
  if data['resultdir'] and indir:
    # Defaults to none if both are the same directory.
    try:
      product_dir = os.path.relpath(data['resultdir'], indir) or None
    except ValueError:
      # This happens on Windows if data['resultdir'] is one drive, let's say
      # 'C:\' and indir on another one like 'D:\'.
      product_dir = None
  if not data['command']:
    print 'No command to run'
    return 1
  return trace_inputs.trace_inputs(
      data['resultfile'],
      data['command'],
      indir,
      data['relative_cwd'],
      product_dir,
      False)


def get_valid_modes():
  """Returns the modes that can be used."""
  return sorted(
      i[4:] for i in dir(sys.modules[__name__]) if i.startswith('MODE'))


def process_options(variables, resultfile, input_file, error):
  """Processes the options and loads the input file. Returns the processed
  values.
  """
  input_file = os.path.abspath(input_file)
  isolate_dir = os.path.dirname(input_file)
  resultfile = os.path.abspath(resultfile)
  logging.info(
      'process_options(%s, %s, %s, ...)' % (variables, resultfile, input_file))

  # Process path variables as a special case. First normalize it, verifies it
  # exists, convert it to an absolute path, then set it as relative to
  # isolate_dir.
  for i in ('DEPTH', 'PRODUCT_DIR'):
    if i not in variables:
      continue
    variable = os.path.normpath(variables[i])
    if not os.path.isdir(variable):
      error('%s=%s is not a directory' % (i, variable))
    variable = os.path.abspath(variable)
    # All variables are relative to the input file.
    variables[i] = os.path.relpath(variable, isolate_dir)

  command, infiles, read_only = load_isolate(
      open(input_file, 'r').read(), variables, error)

  # The trick used to determine the root directory is to look at "how far" back
  # up it is looking up.
  # TODO(maruel): Stop the msbuild generator from generating a mix of / and \\.
  isolate_dir_replaced = isolate_dir.replace(os.path.sep, '/')
  root_dir = isolate_dir_replaced
  logging.debug('root_dir before searching: %s' % root_dir)
  for i in infiles:
    i = i.replace(os.path.sep, '/')
    x = isolate_dir.replace(os.path.sep, '/')
    while i.startswith('../'):
      i = i[3:]
      assert not i.startswith('/')
      x = posixpath.dirname(x)
    if root_dir.startswith(x):
      root_dir = x
  root_dir = root_dir.replace('/', os.path.sep)
  logging.debug('root_dir after searching: %s' % root_dir)

  # The relative directory is automatically determined by the relative path
  # between root_dir and the directory containing the .isolate file.
  relative_dir = os.path.relpath(isolate_dir, root_dir).replace(
      os.path.sep, '/')
  logging.debug('relative_dir: %s' % relative_dir)

  logging.debug(
      'variables: %s' % ', '.join(
        '%s=%s' % (k, v) for k, v in variables.iteritems()))

  data = load_results(resultfile)

  command, infiles, read_only = load_isolate(
      open(input_file, 'r').read(), variables, error)

  # Update data with the up to date information:
  data['command'] = command
  data['read_only'] = read_only
  data['relative_cwd'] = relative_dir
  data['resultfile'] = resultfile
  data['resultdir'] = os.path.dirname(resultfile)

  # Keep the old variables.
  data.setdefault('variables', {}).update(variables)

  logging.debug('command: %s' % command)
  logging.debug('infiles: %s' % infiles)
  logging.debug('read_only: %s' % read_only)
  infiles = [normpath(os.path.join(relative_dir, f)) for f in infiles]
  logging.debug('processed infiles: %s' % infiles)
  return root_dir, infiles, data


def main():
  default_variables = [('OS', trace_inputs.get_flavor())]
  if sys.platform in ('win32', 'cygwin'):
    default_variables.append(('EXECUTABLE_SUFFIX', '.exe'))
  else:
    default_variables.append(('EXECUTABLE_SUFFIX', ''))
  valid_modes = get_valid_modes()
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
  if len(args) != 1:
    logging.debug('%s' % sys.argv)
    parser.error('Use only one argument which should be a .isolate file')

  root_dir, infiles, data = process_options(
      dict(options.variables), options.result, args[0], parser.error)

  try:
    resultcode, data = isolate(
        options.outdir,
        options.mode,
        root_dir,
        infiles,
        data)
  except run_test_from_archive.MappingError, e:
    print >> sys.stderr, str(e)
    return 1
  save_results(options.result, data)
  return resultcode


if __name__ == '__main__':
  sys.exit(main())
