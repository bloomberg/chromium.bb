#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build NativeClient toolchain packages."""

# Done first to setup python module path.
import toolchain_env

import logging
import optparse
import os
import sys
import textwrap

import file_tools
import gsd_storage
import log_tools
import once
import repo_tools
import local_storage_cache


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)
ROOT_DIR = os.path.dirname(NACL_DIR)

DEFAULT_CACHE_DIR = os.path.join(SCRIPT_DIR, 'cache')
DEFAULT_SRC_DIR = os.path.join(SCRIPT_DIR, 'src')
DEFAULT_OUT_DIR = os.path.join(SCRIPT_DIR, 'out')


def PrintAnnotatorURL(url):
  """Print an URL in buildbot annotator form.

  Args:
    url: A URL to print.
  """
  print >>sys.stderr, '@@@STEP_LINK@download@%s@@@' % url


class PackageBuilder(object):
  """Module to build a setup of packages."""

  def __init__(self, packages, args):
    """Constructor.

    Args:
      packages: A dictionary with the following format. There are two types of
                packages: source and build (described below).
        {
          '<package name>': {
            'type': 'source',
                # Source packages are for sources; in particular remote sources
                # where it is not known whether they have changed until they are
                # synced (it can also or for tarballs which need to be
                # unpacked). Source package commands are run unconditionally
                # unless sync is skipped via the command-line option. Source
                # package contents are not memoized.
            'dependencies':  # optional
              [<list of package depdenencies>],
            'output_dirname': # optional
              '<directory name>', # Name of the directory to checkout sources
              # into (a subdirectory of the global source directory); defaults
              # to the package name.
            'commands':
              [<list of command.Runnable objects to run>],
            'inputs': # optional
              {<mapping whose keys are names, and whose values are files or
                directories (e.g. checked-in tarballs) used as input. Since
                source targets are unconditional, this is only useful as a
                convenience for commands, which may refer to the inputs by their
                key name>},
           },
           '<package name>': {
            'type': 'build',
                # Build packages are memoized, and will build only if their
                # inputs have changed. Their inputs consist of the output of
                # their package dependencies plus any file or directory inputs
                # given by their 'inputs' member
            'dependencies':  # optional
              [<list of package depdenencies>],
            'inputs': # optional
              {<mapping whose keys are names, and whose values are files or
                directories (e.g. checked-in tarballs) used as input>},
            'commands':
              [<list of command.Command objects to run>],
              # Objects that have a 'skip_for_incremental' attribute that
              # evaluates to True will not be run on incremental builds unless
              # the working directory is empty.
          },
        }
      args: sys.argv[1:] or equivalent.
    """
    self._packages = packages
    self.DecodeArgs(packages, args)
    self.SetupLogging()
    self._build_once = once.Once(
        use_cached_results=self._options.use_cached_results,
        cache_results=self._options.cache_results,
        print_url=PrintAnnotatorURL,
        storage=self.CreateStorage())
    self._signature_file = None
    if self._options.emit_signatures is not None:
      if self._options.emit_signatures == '-':
        self._signature_file = sys.stdout
      else:
        self._signature_file = open(self._options.emit_signatures, 'w')

  def Main(self):
    """Main entry point."""
    if self._options.sync_sources:
      self.SyncAll()
    self.BuildAll()

  def SetupLogging(self):
    """Setup python logging based on options."""
    if self._options.verbose:
      logging.getLogger().setLevel(logging.DEBUG)
    else:
      logging.getLogger().setLevel(logging.INFO)
    logging.basicConfig(format='%(levelname)s: %(message)s')

  def GetOutputDir(self, package):
    # The output dir of source packages is in the source directory, and can be
    # overridden.
    if self._packages[package]['type'] == 'source':
      dirname = self._packages[package].get('output_dirname', package)
      return os.path.join(self._options.source, dirname)
    return os.path.join(self._options.output, package + '_install')

  def BuildPackage(self, package):
    """Build a single package.

    Assumes dependencies of the package have been built.
    Args:
      package: Package to build.
    """

    package_info = self._packages[package]

    # Validate the package description.
    if 'type' not in package_info:
      raise Exception('package %s does not have a type' % package)
    type_text = package_info['type']
    if type_text not in ('source', 'build'):
      raise Execption('package %s has unrecognized type: %s' %
                      (package, type_text))
    is_source_target = type_text == 'source'

    if 'commands' not in package_info:
      raise Exception('package %s does not have any commands' % package)

    # Source targets do not run when skipping sync.
    if is_source_target and not self._options.sync_sources:
      logging.debug('Sync skipped: not running commands for %s' % package)
      return

    print >>sys.stderr, '@@@BUILD_STEP %s (%s)@@@' % (package, type_text)

    dependencies = package_info.get('dependencies', [])

    # Collect a dict of all the inputs.
    inputs = {}
    # Add in explicit inputs.
    if 'inputs' in package_info:
      for key, value in package_info['inputs'].iteritems():
        if key in dependencies:
          raise Exception('key "%s" found in both dependencies and inputs of '
                          'package "%s"' % (key, package))
        inputs[key] = value
    else:
      inputs['src'] = os.path.join(self._options.source, package)
    # Add in each dependency by package name.
    for dependency in dependencies:
      inputs[dependency] = self.GetOutputDir(dependency)

    # Each package generates intermediate into output/<PACKAGE>_work.
    # Clobbered here explicitly.
    work_dir = os.path.join(self._options.output, package + '_work')
    if self._options.clobber:
      logging.debug('Clobbering working directory %s' % work_dir)
      file_tools.RemoveDirectoryIfPresent(work_dir)
    file_tools.MakeDirectoryIfAbsent(work_dir)

    output = self.GetOutputDir(package)

    if not is_source_target or self._options.clobber_source:
      logging.debug('Clobbering output directory %s' % output)
      file_tools.RemoveDirectoryIfPresent(output)
      os.mkdir(output)

    commands = package_info.get('commands', [])
    if not self._options.clobber and len(os.listdir(work_dir)) > 0:
      commands = [cmd for cmd in commands if
                  not (hasattr(cmd, 'skip_for_incremental') and
                       cmd.skip_for_incremental)]
    # Do it.
    self._build_once.Run(
        package, inputs, output,
        commands=commands,
        working_dir=work_dir,
        memoize=not is_source_target,
        signature_file=self._signature_file)

  def BuildOrder(self, targets):
    """Find what needs to be built in what order to build all targets.

    Args:
      targets: A list of target packages to build.
    Returns:
      A topologically sorted list of the targets plus their transitive
      dependencies, in an order that will allow things to be built.
    """
    order = []
    order_set = set()
    def Add(target, target_path):
      if target in order_set:
        return
      if target not in self._packages:
        raise Exception('Unknown package %s' % target)
      next_target_path = target_path + [target]
      if target in target_path:
        raise Exception('Dependency cycle: %s' % ' -> '.join(next_target_path))
      for dependency in self._packages[target].get('dependencies', []):
        Add(dependency, next_target_path)
      order.append(target)
      order_set.add(target)
    for target in targets:
      Add(target, [])
    return order

  def SyncAll(self):
    """Sync all packages selected and their dependencies."""
    file_tools.MakeDirectoryIfAbsent(self._options.source)

  def BuildAll(self):
    """Build all packages selected and their dependencies."""
    file_tools.MakeDirectoryIfAbsent(self._options.output)
    for target in self._targets:
      self.BuildPackage(target)

  def DecodeArgs(self, packages, args):
    """Decode command line arguments to this build.

    Populated self._options and self._targets.
    Args:
      packages: A list of package names to build.
      args: sys.argv[1:] or equivalent.
    """
    package_list = sorted(packages.keys())
    parser = optparse.OptionParser(
        usage='USAGE: %prog [options] [targets...]\n\n'
              'Available targets:\n' +
              '\n'.join(textwrap.wrap(' '.join(package_list))))
    parser.add_option(
        '-v', '--verbose', dest='verbose',
        default=False, action='store_true',
        help='Produce more output.')
    parser.add_option(
        '-c', '--clobber', dest='clobber',
        default=False, action='store_true',
        help='Clobber working directories before building.')
    parser.add_option(
        '--cache', dest='cache',
        default=DEFAULT_CACHE_DIR,
        help='Select directory containing local storage cache.')
    parser.add_option(
        '-s', '--source', dest='source',
        default=DEFAULT_SRC_DIR,
        help='Select directory containing source checkouts.')
    parser.add_option(
        '-o', '--output', dest='output',
        default=DEFAULT_OUT_DIR,
        help='Select directory containing build output.')
    parser.add_option(
        '--no-use-cached-results', dest='use_cached_results',
        default=True, action='store_false',
        help='Do not rely on cached results.')
    parser.add_option(
        '--no-use-remote-cache', dest='use_remote_cache',
        default=True, action='store_false',
        help='Do not rely on non-local cached results.')
    parser.add_option(
        '--no-cache-results', dest='cache_results',
        default=True, action='store_false',
        help='Do not cache results.')
    parser.add_option(
        '--no-pinned', dest='pinned',
        default=True, action='store_false',
        help='Do not use pinned revisions.')
    parser.add_option(
        '--trybot', dest='trybot',
        default=False, action='store_true',
        help='Run and cache as if on trybot.')
    parser.add_option(
        '--buildbot', dest='buildbot',
        default=False, action='store_true',
        help='Run and cache as if on a non-trybot buildbot.')
    parser.add_option(
        '--clobber-source', dest='clobber_source',
        default=False, action='store_true',
        help='Clobber source directories before building')
    parser.add_option(
        '-y', '--sync', dest='sync_sources',
        default=False, action='store_true',
        help='Run source target commands')
    parser.add_option(
        '--emit-signatures', dest='emit_signatures',
        help='Write human readable build signature for each step to FILE.',
        metavar='FILE')
    options, targets = parser.parse_args(args)
    if options.trybot and options.buildbot:
      print >>sys.stderr, (
          'ERROR: Tried to run with both --trybot and --buildbot.')
      sys.exit(1)
    if options.trybot or options.buildbot:
      options.verbose = True
      options.sync_sources = True
      options.clobber = True
    if not targets:
      targets = sorted(packages.keys())
    targets = self.BuildOrder(targets)
    self._options = options
    self._targets = targets

  def CreateStorage(self):
    """Create a storage object for this build.

    Returns:
      A storage object (GSDStorage).
    """
    if self._options.buildbot:
      return gsd_storage.GSDStorage(
          write_bucket='nativeclient-once',
          read_buckets=['nativeclient-once'])
    elif self._options.trybot:
      return gsd_storage.GSDStorage(
          write_bucket='nativeclient-once-try',
          read_buckets=['nativeclient-once', 'nativeclient-once-try'])
    else:
      read_buckets = []
      if self._options.use_remote_cache:
        read_buckets += ['nativeclient-once']
      return local_storage_cache.LocalStorageCache(
          cache_path=self._options.cache,
          storage=gsd_storage.GSDStorage(
              write_bucket=None,
              read_buckets=read_buckets))
