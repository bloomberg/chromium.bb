#!/usr/bin/env python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script manages the installed toolchains in the chroot.
"""

import copy
import errno
import optparse
import os
import sys

from chromite.buildbot import constants
from chromite.lib import cros_build_lib

# Some sanity checks first.
if not cros_build_lib.IsInsideChroot():
  print '%s: This needs to be run inside the chroot' % sys.argv[0]
  sys.exit(1)
# Only import portage after we've checked that we're inside the chroot.
# Outside may not have portage, in which case the above may not happen.
import portage


EMERGE_CMD = os.path.join(constants.CHROMITE_BIN_DIR, 'parallel_emerge')
CROS_OVERLAY_LIST_CMD = os.path.join(
    constants.SOURCE_ROOT, 'src/platform/dev/host/cros_overlay_list')
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


# Global variable cache. It has to be a descendant of 'object', rather than
# instance thereof, because attributes cannot be set on 'object' instances.
class VariableCache(object):
  pass
VAR_CACHE = VariableCache()


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
  """Returns compiler tuple for the host system.

  Caches the result, because the command can be fairly expensive, and never
  changes throughout a single run.
  """
  CACHE_ATTR = '_host_tuple'

  val = getattr(VAR_CACHE, CACHE_ATTR, None)
  if val is None:
    val = portage.settings['CHOST']
    setattr(VAR_CACHE, CACHE_ATTR, val)
  return val


def GetCrossdevConf(target):
  """Returns a map of crossdev provided variables about a tuple."""
  CACHE_ATTR = '_target_tuple_map'

  val = getattr(VAR_CACHE, CACHE_ATTR, {})
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
    setattr(VAR_CACHE, CACHE_ATTR, {})
  return val[target]


def GetTargetPackages(target):
  """Returns a list of packages for a given target."""
  conf = GetCrossdevConf(target)
  # Undesired packages are denoted by empty ${pkg}_pn variable.
  return [x for x in conf['crosspkgs'].strip("'").split() if conf[x+'_pn']]


# Portage helper functions:
def GetPortagePackage(target, package):
  """Returns a package name for the given target."""
  conf = GetCrossdevConf(target)
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


def GetPortageKeyword(target):
  """Returns a portage friendly keyword for a given target."""
  return GetCrossdevConf(target)['arch']


def IsPackageDisabled(target, package):
  """Returns if the given package is not used for the target."""
  return GetDesiredPackageVersions(target, package) == [PACKAGE_NONE]

# Tree interface functions. They help with retrieving data about the current
# state of the tree:
def GetAllTargets():
  """Get the complete list of targets.

  returns the list of cross targets for the current tree
  """
  cmd = [CROS_OVERLAY_LIST_CMD, '--all_boards']
  overlays = cros_build_lib.RunCommand(cmd, print_cmd=False,
                                       redirect_stdout=True).output.splitlines()
  targets = set()
  for overlay in overlays:
    config = os.path.join(overlay, 'toolchain.conf')
    if os.path.exists(config):
      with open(config) as config_file:
        lines = config_file.read().splitlines()
        for line in lines:
          line = line.split('#', 1)[0]
          targets.update(line.split())

  # Remove the host target as that is not a cross-target. Replace with 'host'.
  targets.discard(GetHostTuple())
  return targets


def GetInstalledPackageVersions(target, package):
  """Extracts the list of current versions of a target, package pair.

  args:
    target, package - the target/package to operate on eg. i686-pc-linux-gnu,gcc

  returns the list of versions of the package currently installed.
  """
  versions = []
  # This is the package name in terms of portage.
  atom = GetPortagePackage(target, package)
  for pkg in portage.db['/']['vartree'].dbapi.match(atom):
    version = portage.versions.cpv_getversion(pkg)
    versions.append(version)
  return versions


def GetStablePackageVersion(target, package, installed):
  """Extracts the current stable version for a given package.

  args:
    target, package - the target/package to operate on eg. i686-pc-linux-gnu,gcc
    installed - Whether we want installed packages or ebuilds

  returns a string containing the latest version.
  """
  def mass_portageq_splitline(entry):
    """Splits the output of mass_best_visible into package:version tuple."""
    # mass_best_visible returns lines of the format "package:cpv"
    split_string = entry.split(':', 1)
    if split_string[1]:
      split_string[1] = portage.versions.cpv_getversion(split_string[1])
    return split_string

  CACHE_ATTR = '_target_stable_map'

  pkgtype = "installed" if installed else "ebuild"

  val = getattr(VAR_CACHE, CACHE_ATTR, {})
  if not target in val:
    val[target] = {}
  if not pkgtype in val[target]:
    keyword = GetPortageKeyword(target)
    extra_env = {'ACCEPT_KEYWORDS' : '-* ' + keyword}
    # Evaluate all packages for a target in one swoop, because it's much faster.
    pkgs = [GetPortagePackage(target, p) for p in GetTargetPackages(target)]
    cmd = ['portageq', 'mass_best_visible', '/', pkgtype] + pkgs
    cpvs = cros_build_lib.RunCommand(cmd,
                                     print_cmd=False, redirect_stdout=True,
                                     extra_env=extra_env).output.splitlines()
    val[target][pkgtype] = dict(map(mass_portageq_splitline, cpvs))
    setattr(VAR_CACHE, CACHE_ATTR, val)

  return val[target][pkgtype][GetPortagePackage(target, package)]


def VersionListToNumeric(target, package, versions, installed):
  """Resolves keywords in a given version list for a particular package.

  Resolving means replacing PACKAGE_STABLE with the actual number.

  args:
    target, package - the target/package to operate on eg. i686-pc-linux-gnu,gcc
    versions - list of versions to resolve

  returns list of purely numeric versions equivalent to argument
  """
  resolved = []
  for version in versions:
    if version == PACKAGE_STABLE:
      resolved.append(GetStablePackageVersion(target, package, installed))
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
      # Do we even want this package && is it initialized?
      if not IsPackageDisabled(target, package) and \
          not GetInstalledPackageVersions(target, package):
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
  try:
    os.unlink(maskfile)
  except EnvironmentError as e:
    if e.errno != errno.ENOENT:
      raise


def CreatePackageMask(target, masks):
  """[Re]creates a package.mask file for the given platform.

  args:
    target - the given target on which to operate
    masks - a map of package : version,
        where version is the highest permissible version (mask >)
  """
  maskfile = os.path.join('/etc/portage/package.mask', 'cross-' + target)
  assert not os.path.exists(maskfile)
  cros_build_lib.SafeMakedirs(os.path.dirname(maskfile))

  with open(maskfile, 'w') as f:
    for pkg, m in masks.items():
      f.write('>%s-%s\n' % (pkg, m))


def CreatePackageKeywords(target):
  """[Re]create a package.keywords file for the platform.

  This sets all package.keywords files to unmask all stable/testing packages.
  TODO: Note that this approach should be deprecated and is only done for
  compatibility reasons. In the future, we'd like to stop using keywords
  altogether, and keep just stable unmasked.

  args:
    target - target for which to recreate package.keywords
  """
  maskfile = os.path.join('/etc/portage/package.keywords', 'cross-' + target)
  try:
    os.unlink(maskfile)
  except EnvironmentError as e:
    if e.errno != errno.ENOENT:
      raise
  cros_build_lib.SafeMakedirs(os.path.dirname(maskfile))

  keyword = GetPortageKeyword(target)

  with open(maskfile, 'w') as f:
    for pkg in GetTargetPackages(target):
      if IsPackageDisabled(target, pkg):
        continue
      f.write('%s %s ~%s\n' %
              (GetPortagePackage(target, pkg), keyword, keyword))


# Main functions performing the actual update steps.
def InitializeCrossdevTargets(targets, usepkg):
  """Calls crossdev to initialize a cross target.
  args:
    targets - the list of targets to initialize using crossdev
    usepkg - copies the commandline opts
  """
  print 'The following targets need to be re-initialized:'
  print targets

  extra_env = { 'FEATURES' : 'splitdebug' }
  for target in targets:
    cmd = ['crossdev', '--show-fail-log',
           '-t', target]
    # Pick stable by default, and override as necessary.
    cmd.extend(['-P', '--oneshot'])
    if usepkg:
      cmd.extend(['-P', '--getbinpkg',
                  '-P', '--usepkgonly',
                  '--without-headers'])

    cmd.extend(['--overlays', '%s %s' % (CHROMIUMOS_OVERLAY, STABLE_OVERLAY)])
    cmd.extend(['--ov-output', CROSSDEV_OVERLAY])

    for pkg in GetTargetPackages(target):
      if pkg == 'gdb':
        # Gdb does not have selectable versions.
        cmd.append('--ex-gdb')
        continue
      # The first of the desired versions is the "primary" one.
      version = GetDesiredPackageVersions(target, pkg)[0]
      cmd.extend(['--%s' % pkg, version])
    cros_build_lib.RunCommand(cmd, extra_env=extra_env)


def UpdateTargets(targets, usepkg):
  """Determines which packages need update/unmerge and defers to portage.

  args:
    targets - the list of targets to update
    usepkg - copies the commandline option
  """
  # TODO(zbehan): This process is rather complex due to package.* handling.
  # With some semantic changes over the original setup_board functionality,
  # it can be considerably cleaned up.
  mergemap = {}

  # For each target, we do two things. Figure out the list of updates,
  # and figure out the appropriate keywords/masks. Crossdev will initialize
  # these, but they need to be regenerated on every update.
  print 'Determining required toolchain updates...'
  for target in targets:
    # Record the highest needed version for each target, for masking purposes.
    RemovePackageMask(target)
    CreatePackageKeywords(target)
    packagemasks = {}
    for package in GetTargetPackages(target):
      # Portage name for the package
      if IsPackageDisabled(target, package):
        continue
      pkg = GetPortagePackage(target, package)
      current = GetInstalledPackageVersions(target, package)
      desired = GetDesiredPackageVersions(target, package)
      desired_num = VersionListToNumeric(target, package, desired, False)
      mergemap[pkg] = set(desired_num).difference(current)

      # Pick the highest version for mask.
      packagemasks[pkg] = portage.versions.best(desired_num)

    CreatePackageMask(target, packagemasks)

  packages = []
  for pkg in mergemap:
    for ver in mergemap[pkg]:
      if ver != PACKAGE_NONE:
        # Be a little more permissive for usepkg, the binaries may not exist.
        packages.append('%s%s-%s' % ('<=' if usepkg else '=', pkg, ver))

  if not packages:
    print 'Nothing to update!'
    return

  print 'Updating packages:'
  print packages

  cmd = [EMERGE_CMD, '--oneshot', '--update']
  if usepkg:
    cmd.extend(['--getbinpkg', '--usepkgonly'])

  cmd.extend(packages)
  cros_build_lib.RunCommand(cmd)


def CleanTargets(targets):
  """Unmerges old packages that are assumed unnecessary."""
  unmergemap = {}
  for target in targets:
    for package in GetTargetPackages(target):
      if IsPackageDisabled(target, package):
        continue
      pkg = GetPortagePackage(target, package)
      current = GetInstalledPackageVersions(target, package)
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

      cmd = [ package + '-config', '-c', target ]
      current = cros_build_lib.RunCommand(cmd, print_cmd=False,
          redirect_stdout=True).output.splitlines()[0]
      # Do not gcc-config when the current is live or nothing needs to be done.
      if current != desired and current != '9999':
        cmd = [ package + '-config', desired ]
        cros_build_lib.RunCommand(cmd, print_cmd=False)


def UpdateToolchains(usepkg, deleteold, hostonly, targets_wanted):
  """Performs all steps to create a synchronized toolchain enviroment.

  args:
    arguments correspond to the given commandline flags
  """
  targets = set()
  if not hostonly:
    # For hostonly, we can skip most of the below logic, much of which won't
    # work on bare systems where this is useful.
    alltargets = GetAllTargets()
    nonexistant = []
    if targets_wanted == set(['all']):
      targets = set(alltargets)
    else:
      targets = set(targets_wanted)
      # Verify user input.
      for target in targets_wanted:
        if target not in alltargets:
          nonexistant.append(target)
    if nonexistant:
      raise Exception("Invalid targets: " + ','.join(nonexistant))

    # First check and initialize all cross targets that need to be.
    crossdev_targets = \
        [t for t in targets if not TargetIsInitialized(t)]
    if crossdev_targets:
      InitializeCrossdevTargets(crossdev_targets, usepkg)

  # We want host updated.
  targets.add('host')

  # Now update all packages.
  UpdateTargets(targets, usepkg)
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
                    dest='targets', default='all',
                    help=('Comma separated list of tuples. '
                          'Special keyword \'host\' is allowed. Default: all.'))
  parser.add_option('', '--hostonly',
                    dest='hostonly', default=False, action='store_true',
                    help=('Only setup the host toolchain. '
                          'Useful for bootstrapping chroot.'))

  (options, _remaining_arguments) = parser.parse_args(argv)

  # This has to be always ran as root.
  if not os.getuid() == 0:
    print "%s: This script must be run as root!" % sys.argv[0]
    sys.exit(1)

  targets = set(options.targets.split(','))
  UpdateToolchains(options.usepkg, options.deleteold, options.hostonly, targets)
