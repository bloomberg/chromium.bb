#!/usr/bin/env python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script manages the installed toolchains in the chroot.
"""

import copy
import json
import optparse
import os
import sys

from chromite.buildbot import constants
from chromite.buildbot import portage_utilities
from chromite.lib import cros_build_lib
from chromite.lib import osutils

if cros_build_lib.IsInsideChroot():
  # Only import portage after we've checked that we're inside the chroot.
  # Outside may not have portage, in which case the above may not happen.
  # We'll check in main() if the operation needs portage.
  import portage


EMERGE_CMD = os.path.join(constants.CHROMITE_BIN_DIR, 'parallel_emerge')
PACKAGE_STABLE = '[stable]'
PACKAGE_NONE = '[none]'
SRC_ROOT = os.path.realpath(constants.SOURCE_ROOT)

CHROMIUMOS_OVERLAY = '/usr/local/portage/chromiumos'
STABLE_OVERLAY = '/usr/local/portage/stable'
CROSSDEV_OVERLAY = '/usr/local/portage/crossdev'


# TODO: The versions are stored here very much like in setup_board.
# The goal for future is to differentiate these using a config file.
# This is done essentially by messing with GetDesiredPackageVersions()
DEFAULT_VERSION = PACKAGE_STABLE
DEFAULT_TARGET_VERSION_MAP = {
}
TARGET_VERSION_MAP = {
  'host' : {
    'binutils' : '2.21.1',
    'gdb' : PACKAGE_NONE,
  },
}
# Overrides for {gcc,binutils}-config, pick a package with particular suffix.
CONFIG_TARGET_SUFFIXES = {
  'binutils' : {
    'i686-pc-linux-gnu' : '-gold',
    'x86_64-cros-linux-gnu' : '-gold',
  },
}
# Global per-run cache that will be filled ondemand in by GetPackageMap()
# function as needed.
target_version_map = {
}


class Crossdev(object):
  """Class for interacting with crossdev and caching its output."""

  _CACHE_FILE = os.path.join(CROSSDEV_OVERLAY, '.configured.json')
  _CACHE = {}

  @classmethod
  def Load(cls, reconfig):
    """Load crossdev cache from disk."""
    crossdev_version = GetStablePackageVersion('sys-devel/crossdev', True)
    cls._CACHE = {'crossdev_version': crossdev_version}
    if os.path.exists(cls._CACHE_FILE) and not reconfig:
      with open(cls._CACHE_FILE) as f:
        data = json.load(f)
        if crossdev_version == data.get('crossdev_version'):
          cls._CACHE = data

  @classmethod
  def Save(cls):
    """Store crossdev cache on disk."""
    # Save the cache from the successful run.
    with open(cls._CACHE_FILE, 'w') as f:
      json.dump(cls._CACHE, f)

  @classmethod
  def GetConfig(cls, target):
    """Returns a map of crossdev provided variables about a tuple."""
    CACHE_ATTR = '_target_tuple_map'

    val = cls._CACHE.setdefault(CACHE_ATTR, {})
    if not target in val:
      # Find out the crossdev tuple.
      target_tuple = target
      if target == 'host':
        target_tuple = GetHostTuple()
      # Catch output of crossdev.
      out = cros_build_lib.RunCommand(['crossdev', '--show-target-cfg',
                                       '--ex-gdb', target_tuple],
                print_cmd=False, redirect_stdout=True).output.splitlines()
      # List of tuples split at the first '=', converted into dict.
      val[target] = dict([x.split('=', 1) for x in out])
    return val[target]

  @classmethod
  def UpdateTargets(cls, targets, usepkg, config_only=False):
    """Calls crossdev to initialize a cross target.

    Args:
      targets - the list of targets to initialize using crossdev
      usepkg - copies the commandline opts
      config_only - Just update
    """
    configured_targets = cls._CACHE.setdefault('configured_targets', [])

    cmdbase = ['crossdev', '--show-fail-log']
    cmdbase.extend(['--env', 'FEATURES=splitdebug'])
    # Pick stable by default, and override as necessary.
    cmdbase.extend(['-P', '--oneshot'])
    if usepkg:
      cmdbase.extend(['-P', '--getbinpkg',
                      '-P', '--usepkgonly',
                      '--without-headers'])

    overlays = '%s %s' % (CHROMIUMOS_OVERLAY, STABLE_OVERLAY)
    cmdbase.extend(['--overlays', overlays])
    cmdbase.extend(['--ov-output', CROSSDEV_OVERLAY])

    for target in targets:
      if config_only and target in configured_targets:
        continue

      cmd = cmdbase + ['-t', target]

      for pkg in GetTargetPackages(target):
        if pkg == 'gdb':
          # Gdb does not have selectable versions.
          cmd.append('--ex-gdb')
          continue
        # The first of the desired versions is the "primary" one.
        version = GetDesiredPackageVersions(target, pkg)[0]
        cmd.extend(['--%s' % pkg, version])

      cmd.extend(targets[target]['crossdev'].split())
      if config_only:
        # In this case we want to just quietly reinit
        cmd.append('--init-target')
        cros_build_lib.RunCommand(cmd, print_cmd=False, redirect_stdout=True)
      else:
        cros_build_lib.RunCommand(cmd)

      configured_targets.append(target)


def GetPackageMap(target):
  """Compiles a package map for the given target from the constants.

  Uses a cache in target_version_map, that is dynamically filled in as needed,
  since here everything is static data and the structuring is for ease of
  configurability only.

  args:
    target - the target for which to return a version map

  returns a map between packages and desired versions in internal format
  (using the PACKAGE_* constants)
  """
  if target in target_version_map:
    return target_version_map[target]

  # Start from copy of the global defaults.
  result = copy.copy(DEFAULT_TARGET_VERSION_MAP)

  for pkg in GetTargetPackages(target):
  # prefer any specific overrides
    if pkg in TARGET_VERSION_MAP.get(target, {}):
      result[pkg] = TARGET_VERSION_MAP[target][pkg]
    else:
      # finally, if not already set, set a sane default
      result.setdefault(pkg, DEFAULT_VERSION)
  target_version_map[target] = result
  return result


def GetHostTuple():
  """Returns compiler tuple for the host system."""
  return portage.settings['CHOST']


def GetTargetPackages(target):
  """Returns a list of packages for a given target."""
  conf = Crossdev.GetConfig(target)
  # Undesired packages are denoted by empty ${pkg}_pn variable.
  return [x for x in conf['crosspkgs'].strip("'").split() if conf[x+'_pn']]


# Portage helper functions:
def GetPortagePackage(target, package):
  """Returns a package name for the given target."""
  conf = Crossdev.GetConfig(target)
  # Portage category:
  if target == 'host':
    category = conf[package + '_category']
  else:
    category = conf['category']
  # Portage package:
  pn = conf[package + '_pn']
  # Final package name:
  assert(category)
  assert(pn)
  return '%s/%s' % (category, pn)


def IsPackageDisabled(target, package):
  """Returns if the given package is not used for the target."""
  return GetDesiredPackageVersions(target, package) == [PACKAGE_NONE]


def GetTuplesForOverlays(overlays):
  """Returns a set of tuples for a given set of overlays."""
  tuples = {}
  default_settings = {
      'sdk'      : True,
      'crossdev' : '',
      'default'  : False,
  }

  for overlay in overlays:
    config = os.path.join(overlay, 'toolchain.conf')
    if os.path.exists(config):
      first_tuple = None
      seen_default = False

      for line in osutils.ReadFile(config).splitlines():
        # Split by hash sign so that comments are ignored.
        # Then split the line to get the tuple and its options.
        line = line.split('#', 1)[0].split()

        if len(line) > 0:
          tuple = line[0]
          if not first_tuple:
            first_tuple = tuple
          if tuple not in tuples:
            tuples[tuple] = copy.copy(default_settings)
          if len(line) > 1:
            tuples[tuple].update(json.loads(' '.join(line[1:])))
            if tuples[tuple]['default']:
              seen_default = True

      # If the user has not explicitly marked a toolchain as default,
      # automatically select the first tuple that we saw in the conf.
      if not seen_default and first_tuple:
        tuples[first_tuple]['default'] = True

  return tuples


# Tree interface functions. They help with retrieving data about the current
# state of the tree:
def GetAllTargets():
  """Get the complete list of targets.

  returns the list of cross targets for the current tree
  """
  targets = GetToolchainsForBoard('all')

  # Remove the host target as that is not a cross-target. Replace with 'host'.
  del targets[GetHostTuple()]
  return targets


def GetToolchainsForBoard(board):
  """Get a list of toolchain tuples for a given board name

  returns the list of toolchain tuples for the given board
  """
  overlays = portage_utilities.FindOverlays(
      constants.BOTH_OVERLAYS, None if board in ('all', 'sdk') else board)
  toolchains = GetTuplesForOverlays(overlays)
  if board == 'sdk':
    toolchains = FilterToolchains(toolchains, 'sdk', True)
  return toolchains


def FilterToolchains(targets, key, value):
  """Filter out targets based on their attributes.

  args:
    targets - dict of toolchains
    key - metadata to examine
    value - expected value for metadata

  returns a dict where all targets whose metadata key does not match value
  have been deleted.
  """
  for target, metadata in targets.items():
    if metadata[key] != value:
      del targets[target]
  return targets


def GetInstalledPackageVersions(atom):
  """Extracts the list of current versions of a target, package pair.

  args:
    atom - the atom to operate on (e.g. sys-devel/gcc)

  returns the list of versions of the package currently installed.
  """
  versions = []
  for pkg in portage.db['/']['vartree'].dbapi.match(atom, use_cache=0):
    version = portage.versions.cpv_getversion(pkg)
    versions.append(version)
  return versions


def GetStablePackageVersion(atom, installed):
  """Extracts the current stable version for a given package.

  args:
    target, package - the target/package to operate on eg. i686-pc-linux-gnu,gcc
    installed - Whether we want installed packages or ebuilds

  returns a string containing the latest version.
  """
  pkgtype = 'vartree' if installed else 'porttree'
  cpv = portage.best(portage.db['/'][pkgtype].dbapi.match(atom, use_cache=0))
  return portage.versions.cpv_getversion(cpv) if cpv else None


def VersionListToNumeric(target, package, versions, installed):
  """Resolves keywords in a given version list for a particular package.

  Resolving means replacing PACKAGE_STABLE with the actual number.

  args:
    target, package - the target/package to operate on eg. i686-pc-linux-gnu,gcc
    versions - list of versions to resolve

  returns list of purely numeric versions equivalent to argument
  """
  resolved = []
  atom = GetPortagePackage(target, package)
  for version in versions:
    if version == PACKAGE_STABLE:
      resolved.append(GetStablePackageVersion(atom, installed))
    elif version != PACKAGE_NONE:
      resolved.append(version)
  return resolved


def GetDesiredPackageVersions(target, package):
  """Produces the list of desired versions for each target, package pair.

  The first version in the list is implicitly treated as primary, ie.
  the version that will be initialized by crossdev and selected.

  If the version is PACKAGE_STABLE, it really means the current version which
  is emerged by using the package atom with no particular version key.
  Since crossdev unmasks all packages by default, this will actually
  mean 'unstable' in most cases.

  args:
    target, package - the target/package to operate on eg. i686-pc-linux-gnu,gcc

  returns a list composed of either a version string, PACKAGE_STABLE
  """
  packagemap = GetPackageMap(target)

  versions = []
  if package in packagemap:
    versions.append(packagemap[package])

  return versions


def TargetIsInitialized(target):
  """Verifies if the given list of targets has been correctly initialized.

  This determines whether we have to call crossdev while emerging
  toolchain packages or can do it using emerge. Emerge is naturally
  preferred, because all packages can be updated in a single pass.

  args:
    targets - list of individual cross targets which are checked

  returns True if target is completely initialized
  returns False otherwise
  """
  # Check if packages for the given target all have a proper version.
  try:
    for package in GetTargetPackages(target):
      atom = GetPortagePackage(target, package)
      # Do we even want this package && is it initialized?
      if not IsPackageDisabled(target, package) and not (
          GetStablePackageVersion(atom, True) and
          GetStablePackageVersion(atom, False)):
        return False
    return True
  except cros_build_lib.RunCommandError:
    # Fails - The target has likely never been initialized before.
    return False


def RemovePackageMask(target):
  """Removes a package.mask file for the given platform.

  The pre-existing package.mask files can mess with the keywords.

  args:
    target - the target for which to remove the file
  """
  maskfile = os.path.join('/etc/portage/package.mask', 'cross-' + target)
  osutils.SafeUnlink(maskfile)


# Main functions performing the actual update steps.
def UpdateTargets(targets, usepkg):
  """Determines which packages need update/unmerge and defers to portage.

  args:
    targets - the list of targets to update
    usepkg - copies the commandline option
  """
  # Remove keyword files created by old versions of cros_setup_toolchains.
  osutils.SafeUnlink('/etc/portage/package.keywords/cross-host')

  # For each target, we do two things. Figure out the list of updates,
  # and figure out the appropriate keywords/masks. Crossdev will initialize
  # these, but they need to be regenerated on every update.
  print 'Determining required toolchain updates...'
  mergemap = {}
  for target in targets:
    # Record the highest needed version for each target, for masking purposes.
    RemovePackageMask(target)
    for package in GetTargetPackages(target):
      # Portage name for the package
      if IsPackageDisabled(target, package):
        continue
      pkg = GetPortagePackage(target, package)
      current = GetInstalledPackageVersions(pkg)
      desired = GetDesiredPackageVersions(target, package)
      desired_num = VersionListToNumeric(target, package, desired, False)
      mergemap[pkg] = set(desired_num).difference(current)

  packages = []
  for pkg in mergemap:
    for ver in mergemap[pkg]:
      if ver != PACKAGE_NONE:
        packages.append(pkg)

  if not packages:
    print 'Nothing to update!'
    return False

  print 'Updating packages:'
  print packages

  cmd = [EMERGE_CMD, '--oneshot', '--update']
  if usepkg:
    cmd.extend(['--getbinpkg', '--usepkgonly'])

  cmd.extend(packages)
  cros_build_lib.RunCommand(cmd)
  return True


def CleanTargets(targets):
  """Unmerges old packages that are assumed unnecessary."""
  unmergemap = {}
  for target in targets:
    for package in GetTargetPackages(target):
      if IsPackageDisabled(target, package):
        continue
      pkg = GetPortagePackage(target, package)
      current = GetInstalledPackageVersions(pkg)
      desired = GetDesiredPackageVersions(target, package)
      desired_num = VersionListToNumeric(target, package, desired, True)
      if not set(desired_num).issubset(current):
        print 'Some packages have been held back, skipping clean!'
        return
      unmergemap[pkg] = set(current).difference(desired_num)

  # Cleaning doesn't care about consistency and rebuilding package.* files.
  packages = []
  for pkg, vers in unmergemap.iteritems():
    packages.extend('=%s-%s' % (pkg, ver) for ver in vers if ver != '9999')

  if packages:
    print 'Cleaning packages:'
    print packages
    cmd = [EMERGE_CMD, '--unmerge']
    cmd.extend(packages)
    cros_build_lib.RunCommand(cmd)
  else:
    print 'Nothing to clean!'


def SelectActiveToolchains(targets, suffixes):
  """Runs gcc-config and binutils-config to select the desired.

  args:
    targets - the targets to select
  """
  for package in ['gcc', 'binutils']:
    for target in targets:
      # Pick the first version in the numbered list as the selected one.
      desired = GetDesiredPackageVersions(target, package)
      desired_num = VersionListToNumeric(target, package, desired, True)
      desired = desired_num[0]
      # *-config does not play revisions, strip them, keep just PV.
      desired = portage.versions.pkgsplit('%s-%s' % (package, desired))[1]

      if target == 'host':
        # *-config is the only tool treating host identically (by tuple).
        target = GetHostTuple()

      # And finally, attach target to it.
      desired = '%s-%s' % (target, desired)

      # Target specific hacks
      if package in suffixes:
        if target in suffixes[package]:
          desired += suffixes[package][target]

      extra_env = {'CHOST': target}
      cmd = ['%s-config' % package, '-c', target]
      current = cros_build_lib.RunCommand(cmd, print_cmd=False,
          redirect_stdout=True, extra_env=extra_env).output.splitlines()[0]
      # Do not gcc-config when the current is live or nothing needs to be done.
      if current != desired and current != '9999':
        cmd = [ package + '-config', desired ]
        cros_build_lib.RunCommand(cmd, print_cmd=False)


def UpdateToolchains(usepkg, deleteold, hostonly, reconfig,
                     targets_wanted, boards_wanted):
  """Performs all steps to create a synchronized toolchain enviroment.

  args:
    arguments correspond to the given commandline flags
  """
  targets, crossdev_targets, reconfig_targets = {}, {}, {}
  if not hostonly:
    # For hostonly, we can skip most of the below logic, much of which won't
    # work on bare systems where this is useful.
    alltargets = GetAllTargets()
    targets_wanted = set(targets_wanted)
    if targets_wanted == set(['all']):
      targets = alltargets
    elif targets_wanted == set(['sdk']):
      # Filter out all the non-sdk toolchains as we don't want to mess
      # with those in all of our builds.
      targets = FilterToolchains(alltargets, 'sdk', True)
    else:
      # Verify user input.
      nonexistant = []
      for target in targets_wanted:
        if target not in alltargets:
          nonexistant.append(target)
        else:
          targets[target] = alltargets[target]
      if nonexistant:
        cros_build_lib.Die('Invalid targets: ' + ','.join(nonexistant))

    # Now re-add any targets that might be from this board.  This is
    # to allow unofficial boards to declare their own toolchains.
    for board in boards_wanted:
      targets.update(GetToolchainsForBoard(board))

    # First check and initialize all cross targets that need to be.
    for target in targets:
      if TargetIsInitialized(target):
        reconfig_targets[target] = targets[target]
      else:
        crossdev_targets[target] = targets[target]
    if crossdev_targets:
      print 'The following targets need to be re-initialized:'
      print crossdev_targets
      Crossdev.UpdateTargets(crossdev_targets, usepkg)
    # Those that were not initialized may need a config update.
    Crossdev.UpdateTargets(reconfig_targets, usepkg, config_only=True)

  # We want host updated.
  targets['host'] = {}

  # Now update all packages.
  if UpdateTargets(targets, usepkg) or crossdev_targets or reconfig:
    SelectActiveToolchains(targets, CONFIG_TARGET_SUFFIXES)

  if deleteold:
    CleanTargets(targets)


def main(argv):
  usage = """usage: %prog [options]

  The script installs and updates the toolchains in your chroot.
  """
  parser = optparse.OptionParser(usage)
  parser.add_option('-u', '--nousepkg',
                    action='store_false', dest='usepkg', default=True,
                    help=('Use prebuilt packages if possible.'))
  parser.add_option('-d', '--deleteold',
                    action='store_true', dest='deleteold', default=False,
                    help=('Unmerge deprecated packages.'))
  parser.add_option('-t', '--targets',
                    dest='targets', default='sdk',
                    help=('Comma separated list of tuples. '
                          'Special keyword \'host\' is allowed. Default: sdk.'))
  parser.add_option('--include-boards',
                    dest='include_boards', default='',
                    help=('Comma separated list of boards whose toolchains we'
                          ' will always include. Default: none.'))
  parser.add_option('--hostonly',
                    dest='hostonly', default=False, action='store_true',
                    help=('Only setup the host toolchain. '
                          'Useful for bootstrapping chroot.'))
  parser.add_option('--show-board-cfg',
                    dest='board_cfg', default=None,
                    help=('Board to list toolchain tuples for'))
  parser.add_option('--reconfig', default=False, action='store_true',
                    help=('Reload crossdev config and reselect toolchains.'))

  (options, _remaining_arguments) = parser.parse_args(argv)

  if options.board_cfg:
    toolchains = GetToolchainsForBoard(options.board_cfg)
    # Make sure we display the default toolchain first.
    tuples = toolchains.keys()
    for tuple in tuples:
      if toolchains[tuple]['default']:
        tuples.remove(tuple)
        tuples.insert(0, tuple)
        break
    print ','.join(tuples)
    return 0

  cros_build_lib.AssertInsideChroot()

  # This has to be always run as root.
  if not os.getuid() == 0:
    print "%s: This script must be run as root!" % sys.argv[0]
    sys.exit(1)

  targets = set(options.targets.split(','))
  boards = set(options.include_boards.split(',')) if options.include_boards \
      else set()
  Crossdev.Load(options.reconfig)
  UpdateToolchains(options.usepkg, options.deleteold, options.hostonly,
                   options.reconfig, targets, boards)
  Crossdev.Save()
