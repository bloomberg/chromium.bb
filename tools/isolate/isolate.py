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

See more information at
http://dev.chromium.org/developers/testing/isolated-testing
"""

import json
import logging
import optparse
import os
import re
import subprocess
import sys
import tempfile

import tree_creator


def relpath(path, root):
  """os.path.relpath() that keeps trailing slash."""
  out = os.path.relpath(path, root)
  if path.endswith('/'):
    out += '/'
  return out


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


def isolate(outdir, resultfile, indir, infiles, mode, read_only, cmd):
  """Main function to isolate a target with its dependencies.

  It's behavior depends on |mode|.
  """
  mode_fn = getattr(sys.modules[__name__], 'MODE' + mode)
  assert mode_fn

  infiles = tree_creator.expand_directories(
        indir, infiles, lambda x: re.match(r'.*\.(svn|pyc)$', x))

  # Note the relative current directory.
  # In general, this path will be the path containing the gyp file where the
  # target was defined. This relative directory may be created implicitely if a
  # file from this directory is needed to run the test. Otherwise it won't be
  # created and the process creation will fail. It's up to the caller to create
  # this directory manually before starting the test.
  relative_cwd = os.path.relpath(os.getcwd(), indir)

  if not cmd:
    # Note that it is exactly the reverse of relative_cwd.
    cmd = [os.path.join(os.path.relpath(indir, os.getcwd()), infiles[0])]
  if cmd[0].endswith('.py'):
    cmd.insert(0, sys.executable)

  # Only hashtable mode really needs the sha-1.
  dictfiles = tree_creator.process_inputs(
      indir, infiles, mode == 'hashtable', read_only)

  if not outdir:
    outdir = os.path.dirname(resultfile)
  result = mode_fn(outdir, indir, dictfiles, read_only, cmd, relative_cwd)

  if result == 0:
    # Saves the resulting file.
    out = {
      'command': cmd,
      'relative_cwd': relative_cwd,
      'files': dictfiles,
    }
    with open(resultfile, 'wb') as f:
      json.dump(out, f, indent=2, sort_keys=True)
  return result


def MODEcheck(outdir, indir, dictfiles, read_only, cmd, relative_cwd):
  """No-op."""
  return 0


def MODEhashtable(outdir, indir, dictfiles, read_only, cmd, relative_cwd):
  """Ignores read_only, cmd and relative_cwd."""
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


def MODEremap(outdir, indir, dictfiles, read_only, cmd, relative_cwd):
  """Ignores cmd and relative_cwd."""
  if not outdir:
    outdir = tempfile.mkdtemp(prefix='isolate')
  tree_creator.recreate_tree(
      outdir, indir, dictfiles.keys(), tree_creator.HARDLINK)
  if read_only:
    tree_creator.make_writable(outdir, True)
  return 0


def MODErun(outdir, indir, dictfiles, read_only, cmd, relative_cwd):
  """Ignores outdir and always uses a temporary directory."""
  outdir = None
  try:
    outdir = tempfile.mkdtemp(prefix='isolate')
    tree_creator.recreate_tree(
        outdir, indir, dictfiles.keys(), tree_creator.HARDLINK)
    cwd = os.path.join(outdir, relative_cwd)
    if not os.path.isdir(cwd):
      os.makedirs(cwd)
    if read_only:
      tree_creator.make_writable(outdir, True)

    logging.info('Running %s, cwd=%s' % (cmd, cwd))
    return subprocess.call(cmd, cwd=cwd)
  finally:
    if read_only:
      tree_creator.make_writable(outdir, False)
    tree_creator.rmtree(outdir)


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
      help='Output file containing the json information about inputs')
  parser.add_option(
      '--root', metavar='DIR', help='Base directory to fetch files, required')
  parser.add_option(
      '--outdir', metavar='DIR',
      help='Directory used to recreate the tree or store the hash table. '
           'For run and remap, uses a /tmp subdirectory. For the other modes, '
           'defaults to the directory containing --result')
  parser.add_option(
      '--read-only', action='store_true',
      help='Make the temporary tree read-only')
  parser.add_option(
      '--files', metavar='FILE',
      help='File to be read containing input files')

  options, args = parser.parse_args()
  level = [logging.ERROR, logging.INFO, logging.DEBUG][min(2, options.verbose)]
  logging.basicConfig(
      level=level,
      format='%(levelname)5s %(module)15s(%(lineno)3d): %(message)s')

  if not options.root:
    parser.error('--root is required.')
  if not options.result:
    parser.error('--result is required.')

  # Normalize the root input directory.
  indir = os.path.normpath(options.root)
  if not os.path.isdir(indir):
    parser.error('%s is not a directory' % indir)

  # Do not call abspath until it was verified the directory exists.
  indir = os.path.abspath(indir)

  logging.info('sys.argv: %s' % sys.argv)
  logging.info('cwd: %s' % os.getcwd())
  logging.info('Args: %s' % args)
  infiles, cmd = separate_inputs_command(args, indir, options.files)
  if not infiles:
    parser.error('Need at least one input file to map')
  logging.info('infiles: %s' % infiles)

  try:
    return isolate(
        options.outdir,
        os.path.abspath(options.result),
        indir,
        infiles,
        options.mode,
        options.read_only,
        cmd)
  except tree_creator.MappingError, e:
    print >> sys.stderr, str(e)
    return 1


if __name__ == '__main__':
  sys.exit(main())
