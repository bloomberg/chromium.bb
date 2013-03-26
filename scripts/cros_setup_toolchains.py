#!/usr/bin/env python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script manages the installed toolchains in the chroot.
"""

import copy
import glob
import json
import os

from chromite.buildbot import constants
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import toolchain

# Needs to be after chromite imports.
import lddtree

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
        target_tuple = toolchain.GetHostTuple()
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


def GetInstalledPackageVersions(atom):
  """Extracts the list of current versions of a target, package pair.

  args:
    atom - the atom to operate on (e.g. sys-devel/gcc)

  returns the list of versions of the package currently installed.
  """
  versions = []
  # pylint: disable=E1101
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
  # pylint: disable=E1101
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
        target = toolchain.GetHostTuple()

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


def ExpandTargets(targets_wanted):
  """Expand any possible toolchain aliases into full targets

  This will expand 'all' and 'sdk' into the respective toolchain tuples.

  Args:
    targets_wanted: The targets specified by the user.
  Returns:
    Full list of tuples with pseudo targets removed.
  """
  alltargets = toolchain.GetAllTargets()
  targets_wanted = set(targets_wanted)
  if targets_wanted == set(['all']):
    targets = alltargets
  elif targets_wanted == set(['sdk']):
    # Filter out all the non-sdk toolchains as we don't want to mess
    # with those in all of our builds.
    targets = toolchain.FilterToolchains(alltargets, 'sdk', True)
  else:
    # Verify user input.
    nonexistent = targets_wanted.difference(alltargets)
    if nonexistent:
      raise ValueError('Invalid targets: %s', ','.join(nonexistent))
    targets = dict((t, alltargets[t]) for t in targets_wanted)
  return targets


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
    targets = ExpandTargets(targets_wanted)

    # Now re-add any targets that might be from this board.  This is
    # to allow unofficial boards to declare their own toolchains.
    for board in boards_wanted:
      targets.update(toolchain.GetToolchainsForBoard(board))

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


def ShowBoardConfig(board):
  """Show the toolchain tuples used by |board|

  Args:
    board: The board to query.
  """
  toolchains = toolchain.GetToolchainsForBoard(board)
  # Make sure we display the default toolchain first.
  print ','.join(
      toolchain.FilterToolchains(toolchains, 'default', True).keys() +
      toolchain.FilterToolchains(toolchains, 'default', False).keys())


def GenerateLdsoWrapper(root, path, interp, libpaths=()):
  """Generate a shell script wrapper which uses local ldso to run the ELF

  Since we cannot rely on the host glibc (or other libraries), we need to
  execute the local packaged ldso directly and tell it where to find our
  copies of libraries.

  Args:
    root: The root tree to generate scripts inside of
    path: The full path (inside |root|) to the program to wrap
    interp: The ldso interpreter that we need to execute
    libpaths: Extra lib paths to search for libraries
  """
  basedir = os.path.dirname(path)
  libpaths = ['/lib'] + list(libpaths)
  replacements = {
      'interp': os.path.join(os.path.relpath('/lib', basedir),
                             os.path.basename(interp)),
      'libpaths': ':'.join(['${basedir}/' + os.path.relpath(p, basedir)
                            for p in libpaths]),
  }
  wrapper = """#!/bin/sh
base=$(realpath "$0")
basedir=${base%%/*}
exec \
  "${basedir}/%(interp)s" \
  --library-path "%(libpaths)s" \
  --inhibit-rpath '' \
  "${base}.elf" \
  "$@"
""" % replacements
  wrappath = root + path
  os.rename(wrappath, wrappath + '.elf')
  osutils.WriteFile(wrappath, wrapper)
  os.chmod(wrappath, 0755)


def GeneratePathWrapper(root, wrappath, path):
  """Generate a shell script to execute another shell script

  Since we can't symlink a wrapped ELF (see GenerateLdsoWrapper) because the
  argv[0] won't be pointing to the correct path, generate a shell script that
  just executes another program with its full path.

  Args:
    root: The root tree to generate scripts inside of
    wrappath: The full path (inside |root|) to create the wrapper
    path: The target program which this wrapper will execute
  """
  replacements = {
      'path': path,
      'relroot': os.path.relpath('/', os.path.dirname(wrappath)),
  }
  wrapper = """#!/bin/sh
base=$(realpath "$0")
basedir=${base%%/*}
exec "${basedir}/%(relroot)s%(path)s" "$@"
""" % replacements
  root_wrapper = root + wrappath
  if os.path.islink(root_wrapper):
    os.unlink(root_wrapper)
  else:
    osutils.SafeMakedirs(os.path.dirname(root_wrapper))
  osutils.WriteFile(root_wrapper, wrapper)
  os.chmod(root_wrapper, 0755)


def FileIsCrosSdkElf(elf):
  """Determine if |elf| is an ELF that we execute in the cros_sdk

  We don't need this to be perfect, just quick.  It makes sure the ELF
  is a 64bit LSB x86_64 ELF.  That is the native type of cros_sdk.

  Args:
    elf: The file to check
  Returns:
    True if we think |elf| is a native ELF
  """
  with open(elf) as f:
    data = f.read(20)
    # Check the magic number, EI_CLASS, EI_DATA, and e_machine.
    return (data[0:4] == '\x7fELF' and
            data[4] == '\x02' and
            data[5] == '\x01' and
            data[18] == '\x3e')


def IsPathPackagable(ptype, path):
  """Should the specified file be included in a toolchain package?

  We only need to handle files as we'll create dirs as we need them.

  Further, trim files that won't be useful:
   - non-english translations (.mo) since it'd require env vars
   - debug files since these are for the host compiler itself
   - info/man pages as they're big, and docs are online, and the
     native docs should work fine for the most part (`man gcc`)

  Args:
    ptype: A string describing the path type (i.e. 'file' or 'dir' or 'sym')
    path: The full path to inspect
  Returns:
    True if we want to include this path in the package
  """
  return not (ptype in ('dir',) or
              path.startswith('/usr/lib/debug/') or
              os.path.splitext(path)[1] == '.mo' or
              ('/man/' in path or '/info/' in path))


def ReadlinkRoot(path, root):
  """Like os.readlink(), but relative to a |root|

  Args:
    path: The symlink to read
    root: The path to use for resolving absolute symlinks
  Returns:
    A fully resolved symlink path
  """
  while os.path.islink(root + path):
    path = os.path.join(os.path.dirname(path), os.readlink(root + path))
  return path


def _GetFilesForTarget(target, root='/'):
  """Locate all the files to package for |target|

  This does not cover ELF dependencies.

  Args:
    target: The toolchain target name
    root: The root path to pull all packages from
  Returns:
    A tuple of a set of all packable paths, and a set of all paths which
    are also native ELFs
  """
  paths = set()
  elfs = set()

  # Find all the files owned by the packages for this target.
  for pkg in GetTargetPackages(target):
    # Ignore packages that are part of the target sysroot.
    if pkg in ('kernel', 'libc'):
      continue

    atom = GetPortagePackage(target, pkg)
    cat, pn = atom.split('/')
    ver = GetInstalledPackageVersions(atom)[0]
    cros_build_lib.Info('packaging %s-%s', atom, ver)

    # pylint: disable=E1101
    dblink = portage.dblink(cat, pn + '-' + ver, myroot=root,
                            settings=portage.settings)
    contents = dblink.getcontents()
    for obj in contents:
      ptype = contents[obj][0]
      if not IsPathPackagable(ptype, obj):
        continue

      if ptype == 'obj':
        # For native ELFs, we need to pull in their dependencies too.
        if FileIsCrosSdkElf(obj):
          elfs.add(obj)
      paths.add(obj)

  return paths, elfs


def _BuildInitialPackageRoot(output_dir, paths, elfs, ldpaths,
                             path_rewrite_func=lambda x:x, root='/'):
  """Link in all packable files and their runtime dependencies

  This also wraps up executable ELFs with helper scripts.

  Args:
    output_dir: The output directory to store files
    paths: All the files to include
    elfs: All the files which are ELFs (a subset of |paths|)
    ldpaths: A dict of static ldpath information
    path_rewrite_func: User callback to rewrite paths in output_dir
    root: The root path to pull all packages/files from
  """
  # Link in all the files.
  sym_paths = []
  for path in paths:
    new_path = path_rewrite_func(path)
    dst = output_dir + new_path
    osutils.SafeMakedirs(os.path.dirname(dst))

    # Is this a symlink which we have to rewrite or wrap?
    # Delay wrap check until after we have created all paths.
    src = root + path
    if os.path.islink(src):
      tgt = os.readlink(src)
      if os.path.sep in tgt:
        sym_paths.append((new_path, lddtree.normpath(ReadlinkRoot(src, root))))

        # Rewrite absolute links to relative and then generate the symlink
        # ourselves.  All other symlinks can be hardlinked below.
        if tgt[0] == '/':
          tgt = os.path.relpath(tgt, os.path.dirname(new_path))
          os.symlink(tgt, dst)
          continue

    os.link(src, dst)

  # Now see if any of the symlinks need to be wrapped.
  for sym, tgt in sym_paths:
    if tgt in elfs:
      GeneratePathWrapper(output_dir, sym, tgt)

  # Locate all the dependencies for all the ELFs.  Stick them all in the
  # top level "lib" dir to make the wrapper simpler.  This exact path does
  # not matter since we execute ldso directly, and we tell the ldso the
  # exact path to search for its libraries.
  libdir = os.path.join(output_dir, 'lib')
  osutils.SafeMakedirs(libdir)
  donelibs = set()
  for elf in elfs:
    e = lddtree.ParseELF(elf, root, ldpaths)
    interp = e['interp']
    if interp:
      # Generate a wrapper if it is executable.
      GenerateLdsoWrapper(output_dir, path_rewrite_func(elf), interp,
                          libpaths=e['rpath'] + e['runpath'])

    for lib, lib_data in e['libs'].iteritems():
      if lib in donelibs:
        continue

      src = path = lib_data['path']
      if path is None:
        cros_build_lib.Warning('%s: could not locate %s', elf, lib)
        continue
      donelibs.add(lib)

      # Needed libs are the SONAME, but that is usually a symlink, not a
      # real file.  So link in the target rather than the symlink itself.
      # We have to walk all the possible symlinks (SONAME could point to a
      # symlink which points to a symlink), and we have to handle absolute
      # ourselves (since we have a "root" argument).
      dst = os.path.join(libdir, os.path.basename(path))
      src = ReadlinkRoot(src, root)

      os.link(root + src, dst)


def _EnvdGetVar(envd, var):
  """Given a Gentoo env.d file, extract a var from it

  Args:
    envd: The env.d file to load (may be a glob path)
    var: The var to extract
  Returns:
    The value of |var|
  """
  envds = glob.glob(envd)
  assert len(envds) == 1, '%s: should have exactly 1 env.d file' % envd
  envd = envds[0]
  return cros_build_lib.LoadKeyValueFile(envd)[var]


def _ProcessBinutilsConfig(target, output_dir):
  """Do what binutils-config would have done"""
  binpath = os.path.join('/bin', target + '-')
  globpath = os.path.join(output_dir, 'usr', toolchain.GetHostTuple(), target,
                          'binutils-bin', '*-gold')
  srcpath = glob.glob(globpath)
  assert len(srcpath) == 1, '%s: did not match 1 path' % globpath
  srcpath = srcpath[0][len(output_dir):]
  gccpath = os.path.join('/usr', 'libexec', 'gcc')
  for prog in os.listdir(output_dir + srcpath):
    # Skip binaries already wrapped.
    if not prog.endswith('.real'):
      GeneratePathWrapper(output_dir, binpath + prog,
                          os.path.join(srcpath, prog))
      GeneratePathWrapper(output_dir, os.path.join(gccpath, prog),
                          os.path.join(srcpath, prog))

  libpath = os.path.join('/usr', toolchain.GetHostTuple(), target, 'lib')
  envd = os.path.join(output_dir, 'etc', 'env.d', 'binutils', '*-gold')
  srcpath = _EnvdGetVar(envd, 'LIBPATH')
  os.symlink(os.path.relpath(srcpath, os.path.dirname(libpath)),
             output_dir + libpath)


def _ProcessGccConfig(target, output_dir):
  """Do what gcc-config would have done"""
  binpath = '/bin'
  envd = os.path.join(output_dir, 'etc', 'env.d', 'gcc', '*')
  srcpath = _EnvdGetVar(envd, 'GCC_PATH')
  for prog in os.listdir(output_dir + srcpath):
    # Skip binaries already wrapped.
    if (not prog.endswith('.real') and
        not prog.endswith('.elf') and
        prog.startswith(target)):
      GeneratePathWrapper(output_dir, os.path.join(binpath, prog),
                          os.path.join(srcpath, prog))
  return srcpath


def _ProcessSysrootWrapper(_target, output_dir, srcpath):
  """Remove chroot-specific things from our sysroot wrapper"""
  # Disable ccache since we know it won't work outside of chroot.
  sysroot_wrapper = glob.glob(os.path.join(
      output_dir + srcpath, 'sysroot_wrapper*'))[0]
  contents = osutils.ReadFile(sysroot_wrapper).splitlines()
  for num in xrange(len(contents)):
    if '@CCACHE_DEFAULT@' in contents[num]:
      contents[num] = 'use_ccache = False'
      break
  # Can't update the wrapper in place since it's a hardlink to a file in /.
  os.unlink(sysroot_wrapper)
  osutils.WriteFile(sysroot_wrapper, '\n'.join(contents))
  os.chmod(sysroot_wrapper, 0755)


def _ProcessDistroCleanups(target, output_dir):
  """Clean up the tree and remove all distro-specific requirements

  Args:
    target: The toolchain target name
    output_dir: The output directory to clean up
  """
  _ProcessBinutilsConfig(target, output_dir)
  gcc_path = _ProcessGccConfig(target, output_dir)
  _ProcessSysrootWrapper(target, output_dir, gcc_path)

  osutils.RmDir(os.path.join(output_dir, 'etc'))


def CreatePackagableRoot(target, output_dir, ldpaths, root='/'):
  """Setup a tree from the packages for the specified target

  This populates a path with all the files from toolchain packages so that
  a tarball can easily be generated from the result.

  Args:
    target: The target to create a packagable root from
    output_dir: The output directory to place all the files
    ldpaths: A dict of static ldpath information
    root: The root path to pull all packages/files from
  """
  # Find all the files owned by the packages for this target.
  paths, elfs = _GetFilesForTarget(target, root=root)

  # Link in all the package's files, any ELF dependencies, and wrap any
  # executable ELFs with helper scripts.
  def MoveUsrBinToBin(path):
    """Move /usr/bin to /bin so people can just use that toplevel dir"""
    return path[4:] if path.startswith('/usr/bin/') else path
  _BuildInitialPackageRoot(output_dir, paths, elfs, ldpaths,
                           path_rewrite_func=MoveUsrBinToBin, root=root)

  # The packages, when part of the normal distro, have helper scripts
  # that setup paths and such.  Since we are making this standalone, we
  # need to preprocess all that ourselves.
  _ProcessDistroCleanups(target, output_dir)


def CreatePackages(targets_wanted, output_dir, root='/'):
  """Create redistributable cross-compiler packages for the specified targets

  This creates toolchain packages that should be usable in conjunction with
  a downloaded sysroot (created elsewhere).

  Tarballs (one per target) will be created in $PWD.

  Args:
    targets_wanted: The targets to package up
    root: The root path to pull all packages/files from
  """
  osutils.SafeMakedirs(output_dir)
  ldpaths = lddtree.LoadLdpaths(root)
  targets = ExpandTargets(targets_wanted)

  with osutils.TempDir() as tempdir:
    # We have to split the root generation from the compression stages.  This is
    # because we hardlink in all the files (to avoid overhead of reading/writing
    # the copies multiple times).  But tar gets angry if a file's hardlink count
    # changes from when it starts reading a file to when it finishes.
    with parallel.BackgroundTaskRunner(CreatePackagableRoot) as queue:
      for target in targets:
        output_target_dir = os.path.join(tempdir, target)
        queue.put([target, output_target_dir, ldpaths, root])

    # Build the tarball.
    with parallel.BackgroundTaskRunner(cros_build_lib.CreateTarball) as queue:
      for target in targets:
        tar_file = os.path.join(output_dir, target + '.tar.xz')
        queue.put([tar_file, os.path.join(tempdir, target)])


def main(argv):
  usage = """usage: %prog [options]

  The script installs and updates the toolchains in your chroot."""
  parser = commandline.OptionParser(usage)
  parser.add_option('-u', '--nousepkg',
                    action='store_false', dest='usepkg', default=True,
                    help='Use prebuilt packages if possible')
  parser.add_option('-d', '--deleteold',
                    action='store_true', dest='deleteold', default=False,
                    help='Unmerge deprecated packages')
  parser.add_option('-t', '--targets',
                    dest='targets', default='sdk',
                    help='Comma separated list of tuples. '
                         'Special keyword \'host\' is allowed. Default: sdk')
  parser.add_option('--include-boards',
                    dest='include_boards', default='',
                    help='Comma separated list of boards whose toolchains we'
                         ' will always include. Default: none')
  parser.add_option('--hostonly',
                    dest='hostonly', default=False, action='store_true',
                    help='Only setup the host toolchain. '
                         'Useful for bootstrapping chroot')
  parser.add_option('--show-board-cfg',
                    dest='board_cfg', default=None,
                    help='Board to list toolchain tuples for')
  parser.add_option('--create-packages',
                    action='store_true', default=False,
                    help='Build redistributable packages')
  parser.add_option('--output-dir', default=os.getcwd(), type='path',
                    help='Output directory')
  parser.add_option('--reconfig', default=False, action='store_true',
                    help='Reload crossdev config and reselect toolchains')

  (options, remaining_arguments) = parser.parse_args(argv)
  if len(remaining_arguments):
    parser.error('script does not take arguments: %s' % remaining_arguments)

  # Figure out what we're supposed to do and reject conflicting options.
  if options.board_cfg and options.create_packages:
    parser.error('conflicting options: create-packages & show-board-cfg')

  targets = set(options.targets.split(','))
  boards = set(options.include_boards.split(',')) if options.include_boards \
      else set()

  if options.board_cfg:
    ShowBoardConfig(options.board_cfg)
  elif options.create_packages:
    cros_build_lib.AssertInsideChroot()
    Crossdev.Load(False)
    CreatePackages(targets, options.output_dir)
  else:
    cros_build_lib.AssertInsideChroot()
    # This has to be always run as root.
    if os.geteuid() != 0:
      cros_build_lib.Die('this script must be run as root')

    Crossdev.Load(options.reconfig)
    UpdateToolchains(options.usepkg, options.deleteold, options.hostonly,
                     options.reconfig, targets, boards)
    Crossdev.Save()

  return 0
