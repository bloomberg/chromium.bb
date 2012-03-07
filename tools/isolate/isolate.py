#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Does one of the following depending on the --mode argument:
  check   verify all the inputs exist, touches the file specified with
          --result and exits.
  run     recreates a tree with all the inputs files and run the executable
          in it.
  remap   stores all the inputs files in a directory without running the
          executable.
"""

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


def isolate(outdir, root, resultfile, mode, read_only, args):
  """Main function to isolate a target with its dependencies."""
  cmd = []
  if '--' in args:
    # Strip off the command line from the inputs.
    i = args.index('--')
    cmd = args[i+1:]
    args = args[:i]

  # gyp provides paths relative to cwd. Convert them to be relative to
  # root.
  cwd = os.getcwd()

  def make_relpath(i):
    """Makes an input file a relative path but keeps any trailing slash."""
    out = os.path.relpath(os.path.join(cwd, i), root)
    if i.endswith('/'):
      out += '/'
    return out

  infiles = [make_relpath(i) for i in args]

  if not infiles:
    raise ValueError('Need at least one input file to map')

  # Other modes ignore the command.
  if mode == 'run' and not cmd:
    print >> sys.stderr, 'Using first input %s as executable' % infiles[0]
    cmd = [infiles[0]]

  tempdir = None
  try:
    if not outdir and mode != 'check':
      tempdir = tempfile.mkdtemp(prefix='isolate')
      outdir = tempdir
    elif outdir:
      outdir = os.path.abspath(outdir)

    tree_creator.recreate_tree(
        outdir,
        os.path.abspath(root),
        infiles,
        tree_creator.DRY_RUN if mode == 'check' else tree_creator.HARDLINK,
        lambda x: re.match(r'.*\.(svn|pyc)$', x))

    if mode != 'check' and read_only:
      tree_creator.make_writable(outdir, True)

    if mode in ('check', 'remap'):
      result = 0
    else:
      # TODO(maruel): Remove me. Layering violation. Used by
      # base/base_paths_linux.cc
      env = os.environ.copy()
      env['CR_SOURCE_ROOT'] = outdir.encode()
      # Rebase the command to the right path.
      cmd[0] = os.path.join(outdir, cmd[0])
      logging.info('Running %s' % cmd)
      result = subprocess.call(cmd, cwd=outdir, env=env)

    if not result and resultfile:
      # Signal the build tool that the test succeeded.
      touch(resultfile)
    return result
  finally:
    if tempdir and mode == 'isolate':
      if read_only:
        tree_creator.make_writable(tempdir, False)
      rmtree(tempdir)


def main():
  parser = optparse.OptionParser(
      usage='%prog [options] [inputs] -- [command line]',
      description=sys.modules[__name__].__doc__)
  parser.allow_interspersed_args = False
  parser.format_description = lambda *_: parser.description
  parser.add_option(
      '-v', '--verbose', action='count', default=0, help='Use multiple times')
  parser.add_option(
      '--mode', choices=['remap', 'check', 'run'],
      help='Determines the action to be taken')
  parser.add_option(
      '--result', metavar='X',
      help='File to be touched when the command succeeds')
  parser.add_option('--root', help='Base directory to fetch files, required')
  parser.add_option(
      '--outdir', metavar='X',
      help='Directory used to recreate the tree. Defaults to a /tmp '
           'subdirectory')
  parser.add_option(
      '--read-only', action='store_true',
      help='Make the temporary tree read-only')

  options, args = parser.parse_args()
  level = [logging.ERROR, logging.INFO, logging.DEBUG][min(2, options.verbose)]
  logging.basicConfig(
      level=level,
      format='%(levelname)5s %(module)15s(%(lineno)3d): %(message)s')

  if not options.root:
    parser.error('--root is required.')

  try:
    return isolate(
        options.outdir,
        options.root,
        options.result,
        options.mode,
        options.read_only,
        args)
  except ValueError, e:
    parser.error(str(e))


if __name__ == '__main__':
  sys.exit(main())
