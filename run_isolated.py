#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Reads a .isolated, creates a tree of hardlinks and runs the test.

To improve performance, it keeps a local cache. The local cache can safely be
deleted.

Any ${ISOLATED_OUTDIR} on the command line will be replaced by the location of a
temporary directory upon execution of the command specified in the .isolated
file. All content written to this directory will be uploaded upon termination
and the .isolated file describing this directory will be printed to stdout.
"""

__version__ = '0.4.1'

import logging
import optparse
import os
import sys
import tempfile

from third_party.depot_tools import fix_encoding

from utils import file_path
from utils import on_error
from utils import subprocess42
from utils import tools
from utils import zip_package

import auth
import isolated_format
import isolateserver


# Absolute path to this file (can be None if running from zip on Mac).
THIS_FILE_PATH = os.path.abspath(__file__) if __file__ else None

# Directory that contains this file (might be inside zip package).
BASE_DIR = os.path.dirname(THIS_FILE_PATH) if __file__ else None

# Directory that contains currently running script file.
if zip_package.get_main_script_path():
  MAIN_DIR = os.path.dirname(
      os.path.abspath(zip_package.get_main_script_path()))
else:
  # This happens when 'import run_isolated' is executed at the python
  # interactive prompt, in that case __file__ is undefined.
  MAIN_DIR = None

# The name of the log file to use.
RUN_ISOLATED_LOG_FILE = 'run_isolated.log'

# The name of the log to use for the run_test_cases.py command
RUN_TEST_CASES_LOG = 'run_test_cases.log'


def get_as_zip_package(executable=True):
  """Returns ZipPackage with this module and all its dependencies.

  If |executable| is True will store run_isolated.py as __main__.py so that
  zip package is directly executable be python.
  """
  # Building a zip package when running from another zip package is
  # unsupported and probably unneeded.
  assert not zip_package.is_zipped_module(sys.modules[__name__])
  assert THIS_FILE_PATH
  assert BASE_DIR
  package = zip_package.ZipPackage(root=BASE_DIR)
  package.add_python_file(THIS_FILE_PATH, '__main__.py' if executable else None)
  package.add_python_file(os.path.join(BASE_DIR, 'isolated_format.py'))
  package.add_python_file(os.path.join(BASE_DIR, 'isolateserver.py'))
  package.add_python_file(os.path.join(BASE_DIR, 'auth.py'))
  package.add_directory(os.path.join(BASE_DIR, 'third_party'))
  package.add_directory(os.path.join(BASE_DIR, 'utils'))
  return package


def make_temp_dir(prefix, root_dir):
  """Returns a temporary directory on the same file system as root_dir."""
  base_temp_dir = None
  if (root_dir and
      not file_path.is_same_filesystem(root_dir, tempfile.gettempdir())):
    base_temp_dir = os.path.dirname(root_dir)
  return tempfile.mkdtemp(prefix=prefix, dir=base_temp_dir)


def change_tree_read_only(rootdir, read_only):
  """Changes the tree read-only bits according to the read_only specification.

  The flag can be 0, 1 or 2, which will affect the possibility to modify files
  and create or delete files.
  """
  if read_only == 2:
    # Files and directories (except on Windows) are marked read only. This
    # inhibits modifying, creating or deleting files in the test directory,
    # except on Windows where creating and deleting files is still possible.
    file_path.make_tree_read_only(rootdir)
  elif read_only == 1:
    # Files are marked read only but not the directories. This inhibits
    # modifying files but creating or deleting files is still possible.
    file_path.make_tree_files_read_only(rootdir)
  elif read_only in (0, None):
    # Anything can be modified.
    # TODO(maruel): This is currently dangerous as long as DiskCache.touch()
    # is not yet changed to verify the hash of the content of the files it is
    # looking at, so that if a test modifies an input file, the file must be
    # deleted.
    file_path.make_tree_writeable(rootdir)
  else:
    raise ValueError(
        'change_tree_read_only(%s, %s): Unknown flag %s' %
        (rootdir, read_only, read_only))


def process_command(command, out_dir):
  """Replaces isolated specific variables in a command line."""
  filtered = []
  for arg in command:
    if '${ISOLATED_OUTDIR}' in arg:
      arg = arg.replace('${ISOLATED_OUTDIR}', out_dir).replace('/', os.sep)
    filtered.append(arg)
  return filtered


def run_tha_test(isolated_hash, storage, cache, leak_temp_dir, extra_args):
  """Downloads the dependencies in the cache, hardlinks them into a temporary
  directory and runs the executable from there.

  A temporary directory is created to hold the output files. The content inside
  this directory will be uploaded back to |storage| packaged as a .isolated
  file.

  Arguments:
    isolated_hash: the SHA-1 of the .isolated file that must be retrieved to
                   recreate the tree of files to run the target executable.
    storage: an isolateserver.Storage object to retrieve remote objects. This
             object has a reference to an isolateserver.StorageApi, which does
             the actual I/O.
    cache: an isolateserver.LocalCache to keep from retrieving the same objects
           constantly by caching the objects retrieved. Can be on-disk or
           in-memory.
    leak_temp_dir: if true, the temporary directory will be deliberately leaked
                   for later examination.
    extra_args: optional arguments to add to the command stated in the .isolate
                file.
  """
  run_dir = make_temp_dir('run_tha_test', cache.cache_dir)
  out_dir = unicode(make_temp_dir('isolated_out', cache.cache_dir))
  result = 0
  try:
    try:
      bundle = isolateserver.fetch_isolated(
          isolated_hash=isolated_hash,
          storage=storage,
          cache=cache,
          outdir=run_dir,
          require_command=True)
    except isolated_format.IsolatedError:
      on_error.report(None)
      return 1

    change_tree_read_only(run_dir, bundle.read_only)
    cwd = os.path.normpath(os.path.join(run_dir, bundle.relative_cwd))
    command = bundle.command + extra_args

    file_path.ensure_command_has_abs_path(command, cwd)
    command = process_command(command, out_dir)
    logging.info('Running %s, cwd=%s' % (command, cwd))

    # TODO(csharp): This should be specified somewhere else.
    # TODO(vadimsh): Pass it via 'env_vars' in manifest.
    # Add a rotating log file if one doesn't already exist.
    env = os.environ.copy()
    if MAIN_DIR:
      env.setdefault('RUN_TEST_CASES_LOG_FILE',
          os.path.join(MAIN_DIR, RUN_TEST_CASES_LOG))
    sys.stdout.flush()
    with tools.Profiler('RunTest'):
      try:
        with subprocess42.Popen_with_handler(command, cwd=cwd, env=env) as p:
          p.communicate()
          result = p.returncode
      except OSError:
        on_error.report('Failed to run %s; cwd=%s' % (command, cwd))
        result = 1
    logging.info(
        'Command finished with exit code %d (%s)',
        result, hex(0xffffffff & result))
  finally:
    try:
      if leak_temp_dir:
        logging.warning('Deliberately leaking %s for later examination',
                        run_dir)
      else:
        try:
          if not file_path.rmtree(run_dir):
            print >> sys.stderr, (
                'Failed to delete the temporary directory, forcibly failing\n'
                'the task because of it. No zombie process can outlive a\n'
                'successful task run and still be marked as successful.\n'
                'Fix your stuff.')
            result = result or 1
        except OSError:
          logging.warning('Leaking %s', run_dir)
          result = 1

      # HACK(vadimsh): On Windows rmtree(run_dir) call above has
      # a synchronization effect: it finishes only when all task child processes
      # terminate (since a running process locks *.exe file). Examine out_dir
      # only after that call completes (since child processes may
      # write to out_dir too and we need to wait for them to finish).

      # Upload out_dir and generate a .isolated file out of this directory.
      # It is only done if files were written in the directory.
      if os.path.isdir(out_dir) and os.listdir(out_dir):
        with tools.Profiler('ArchiveOutput'):
          results = isolateserver.archive_files_to_storage(
              storage, [out_dir], None)
        # TODO(maruel): Implement side-channel to publish this information.
        output_data = {
          'hash': results[0][0],
          'namespace': storage.namespace,
          'storage': storage.location,
        }
        sys.stdout.flush()
        print(
            '[run_isolated_out_hack]%s[/run_isolated_out_hack]' %
            tools.format_json(output_data, dense=True))

    finally:
      try:
        if os.path.isdir(out_dir) and not file_path.rmtree(out_dir):
          result = result or 1
      except OSError:
        # The error was already printed out. Report it but that's it. Only
        # report on non-Windows or on Windows when the process had succeeded.
        # Due to the way file sharing works on Windows, it's sadly expected that
        # file deletion may fail when a test failed.
        if sys.platform != 'win32' or not result:
          on_error.report(None)
        result = 1

  return result


def main(args):
  tools.disable_buffering()
  parser = tools.OptionParserWithLogging(
      usage='%prog <options>',
      version=__version__,
      log_file=RUN_ISOLATED_LOG_FILE)

  data_group = optparse.OptionGroup(parser, 'Data source')
  data_group.add_option(
      '-s', '--isolated',
      help='Hash of the .isolated to grab from the isolate server')
  data_group.add_option(
      '-H', dest='isolated', help=optparse.SUPPRESS_HELP)
  isolateserver.add_isolate_server_options(data_group)
  parser.add_option_group(data_group)

  isolateserver.add_cache_options(parser)
  parser.set_defaults(cache='cache')

  debug_group = optparse.OptionGroup(parser, 'Debugging')
  debug_group.add_option(
      '--leak-temp-dir',
      action='store_true',
      help='Deliberately leak isolate\'s temp dir for later examination '
          '[default: %default]')
  parser.add_option_group(debug_group)

  auth.add_auth_options(parser)
  options, args = parser.parse_args(args)
  if not options.isolated:
    parser.error('--isolated is required.')
  auth.process_auth_options(parser, options)
  isolateserver.process_isolate_server_options(parser, options, True)

  cache = isolateserver.process_cache_options(options)
  with isolateserver.get_storage(
      options.isolate_server, options.namespace) as storage:
    # Hashing schemes used by |storage| and |cache| MUST match.
    assert storage.hash_algo == cache.hash_algo
    return run_tha_test(
        options.isolated, storage, cache, options.leak_temp_dir, args)


if __name__ == '__main__':
  # Ensure that we are always running with the correct encoding.
  fix_encoding.fix_encoding()
  sys.exit(main(sys.argv[1:]))
