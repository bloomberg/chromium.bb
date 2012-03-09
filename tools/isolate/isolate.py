#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Does one of the following depending on the --mode argument:
  check     verify all the inputs exist, touches the file specified with
            --result and exits.
  hashtable puts a manifest file and hard links each of the inputs into the
            output directory.
  remap     stores all the inputs files in a directory without running the
            executable.
  run       recreates a tree with all the inputs files and run the executable
            in it.

See more information at
http://dev.chromium.org/developers/testing/isolated-testing
"""

import hashlib
import json
import logging
import optparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

import tree_creator

# Needs to be coherent with the file's docstring above.
VALID_MODES = ('check', 'hashtable', 'remap', 'run')


def touch(filename):
  """Implements the equivalent of the 'touch' command."""
  if not os.path.exists(filename):
    open(filename, 'a').close()
  os.utime(filename, None)


def rmtree(root):
  """Wrapper around shutil.rmtree() to retry automatically on Windows."""
  if sys.platform == 'win32':
    for i in range(3):
      try:
        shutil.rmtree(root)
        break
      except WindowsError:  # pylint: disable=E0602
        delay = (i+1)*2
        print >> sys.stderr, (
            'The test has subprocess outliving it. Sleep %d seconds.' % delay)
        time.sleep(delay)
  else:
    shutil.rmtree(root)


def relpath(path, root):
  """os.path.relpath() that keeps trailing slash."""
  out = os.path.relpath(path, root)
  if path.endswith('/'):
    out += '/'
  return out


def separate_inputs_command(args, root):
  """Strips off the command line from the inputs.

  gyp provides input paths relative to cwd. Convert them to be relative to root.
  """
  cmd = []
  if '--' in args:
    i = args.index('--')
    cmd = args[i+1:]
    args = args[:i]
  cwd = os.getcwd()
  return [relpath(os.path.join(cwd, arg), root) for arg in args], cmd


def isolate(outdir, resultfile, indir, infiles, mode, read_only, cmd):
  """Main function to isolate a target with its dependencies.

  It's behavior depends on |mode|.
  """
  if mode == 'run':
    return run(outdir, resultfile, indir, infiles, read_only, cmd)

  if mode == 'hashtable':
    return hashtable(outdir, resultfile, indir, infiles)

  assert mode in ('check', 'remap'), mode
  if mode == 'remap':
    if not outdir:
      outdir = tempfile.mkdtemp(prefix='isolate')
    tree_creator.recreate_tree(
        outdir, indir, infiles, tree_creator.HARDLINK)
    if read_only:
      tree_creator.make_writable(outdir, True)

  if resultfile:
    # Signal the build tool that the test succeeded.
    with open(resultfile, 'wb') as f:
      for infile in infiles:
        f.write(infile.encode('utf-8'))
        f.write('\n')


def run(outdir, resultfile, indir, infiles, read_only, cmd):
  """Implements the 'run' mode."""
  if not cmd:
    print >> sys.stderr, 'Using first input %s as executable' % infiles[0]
    cmd = [infiles[0]]
  outdir = None
  try:
    outdir = tempfile.mkdtemp(prefix='isolate')
    tree_creator.recreate_tree(
        outdir, indir, infiles, tree_creator.HARDLINK)
    if read_only:
      tree_creator.make_writable(outdir, True)

    # Rebase the command to the right path.
    cwd = os.path.join(outdir, os.path.relpath(os.getcwd(), indir))
    logging.info('Running %s, cwd=%s' % (cmd, cwd))
    result = subprocess.call(cmd, cwd=cwd)
    if not result and resultfile:
      # Signal the build tool that the test succeeded.
      touch(resultfile)
    return result
  finally:
    if read_only:
      tree_creator.make_writable(outdir, False)
    rmtree(outdir)


def hashtable(outdir, resultfile, indir, infiles):
  """Implements the 'hashtable' mode."""
  results = {}
  for relfile in infiles:
    infile = os.path.join(indir, relfile)
    h = hashlib.sha1()
    with open(infile, 'rb') as f:
      h.update(f.read())
    digest = h.hexdigest()
    outfile = os.path.join(outdir, digest)
    tree_creator.process_file(outfile, infile, tree_creator.HARDLINK)
    results[relfile] = {'sha1': digest}
  json.dump(
      {
        'files': results,
      },
      open(resultfile, 'wb'))


def main():
  parser = optparse.OptionParser(
      usage='%prog [options] [inputs] -- [command line]',
      description=sys.modules[__name__].__doc__)
  parser.allow_interspersed_args = False
  parser.format_description = lambda *_: parser.description
  parser.add_option(
      '-v', '--verbose', action='count', default=0, help='Use multiple times')
  parser.add_option(
      '--mode', choices=VALID_MODES,
      help='Determines the action to be taken: %s' % ', '.join(VALID_MODES))
  parser.add_option(
      '--result', metavar='FILE',
      help='File to be touched when the command succeeds')
  parser.add_option(
      '--root', metavar='DIR', help='Base directory to fetch files, required')
  parser.add_option(
      '--outdir', metavar='DIR',
      help='Directory used to recreate the tree or store the hash table. '
           'Defaults to a /tmp subdirectory for modes run and remap.')
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

  if options.files:
    args = [
        i.decode('utf-8')
        for i in open(options.files, 'rb').read().splitlines() if i
    ] + args

  infiles, cmd = separate_inputs_command(args, options.root)
  if not infiles:
    parser.error('Need at least one input file to map')
  # Preprocess the input files.
  try:
    infiles, root = tree_creator.preprocess_inputs(
          options.root, infiles, lambda x: re.match(r'.*\.(svn|pyc)$', x))
    return isolate(
        options.outdir,
        options.result,
        root,
        infiles,
        options.mode,
        options.read_only,
        cmd)
  except tree_creator.MappingError, e:
    print >> sys.stderr, str(e)
    return 1


if __name__ == '__main__':
  sys.exit(main())
