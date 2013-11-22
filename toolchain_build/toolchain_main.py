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


def PrintFlush(message):
  """Print to stdout and flush.

  Windows flushes stdout very intermittently (particularly through the
  buildbot). Forcing an immediate flush so that buildbot annotator section
  headings appear at the right place in relation to logging output which goes
  to stderr.
  """
  print message
  sys.stdout.flush()


def PrintAnnotatorURL(url):
  """Print an URL in buildbot annotator form.

  Args:
    url: A URL to print.
  """
  PrintFlush('@@@STEP_LINK@download@%s@@@' % url)


class PackageBuilder(object):
  """Module to build a setup of packages."""

  def __init__(self, packages, args):
    """Constructor.

    Args:
      packages: A dictionary with the following format:
        {
          '<package name>': {
            REPO_SRC_INFO,
            'commands':
              [<list of command.Command objects to run>],
            'dependencies':  # optional
              [<list of package depdenencies>],
            'unpack_commands':  # optional
              [<list of command.Command objects for unpacking inputs
                before they are hashed>'],
            'hashed_inputs':  # optional
              [<list of paths to use for build signature>],
          },
        }
        REPO_SRC_INFO is either:
         'git_url': '<git repo url>',
         'git_revision': '<git hex digest to sync>',
        OR:
         'tar_src': '<root relative path to source tarball>',
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

  def Main(self):
    """Main entry point."""
    if self._options.clobber:
      PrintFlush('@@@BUILD_STEP clobber@@@')
      file_tools.RemoveDirectoryIfPresent(self._options.source)
      file_tools.RemoveDirectoryIfPresent(self._options.output)
    self.SyncAll()
    self.BuildAll()

  def SetupLogging(self):
    """Setup python logging based on options."""
    if self._options.verbose:
      logging.getLogger().setLevel(logging.DEBUG)
    else:
      logging.getLogger().setLevel(logging.INFO)
    logging.basicConfig(format='%(levelname)s: %(message)s')

  def SyncPackageGitRepo(self, package):
    """Sync the git repo specified by a package.

    Args:
      package: Package name to sync.
    """

    PrintFlush('@@@BUILD_STEP sync %s@@@' % package)
    package_info = self._packages[package]
    url = package_info['git_url']
    revision = package_info['git_revision']
    destination = os.path.join(self._options.source, package)
    logging.info('Syncing %s...' % package)
    repo_tools.SyncGitRepo(url,
                           destination,
                           revision if self._options.pinned else None,
                           reclone=self._options.reclone)
    logging.info('Done syncing %s.' % package)

  def BuildPackage(self, package):
    """Build a single package.

    Assumes dependencies of the package have been built.
    Args:
      package: Package to build.
    """
    PrintFlush('@@@BUILD_STEP build %s@@@' % package)
    package_info = self._packages[package]
    dependencies = package_info.get('dependencies', [])
    # Collect a dict of all the inputs.
    inputs = {}
    # Add in either a tar source or a git source.
    if 'tar_src' in package_info:
      inputs['src'] = os.path.join(ROOT_DIR, package_info['tar_src'])
    else:
      inputs['src'] = os.path.join(self._options.source, package)
    # Add in each dependency by package name.
    for dependency in dependencies:
      inputs[dependency] = os.path.join(
          self._options.output, dependency + '_install')
    # Each package generates intermediate into output/<PACKAGE>_work.
    # Clobbered here explicitly.
    work_dir = os.path.join(self._options.output, package + '_work')
    file_tools.RemoveDirectoryIfPresent(work_dir)
    os.mkdir(work_dir)
    # Each package emits its output to output/<PACKAGE>_install.
    # Clobbered implicitly by Run().
    output = os.path.join(self._options.output, package + '_install')
    # A package may define an alternate set of inputs to be used for
    # computing the build signature. These are assumed to be in the working
    # directory.
    hashed_inputs = package_info.get('hashed_inputs')
    if hashed_inputs is not None:
      for key, value in hashed_inputs.iteritems():
        hashed_inputs[key] = os.path.join(work_dir, value)
    # Do it.
    self._build_once.Run(
        package, inputs, output,
        commands=package_info.get('commands', []),
        unpack_commands=package_info.get('unpack_commands', []),
        hashed_inputs=hashed_inputs,
        working_dir=work_dir)

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
    for target in self._targets:
      # Only packages using git repos need to be synced.
      if 'git_url' in self._packages[target]:
        self.SyncPackageGitRepo(target)

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
        help='Clobber source and output directories.')
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
        '--reclone', dest='reclone',
        default=False, action='store_true',
        help='Clone source trees from scratch.')
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
    options, targets = parser.parse_args(args)
    if options.trybot and options.buildbot:
      PrintFlush('ERROR: Tried to run with both --trybot and --buildbot.')
      sys.exit(1)
    if options.trybot or options.buildbot:
      options.verbose = True
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
