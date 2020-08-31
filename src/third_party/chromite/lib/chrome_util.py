# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library containing utility functions used for Chrome-specific build tasks."""

from __future__ import print_function

import ast
import functools
import glob
import os
import re
import shlex
import shutil
import sys

from chromite.lib import failures_lib
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


# Taken from external/gyp.git/pylib.
def _NameValueListToDict(name_value_list):
  """Converts Name-Value list to dictionary.

  Takes an array of strings of the form 'NAME=VALUE' and creates a dictionary
  of the pairs.  If a string is simply NAME, then the value in the dictionary
  is set to True.  If VALUE can be converted to an integer, it is.
  """
  result = {}
  for item in name_value_list:
    tokens = item.split('=', 1)
    if len(tokens) == 2:
      # If we can make it an int, use that, otherwise, use the string.
      try:
        token_value = int(tokens[1])
      except ValueError:
        token_value = tokens[1]
      # Set the variable to the supplied value.
      result[tokens[0]] = token_value
    else:
      # No value supplied, treat it as a boolean and set it.
      result[tokens[0]] = True
  return result


def ProcessShellFlags(defines):
  """Validate and convert a string of shell style flags to a dictionary."""
  assert defines is not None
  return _NameValueListToDict(shlex.split(defines))


class Conditions(object):
  """Functions that return conditions used to construct Path objects.

  Condition functions returned by the public methods have signature
  f(gn_args, staging_flags). For descriptions of gn_args and
  staging_flags see docstring for StageChromeFromBuildDir().
  """

  @classmethod
  def _GnSetTo(cls, flag, value, gn_args, _staging_flags):
    val = gn_args.get(flag)
    return val == value

  @classmethod
  def _StagingFlagSet(cls, flag, _gn_args, staging_flags):
    return flag in staging_flags

  @classmethod
  def _StagingFlagNotSet(cls, flag, gn_args, staging_flags):
    return not cls._StagingFlagSet(flag, gn_args, staging_flags)

  @classmethod
  def GnSetTo(cls, flag, value):
    """Returns condition that tests a gn flag is set to a value."""
    return functools.partial(cls._GnSetTo, flag, value)

  @classmethod
  def StagingFlagSet(cls, flag):
    """Returns condition that tests a staging_flag is set."""
    return functools.partial(cls._StagingFlagSet, flag)

  @classmethod
  def StagingFlagNotSet(cls, flag):
    """Returns condition that tests a staging_flag is not set."""
    return functools.partial(cls._StagingFlagNotSet, flag)


class MultipleMatchError(failures_lib.StepFailure):
  """A glob pattern matches multiple files but a non-dir dest was specified."""


class MissingPathError(failures_lib.StepFailure):
  """An expected path is non-existant."""


class MustNotBeDirError(failures_lib.StepFailure):
  """The specified path should not be a directory, but is."""


class GetRuntimeDepsError(failures_lib.StepFailure):
  """Unable to get runtime deps for a build target."""


class GnIsolateMapFileError(failures_lib.StepFailure):
  """Failed to parse gn isolate map file."""


class Copier(object):
  """File/directory copier.

  Provides destination stripping and permission setting functionality.
  """

  def __init__(self, strip_bin=None, strip_flags=None, default_mode=0o644,
               dir_mode=0o755, exe_mode=0o755):
    """Initialization.

    Args:
      strip_bin: Path to the program used to strip binaries.  If set to None,
                 binaries will not be stripped.
      strip_flags: A list of flags to pass to the |strip_bin| executable.
      default_mode: Default permissions to set on files.
      dir_mode: Mode to set for directories.
      exe_mode: Permissions to set on executables.
    """
    self.strip_bin = strip_bin
    self.strip_flags = strip_flags
    self.default_mode = default_mode
    self.dir_mode = dir_mode
    self.exe_mode = exe_mode

  @staticmethod
  def Log(src, dest, directory):
    sep = ' [d] -> ' if directory else ' -> '
    logging.debug('%s %s %s', src, sep, dest)

  def _CopyFile(self, src, dest, path):
    """Perform the copy.

    Args:
      src: The path of the file/directory to copy.
      dest: The exact path of the destination. Does nothing if it already
            exists.
      path: The Path instance containing copy operation modifiers (such as
            Path.exe, Path.strip, etc.)
    """
    assert not os.path.isdir(src), '%s: Not expecting a directory!' % src

    # This file has already been copied by an earlier Path.
    if os.path.exists(dest):
      return

    osutils.SafeMakedirs(os.path.dirname(dest), mode=self.dir_mode)
    if path.exe and self.strip_bin and path.strip and os.path.getsize(src) > 0:
      strip_flags = (['--strip-unneeded'] if self.strip_flags is None else
                     self.strip_flags)
      cros_build_lib.dbg_run(
          [self.strip_bin] + strip_flags + ['-o', dest, src])
      shutil.copystat(src, dest)
    else:
      shutil.copy2(src, dest)

    mode = path.mode
    if mode is None:
      mode = self.exe_mode if path.exe else self.default_mode
    os.chmod(dest, mode)

  def Copy(self, src_base, dest_base, path, sloppy=False):
    """Copy artifact(s) from source directory to destination.

    Args:
      src_base: The directory to apply the src glob pattern match in.
      dest_base: The directory to copy matched files to.  |Path.dest|.
      path: A Path instance that specifies what is to be copied.
      sloppy: If set, ignore when mandatory artifacts are missing.

    Returns:
      A list of the artifacts copied.
    """
    copied_paths = []
    src = os.path.join(src_base, path.src)
    if not src.endswith('/') and os.path.isdir(src):
      raise MustNotBeDirError('%s must not be a directory\n'
                              'Aborting copy...' % (src,))
    paths = glob.glob(src)
    if not paths:
      if path.optional:
        logging.debug('%s does not exist and is optional.  Skipping.', src)
      elif sloppy:
        logging.warning('%s does not exist and is required.  Skipping anyway.',
                        src)
      else:
        msg = ('%s does not exist and is required.\n'
               'You can bypass this error with --sloppy.\n'
               'Aborting copy...' % src)
        raise MissingPathError(msg)
    elif len(paths) > 1 and path.dest and not path.dest.endswith('/'):
      raise MultipleMatchError(
          'Glob pattern %r has multiple matches, but dest %s '
          'is not a directory.\n'
          'Aborting copy...' % (path.src, path.dest))
    else:
      for p in paths:
        rel_src = os.path.relpath(p, src_base)
        if path.IsBlacklisted(rel_src):
          continue
        if path.dest is None:
          rel_dest = rel_src
        elif path.dest.endswith('/'):
          rel_dest = os.path.join(path.dest, os.path.basename(p))
        else:
          rel_dest = path.dest
        assert not rel_dest.endswith('/')
        dest = os.path.join(dest_base, rel_dest)

        copied_paths.append(p)
        self.Log(p, dest, os.path.isdir(p))
        if os.path.isdir(p):
          for sub_path in osutils.DirectoryIterator(p):
            rel_path = os.path.relpath(sub_path, p)
            sub_dest = os.path.join(dest, rel_path)
            if path.IsBlacklisted(rel_path):
              continue
            if sub_path.endswith('/'):
              osutils.SafeMakedirs(sub_dest, mode=self.dir_mode)
            else:
              self._CopyFile(sub_path, sub_dest, path)
        else:
          self._CopyFile(p, dest, path)

    return copied_paths


class Path(object):
  """Represents an artifact to be copied from build dir to staging dir."""

  DEFAULT_BLACKLIST = (r'(^|.*/)\.git($|/.*)',)

  def __init__(self, src, exe=False, cond=None, dest=None, mode=None,
               optional=False, strip=True, blacklist=None):
    """Initializes the object.

    Args:
      src: The relative path of the artifact.  Can be a file or a directory.
           Can be a glob pattern.
      exe: Identifes the path as either being an executable or containing
           executables.  Executables may be stripped during copy, and have
           special permissions set.  We currently only support stripping of
           specified files and glob patterns that return files.  If |src| is a
           directory or contains directories, the content of the directory will
           not be stripped.
      cond: A condition (see Conditions class) to test for in deciding whether
            to process this artifact.
      dest: Name to give to the target file/directory.  Defaults to keeping the
            same name as the source.
      mode: The mode to set for the matched files, and the contents of matched
            directories.
      optional: Whether to enforce the existence of the artifact.  If unset, the
                script errors out if the artifact does not exist.  In 'sloppy'
                mode, the Copier class treats all artifacts as optional.
      strip: If |exe| is set, whether to strip the executable.
      blacklist: A list of path patterns to ignore during the copy. This gets
                 added to a default blacklist pattern.
    """
    self.src = src
    self.exe = exe
    self.cond = cond
    self.dest = dest
    self.mode = mode
    self.optional = optional
    self.strip = strip
    self.blacklist = self.DEFAULT_BLACKLIST
    if blacklist is not None:
      self.blacklist += tuple(blacklist)

  def IsBlacklisted(self, path):
    """Returns whether |path| is in the blacklist.

    A file in the blacklist is not copied over to the staging directory.

    Args:
      path: The path of a file, relative to the path of this Path object.
    """
    for pattern in self.blacklist:
      if re.match(pattern, path):
        return True
    return False

  def ShouldProcess(self, gn_args, staging_flags):
    """Tests whether this artifact should be copied."""
    if not gn_args and not staging_flags:
      return True
    if self.cond and isinstance(self.cond, list):
      for c in self.cond:
        if not c(gn_args, staging_flags):
          return False
    elif self.cond:
      return self.cond(gn_args, staging_flags)
    return True

_ENABLE_NACL = 'enable_nacl'
_IS_CHROME_BRANDED = 'is_chrome_branded'
_IS_COMPONENT_BUILD = 'is_component_build'

_HIGHDPI_FLAG = 'highdpi'
STAGING_FLAGS = (
    _HIGHDPI_FLAG,
)

C = Conditions

# In the below Path lists, if two Paths both match a file, the earlier Path
# takes precedence.

# Files shared between all deployment types.
_COPY_PATHS_COMMON = (
    # Copying icudtl.dat has to be optional because in CROS, icudtl.dat will
    # be installed by the package "chrome-icu", and icudtl.dat in chrome is
    # deleted in the chromeos-chrome ebuild. But we can not delete this line
    # totally because chromite/deloy_chrome is used outside of ebuild
    # (see https://crbug.com/1081884).
    Path('icudtl.dat', optional=True),
    Path('libosmesa.so', exe=True, optional=True),
    # Do not strip the nacl_helper_bootstrap binary because the binutils
    # objcopy/strip mangles the ELF program headers.
    Path('nacl_helper_bootstrap',
         exe=True,
         strip=False,
         cond=C.GnSetTo(_ENABLE_NACL, True)),
    Path('nacl_irt_*.nexe', cond=C.GnSetTo(_ENABLE_NACL, True)),
    Path('nacl_helper',
         exe=True,
         optional=True,
         cond=C.GnSetTo(_ENABLE_NACL, True)),
    Path('nacl_helper_nonsfi',
         exe=True,
         optional=True,
         cond=C.GnSetTo(_ENABLE_NACL, True)),
    Path('natives_blob.bin', optional=True),
    Path('pnacl/', cond=C.GnSetTo(_ENABLE_NACL, True)),
    Path('snapshot_blob.bin', optional=True),
)

_COPY_PATHS_APP_SHELL = (
    Path('app_shell', exe=True),
    Path('extensions_shell_and_test.pak'),
) + _COPY_PATHS_COMMON

_COPY_PATHS_CHROME = (
    Path('chrome', exe=True),
    Path('chrome-wrapper'),
    Path('chrome_100_percent.pak'),
    Path('chrome_200_percent.pak', cond=C.StagingFlagSet(_HIGHDPI_FLAG)),
    # TODO(jperaza): make the handler required when  Crashpad is enabled.
    Path('crashpad_handler', exe=True, optional=True),
    Path('dbus/', optional=True),
    Path('keyboard_resources.pak'),
    Path('libassistant.so', exe=True, optional=True),
    Path('libmojo_core.so', exe=True),

    # The ARC++ mojo_core libraries are pre-stripped and don't play well with
    # the binutils stripping tools, hence stripping is disabled here.
    Path('libmojo_core_arc32.so', exe=True, strip=False),
    Path('libmojo_core_arc64.so', exe=True, strip=False),

    # Widevine CDM is already pre-stripped.  In addition, it doesn't
    # play well with the binutils stripping tools, so skip stripping.
    # Optional for arm64 builds (http://crbug.com/881022)
    Path('libwidevinecdm.so',
         exe=True,
         strip=False,
         cond=C.GnSetTo(_IS_CHROME_BRANDED, True),
         optional=C.GnSetTo('target_cpu', 'arm64')),
    # In component build, copy so files (e.g. libbase.so) except for the
    # blacklist.
    Path('*.so',
         blacklist=(r'libwidevinecdm.so',),
         exe=True,
         cond=C.GnSetTo(_IS_COMPONENT_BUILD, True)),
    Path('locales/*.pak', optional=True),
    Path('locales/*.pak.gz', optional=True),
    Path('Packages/chrome_content_browser/manifest.json', optional=True),
    Path('Packages/chrome_content_gpu/manifest.json', optional=True),
    Path('Packages/chrome_content_plugin/manifest.json', optional=True),
    Path('Packages/chrome_content_renderer/manifest.json', optional=True),
    Path('Packages/chrome_content_utility/manifest.json', optional=True),
    Path('Packages/chrome_mash/manifest.json', optional=True),
    Path('Packages/chrome_mash_content_browser/manifest.json', optional=True),
    Path('Packages/content_browser/manifest.json', optional=True),
    Path('resources/chromeos/'),
    Path('resources.pak'),
    Path('xdg-settings'),
    Path('*.png'),
) + _COPY_PATHS_COMMON

_COPY_PATHS_MAP = {
    'app_shell': _COPY_PATHS_APP_SHELL,
    'chrome': _COPY_PATHS_CHROME,
}


def _FixPermissions(dest_base):
  """Last minute permission fixes."""
  cros_build_lib.dbg_run(['chmod', '-R', 'a+r', dest_base])
  cros_build_lib.dbg_run(
      ['find', dest_base, '-perm', '/110', '-exec', 'chmod', 'a+x', '{}', '+'])


def GetCopyPaths(deployment_type='chrome'):
  """Returns the list of copy paths used as a filter for staging files.

  Args:
    deployment_type: String describing the deployment type. Either "app_shell"
                     or "chrome".

  Returns:
    The list of paths to use as a filter for staging files.
  """
  paths = _COPY_PATHS_MAP.get(deployment_type)
  if paths is None:
    raise RuntimeError('Invalid deployment type "%s"' % deployment_type)
  return paths


def _GetGnLabel(build_dir, build_target):
  """Gets the gn label for a build target in a build dir.

  Args:
    build_dir: The build output directory.
    build_target: The build target whose gn label to be returned.

  Returns:
    Gn label for the build target as a string.
  """
  src_dir = os.path.dirname(os.path.dirname(build_dir))
  # Look up gn label from testing/buildbot/gn_isolate_map.pyl, which contains
  # a mapping of build targets to GN labels. This is faster than extracting the
  # gn label from "gn ls" output.
  isolate_map_file = os.path.join(src_dir, 'testing', 'buildbot',
                                  'gn_isolate_map.pyl')
  try:
    isolate_map = ast.literal_eval(osutils.ReadFile(isolate_map_file))
  except SyntaxError as e:
    raise GnIsolateMapFileError(
        'Failed to parse isolate map file "%s": %s' % (isolate_map_file, e))

  if not build_target in isolate_map:
    raise GnIsolateMapFileError(
        'Target %s not found in %s' % (build_target, isolate_map_file))

  gn_label = isolate_map[build_target]['label']
  assert gn_label.startswith('//')
  return gn_label


def GetChromeRuntimeDeps(build_dir, build_target):
  """Returns a list of runtime deps files for the given build target.

  Args:
    build_dir: The build output directory.
    build_target: A chrome build target.

  Returns:
    The list of runtime deps files for |build_target| relative to two levels up
    |build_dir|, i.e. chrome src dir.
  """
  gn_label = _GetGnLabel(build_dir, build_target)

  # Runtime deps files are generated for test executables of ChromeOS build
  # inside SDK env in testing/test.gni.
  #   https://cs.chromium.org/chromium/src/testing/test.gni?rcl=9b95cd58&l=336
  # Check out test.gni if the file is missing and the slow "gn desc" code path
  # is used.
  generated_runtime_deps_file = os.path.join(build_dir,
                                             'gen.runtime',
                                             gn_label.split(':')[0].lstrip('/'),
                                             build_target,
                                             build_target + '.runtime_deps')

  runtime_deps = None
  if not os.path.exists(generated_runtime_deps_file):
    logging.warning('Unable to find generated runtime deps file: %s',
                    generated_runtime_deps_file)
  else:
    runtime_deps = osutils.ReadFile(generated_runtime_deps_file).splitlines()

  if not runtime_deps:
    result = cros_build_lib.run(['gn', 'desc', build_dir, gn_label,
                                 'runtime_deps'],
                                capture_output=True, encoding='utf-8')
    if result.returncode != 0:
      raise GetRuntimeDepsError('Failed to get runtime deps for: %s' %
                                build_target)

    runtime_deps = result.output.splitlines()

  # |runtime_deps| is relative to |build_dir|. Make them relative to |src_dir|.
  src_dir = os.path.dirname(os.path.dirname(build_dir))
  rebased_runtime_deps = []
  for f in runtime_deps:
    rebased = os.path.relpath(os.path.abspath(os.path.join(build_dir, f)),
                              src_dir)
    # Dirs from a "data" rule in gn file do not have trailing '/' in runtime
    # deps. Ensures such dirs are ended with a trailing '/'.
    if os.path.isdir(rebased) and not rebased.endswith('/'):
      rebased += '/'

    rebased_runtime_deps.append(rebased)

  return rebased_runtime_deps


def GetChromeTestCopyPaths(build_dir, test_target):
  """Returns the list of copy paths for the given chrome test target.

  Args:
    build_dir: The build output directory that |runtime_deps| is relative to.
    test_target: A build target defined in //chrome/test/BUILD.gn

  Returns:
    The list of paths to stage for |runtime_deps| relative to two levels up
    |build_dir|, i.e. chrome src dir.
  """

  # Black list of file patterns for files in the runtime deps of the test target
  # but are not really needed to run the test. Keep sync with the list in
  # build/chromeos/test_runner.py in chromium code.
  _BLACKLIST = [
      re.compile(r'.*build/android.*'),
      re.compile(r'.*build/chromeos.*'),
      re.compile(r'.*build/cros_cache.*'),
      re.compile(r'.*testing/(?!buildbot/filters).*'),
      re.compile(r'.*third_party/chromite.*'),
      re.compile(r'.*tools/swarming_client.*'),
  ]

  src_dir = os.path.dirname(os.path.dirname(build_dir))
  copy_paths = []
  for f in GetChromeRuntimeDeps(build_dir, test_target):
    if not any(regex.match(f) for regex in _BLACKLIST):
      local_path = os.path.join(src_dir, f)
      is_exe = os.path.isfile(local_path) and os.access(local_path, os.X_OK)
      copy_paths.append(Path(f, exe=is_exe))

  return copy_paths


def StageChromeFromBuildDir(staging_dir, build_dir, strip_bin, sloppy=False,
                            gn_args=None, staging_flags=None,
                            strip_flags=None, copy_paths=_COPY_PATHS_CHROME):
  """Populates a staging directory with necessary build artifacts.

  If |gn_args| or |staging_flags| are set, then we decide what to stage
  based on the flag values. Otherwise, we stage everything that we know
  about that we can find.

  Args:
    staging_dir: Path to an empty staging directory.
    build_dir: Path to location of Chrome build artifacts.
    strip_bin: Path to executable used for stripping binaries.
    sloppy: Ignore when mandatory artifacts are missing.
    gn_args: A dictionary of args.gn valuses that Chrome was built with.
    staging_flags: A list of extra staging flags.  Valid flags are specified in
      STAGING_FLAGS.
    strip_flags: A list of flags to pass to the tool used to strip binaries.
    copy_paths: The list of paths to use as a filter for staging files.
  """
  os.mkdir(os.path.join(staging_dir, 'plugins'), 0o755)

  if gn_args is None:
    gn_args = {}
  if staging_flags is None:
    staging_flags = []

  copier = Copier(strip_bin=strip_bin, strip_flags=strip_flags)
  copied_paths = []
  for p in copy_paths:
    if p.ShouldProcess(gn_args, staging_flags):
      copied_paths += copier.Copy(build_dir, staging_dir, p, sloppy=sloppy)

  if not copied_paths:
    raise MissingPathError("Couldn't find anything to copy!\n"
                           'Are you looking in the right directory?\n'
                           'Aborting copy...')

  _FixPermissions(staging_dir)
