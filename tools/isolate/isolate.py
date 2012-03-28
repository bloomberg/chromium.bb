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
import re
import stat
import subprocess
import sys
import tempfile

import trace_inputs
import tree_creator


def relpath(path, root):
  """os.path.relpath() that keeps trailing slash."""
  out = os.path.relpath(path, root)
  if path.endswith('/'):
    out += '/'
  return out


def to_relative(path, root, relative):
  """Converts any absolute path to a relative path, only if under root."""
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
      raise tree_creator.MappingError('Can\'t map absolute path %s' % relfile)
    infile = os.path.normpath(os.path.join(indir, relfile))
    if not infile.startswith(indir):
      raise tree_creator.MappingError(
          'Can\'t map file %s outside %s' % (infile, indir))

    if relfile.endswith('/'):
      if not os.path.isdir(infile):
        raise tree_creator.MappingError(
            'Input directory %s must have a trailing slash' % infile)
      for dirpath, dirnames, filenames in os.walk(infile):
        # Convert the absolute path to subdir + relative subdirectory.
        reldirpath = dirpath[len(indir)+1:]
        outfiles.extend(os.path.join(reldirpath, f) for f in filenames)
        for index, dirname in enumerate(dirnames):
          # Do not process blacklisted directories.
          if blacklist(os.path.join(reldirpath, dirname)):
            del dirnames[index]
    else:
      if not os.path.isfile(infile):
        raise tree_creator.MappingError('Input file %s doesn\'t exist' % infile)
      outfiles.append(relfile)
  return outfiles


def process_inputs(indir, infiles, need_hash, read_only):
  """Returns a dictionary of input files, populated with the files' mode and
  hash.

  The file mode is manipulated if read_only is True. In practice, we only save
  one of 4 modes: 0755 (rwx), 0644 (rw), 0555 (rx), 0444 (r).
  """
  outdict = {}
  for infile in infiles:
    filepath = os.path.join(indir, infile)
    filemode = stat.S_IMODE(os.stat(filepath).st_mode)
    # Remove write access for non-owner.
    filemode &= ~(stat.S_IWGRP | stat.S_IWOTH)
    if read_only:
      filemode &= ~stat.S_IWUSR
    if filemode & stat.S_IXUSR:
      filemode |= (stat.S_IXGRP | stat.S_IXOTH)
    else:
      filemode &= ~(stat.S_IXGRP | stat.S_IXOTH)
    outdict[infile] = {
      'mode': filemode,
    }
    if need_hash:
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
      tree_creator.HARDLINK, tree_creator.SYMLINK, tree_creator.COPY)
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
    tree_creator.link_file(outfile, infile, action)


def separate_inputs_command(args, root, files):
  """Strips off the command line from the inputs.

  gyp provides input paths relative to cwd. Convert them to be relative to root.
  OptionParser kindly strips off '--' from sys.argv if it's provided and that's
  the first non-arg value. Manually look up if it was present in sys.argv.
  """
  cmd = []
  if '--' in args:
    i = args.index('--')
    cmd = args[i+1:]
    args = args[:i]
  elif '--' in sys.argv:
    # optparse is messing with us. Fix it manually.
    cmd = args
    args = []
  if files:
    args = [
      i.decode('utf-8') for i in open(files, 'rb').read().splitlines() if i
    ] + args
  cwd = os.getcwd()
  return [relpath(os.path.join(cwd, arg), root) for arg in args], cmd


def isolate(outdir, resultfile, indir, infiles, mode, read_only, cmd, no_save):
  """Main function to isolate a target with its dependencies.

  Arguments:
  - outdir: Output directory where the result is stored. Depends on |mode|.
  - resultfile: File to save the json data.
  - indir: Root directory to be used as the base directory for infiles.
  - infiles: List of files, with relative path, to process.
  - mode: Action to do. See file level docstring.
  - read_only: Makes the temporary directory read only.
  - cmd: Command to execute.
  - no_save: If True, do not touch resultfile.

  Some arguments are optional, dependending on |mode|. See the corresponding
  MODE<mode> function for the exact behavior.
  """
  mode_fn = getattr(sys.modules[__name__], 'MODE' + mode)
  assert mode_fn
  assert os.path.isabs(resultfile)

  infiles = expand_directories(
      indir, infiles, lambda x: re.match(r'.*\.(svn|pyc)$', x))

  # Note the relative current directory.
  # In general, this path will be the path containing the gyp file where the
  # target was defined. This relative directory may be created implicitely if a
  # file from this directory is needed to run the test. Otherwise it won't be
  # created and the process creation will fail. It's up to the caller to create
  # this directory manually before starting the test.
  cwd = os.getcwd()
  relative_cwd = os.path.relpath(cwd, indir)

  # Workaround make behavior of passing absolute paths.
  cmd = [to_relative(i, indir, cwd) for i in cmd]

  if not cmd:
    # Note that it is exactly the reverse of relative_cwd.
    cmd = [os.path.join(os.path.relpath(indir, cwd), infiles[0])]
  if cmd[0].endswith('.py'):
    cmd.insert(0, sys.executable)

  # Only hashtable mode really needs the sha-1.
  dictfiles = process_inputs(indir, infiles, mode == 'hashtable', read_only)

  result = mode_fn(
      outdir, indir, dictfiles, read_only, cmd, relative_cwd, resultfile)

  if result == 0 and not no_save:
    # Saves the resulting file.
    out = {
      'command': cmd,
      'relative_cwd': relative_cwd,
      'files': dictfiles,
      'read_only': read_only,
    }
    with open(resultfile, 'wb') as f:
      json.dump(out, f, indent=2, sort_keys=True)
  return result


def MODEcheck(
    _outdir, _indir, _dictfiles, _read_only, _cmd, _relative_cwd, _resultfile):
  """No-op."""
  return 0


def MODEhashtable(
    outdir, indir, dictfiles, _read_only, _cmd, _relative_cwd, resultfile):
  outdir = outdir or os.path.dirname(resultfile)
  for relfile, properties in dictfiles.iteritems():
    infile = os.path.join(indir, relfile)
    outfile = os.path.join(outdir, properties['sha-1'])
    if os.path.isfile(outfile):
      # Just do a quick check that the file size matches.
      if os.stat(infile).st_size == os.stat(outfile).st_size:
        continue
      # Otherwise, an exception will be raised.
    tree_creator.link_file(outfile, infile, tree_creator.HARDLINK)
  return 0


def MODEremap(
    outdir, indir, dictfiles, read_only, _cmd, _relative_cwd, _resultfile):
  if not outdir:
    outdir = tempfile.mkdtemp(prefix='isolate')
  print 'Remapping into %s' % outdir
  if len(os.listdir(outdir)):
    print 'Can\'t remap in a non-empty directory'
    return 1
  recreate_tree(outdir, indir, dictfiles.keys(), tree_creator.HARDLINK)
  if read_only:
    tree_creator.make_writable(outdir, True)
  return 0


def MODErun(
    _outdir, indir, dictfiles, read_only, cmd, relative_cwd, _resultfile):
  """Always uses a temporary directory."""
  try:
    outdir = tempfile.mkdtemp(prefix='isolate')
    recreate_tree(outdir, indir, dictfiles.keys(), tree_creator.HARDLINK)
    cwd = os.path.join(outdir, relative_cwd)
    if not os.path.isdir(cwd):
      os.makedirs(cwd)
    if read_only:
      tree_creator.make_writable(outdir, True)

    logging.info('Running %s, cwd=%s' % (cmd, cwd))
    return subprocess.call(cmd, cwd=cwd)
  finally:
    tree_creator.rmtree(outdir)


def MODEtrace(
    _outdir, indir, _dictfiles, _read_only, cmd, relative_cwd, resultfile):
  """Shortcut to use trace_inputs.py properly.

  It constructs the equivalent of dictfiles. It is hardcoded to base the
  checkout at src/.
  """
  logging.info('Running %s, cwd=%s' % (cmd, os.path.join(indir, relative_cwd)))
  return trace_inputs.trace_inputs(
      '%s.log' % resultfile,
      cmd,
      indir,
      relative_cwd,
      os.path.dirname(resultfile),  # Guesswork here.
      False)


def get_valid_modes():
  """Returns the modes that can be used."""
  return sorted(
      i[4:] for i in dir(sys.modules[__name__]) if i.startswith('MODE'))


def main():
  valid_modes = get_valid_modes()
  parser = optparse.OptionParser(
      usage='%prog [options] [inputs] -- [command line]',
      description=sys.modules[__name__].__doc__)
  parser.allow_interspersed_args = False
  parser.format_description = lambda *_: parser.description
  parser.add_option(
      '-v', '--verbose', action='count', default=0, help='Use multiple times')
  parser.add_option(
      '--mode', choices=valid_modes,
      help='Determines the action to be taken: %s' % ', '.join(valid_modes))
  parser.add_option(
      '--result', metavar='FILE',
      help='File containing the json information about inputs')
  parser.add_option(
      '--root', metavar='DIR', help='Base directory to fetch files, required')
  parser.add_option(
      '--outdir', metavar='DIR',
      help='Directory used to recreate the tree or store the hash table. '
           'For run and remap, uses a /tmp subdirectory. For the other modes, '
           'defaults to the directory containing --result')
  parser.add_option(
      '--read-only', action='store_true', default=False,
      help='Make the temporary tree read-only')
  parser.add_option(
      '--from-results', action='store_true',
      help='Loads everything from the result file instead of generating it')
  parser.add_option(
      '--files', metavar='FILE',
      help='File to be read containing input files')

  options, args = parser.parse_args()
  level = [logging.ERROR, logging.INFO, logging.DEBUG][min(2, options.verbose)]
  logging.basicConfig(
      level=level,
      format='%(levelname)5s %(module)15s(%(lineno)3d): %(message)s')

  if not options.mode:
    parser.error('--mode is required')

  if not options.result:
    parser.error('--result is required.')
  if options.from_results:
    if not options.root:
      options.root = os.getcwd()
    if args:
      parser.error('Arguments cannot be used with --from-result')
    if options.files:
      parser.error('--files cannot be used with --from-result')
  else:
    if not options.root:
      parser.error('--root is required.')

  options.result = os.path.abspath(options.result)

  # Normalize the root input directory.
  indir = os.path.normpath(options.root)
  if not os.path.isdir(indir):
    parser.error('%s is not a directory' % indir)

  # Do not call abspath until it was verified the directory exists.
  indir = os.path.abspath(indir)

  logging.info('sys.argv: %s' % sys.argv)
  logging.info('cwd: %s' % os.getcwd())
  logging.info('Args: %s' % args)
  if not options.from_results:
    infiles, cmd = separate_inputs_command(args, indir, options.files)
    if not infiles:
      parser.error('Need at least one input file to map')
  else:
    data = json.load(open(options.result))
    cmd = data['command']
    infiles = data['files'].keys()
    os.chdir(data['relative_cwd'])

  logging.info('infiles: %s' % infiles)

  try:
    return isolate(
        options.outdir,
        options.result,
        indir,
        infiles,
        options.mode,
        options.read_only,
        cmd,
        options.from_results)
  except tree_creator.MappingError, e:
    print >> sys.stderr, str(e)
    return 1


if __name__ == '__main__':
  sys.exit(main())
