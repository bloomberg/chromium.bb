#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Performance Test Bisect Tool

This script bisects a series of changelists using binary search. It starts at
a bad revision where a performance metric has regressed, and asks for a last
known-good revision. It will then binary search across this revision range by
syncing, building, and running a performance test. If the change is
suspected to occur as a result of WebKit/V8 changes, the script will
further bisect changes to those depots and attempt to narrow down the revision
range.


An example usage (using svn cl's):

./tools/bisect-perf-regression.py -c\
"out/Release/performance_ui_tests --gtest_filter=ShutdownTest.SimpleUserQuit"\
-g 168222 -b 168232 -m shutdown/simple-user-quit

Be aware that if you're using the git workflow and specify an svn revision,
the script will attempt to find the git SHA1 where svn changes up to that
revision were merged in.


An example usage (using git hashes):

./tools/bisect-perf-regression.py -c\
"out/Release/performance_ui_tests --gtest_filter=ShutdownTest.SimpleUserQuit"\
-g 1f6e67861535121c5c819c16a666f2436c207e7b\
-b b732f23b4f81c382db0b23b9035f3dadc7d925bb\
-m shutdown/simple-user-quit

"""

import copy
import datetime
import errno
import imp
import math
import optparse
import os
import re
import shlex
import shutil
import StringIO
import subprocess
import sys
import time
import zipfile

import bisect_utils
import post_perf_builder_job


try:
  from telemetry.page import cloud_storage
except ImportError:
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), 'telemetry'))
  from telemetry.page import cloud_storage

# The additional repositories that might need to be bisected.
# If the repository has any dependant repositories (such as skia/src needs
# skia/include and skia/gyp to be updated), specify them in the 'depends'
# so that they're synced appropriately.
# Format is:
# src: path to the working directory.
# recurse: True if this repositry will get bisected.
# depends: A list of other repositories that are actually part of the same
#   repository in svn.
# svn: Needed for git workflow to resolve hashes to svn revisions.
# from: Parent depot that must be bisected before this is bisected.
DEPOT_DEPS_NAME = {
  'chromium' : {
    "src" : "src",
    "recurse" : True,
    "depends" : None,
    "from" : ['cros', 'android-chrome'],
    'viewvc': 'http://src.chromium.org/viewvc/chrome?view=revision&revision='
  },
  'webkit' : {
    "src" : "src/third_party/WebKit",
    "recurse" : True,
    "depends" : None,
    "from" : ['chromium'],
    'viewvc': 'http://src.chromium.org/viewvc/blink?view=revision&revision='
  },
  'angle' : {
    "src" : "src/third_party/angle",
    "src_old" : "src/third_party/angle_dx11",
    "recurse" : True,
    "depends" : None,
    "from" : ['chromium'],
    "platform": 'nt',
  },
  'v8' : {
    "src" : "src/v8",
    "recurse" : True,
    "depends" : None,
    "from" : ['chromium'],
    "custom_deps": bisect_utils.GCLIENT_CUSTOM_DEPS_V8,
    'viewvc': 'https://code.google.com/p/v8/source/detail?r=',
  },
  'v8_bleeding_edge' : {
    "src" : "src/v8_bleeding_edge",
    "recurse" : True,
    "depends" : None,
    "svn": "https://v8.googlecode.com/svn/branches/bleeding_edge",
    "from" : ['v8'],
    'viewvc': 'https://code.google.com/p/v8/source/detail?r=',
  },
  'skia/src' : {
    "src" : "src/third_party/skia/src",
    "recurse" : True,
    "svn" : "http://skia.googlecode.com/svn/trunk/src",
    "depends" : ['skia/include', 'skia/gyp'],
    "from" : ['chromium'],
    'viewvc': 'https://code.google.com/p/skia/source/detail?r=',
  },
  'skia/include' : {
    "src" : "src/third_party/skia/include",
    "recurse" : False,
    "svn" : "http://skia.googlecode.com/svn/trunk/include",
    "depends" : None,
    "from" : ['chromium'],
    'viewvc': 'https://code.google.com/p/skia/source/detail?r=',
  },
  'skia/gyp' : {
    "src" : "src/third_party/skia/gyp",
    "recurse" : False,
    "svn" : "http://skia.googlecode.com/svn/trunk/gyp",
    "depends" : None,
    "from" : ['chromium'],
    'viewvc': 'https://code.google.com/p/skia/source/detail?r=',
  },
}

DEPOT_NAMES = DEPOT_DEPS_NAME.keys()
CROS_SDK_PATH = os.path.join('..', 'cros', 'chromite', 'bin', 'cros_sdk')
CROS_VERSION_PATTERN = 'new version number from %s'
CROS_CHROMEOS_PATTERN = 'chromeos-base/chromeos-chrome'
CROS_TEST_KEY_PATH = os.path.join('..', 'cros', 'chromite', 'ssh_keys',
                                  'testing_rsa')
CROS_SCRIPT_KEY_PATH = os.path.join('..', 'cros', 'src', 'scripts',
                                    'mod_for_test_scripts', 'ssh_keys',
                                    'testing_rsa')

BUILD_RESULT_SUCCEED = 0
BUILD_RESULT_FAIL = 1
BUILD_RESULT_SKIPPED = 2


def _AddAdditionalDepotInfo(depot_info):
  """Adds additional depot info to the global depot variables."""
  global DEPOT_DEPS_NAME
  global DEPOT_NAMES
  DEPOT_DEPS_NAME = dict(DEPOT_DEPS_NAME.items() +
      depot_info.items())
  DEPOT_NAMES = DEPOT_DEPS_NAME.keys()


def CalculateTruncatedMean(data_set, truncate_percent):
  """Calculates the truncated mean of a set of values.

  Args:
    data_set: Set of values to use in calculation.
    truncate_percent: The % from the upper/lower portions of the data set to
        discard, expressed as a value in [0, 1].

  Returns:
    The truncated mean as a float.
  """
  if len(data_set) > 2:
    data_set = sorted(data_set)

    discard_num_float = len(data_set) * truncate_percent
    discard_num_int = int(math.floor(discard_num_float))
    kept_weight = len(data_set) - discard_num_float * 2

    data_set = data_set[discard_num_int:len(data_set)-discard_num_int]

    weight_left = 1.0 - (discard_num_float - discard_num_int)

    if weight_left < 1:
      # If the % to discard leaves a fractional portion, need to weight those
      # values.
      unweighted_vals = data_set[1:len(data_set)-1]
      weighted_vals = [data_set[0], data_set[len(data_set)-1]]
      weighted_vals = [w * weight_left for w in weighted_vals]
      data_set = weighted_vals + unweighted_vals
  else:
    kept_weight = len(data_set)

  truncated_mean = reduce(lambda x, y: float(x) + float(y),
                          data_set) / kept_weight

  return truncated_mean


def CalculateStandardDeviation(v):
  if len(v) == 1:
    return 0.0

  mean = CalculateTruncatedMean(v, 0.0)
  variances = [float(x) - mean for x in v]
  variances = [x * x for x in variances]
  variance = reduce(lambda x, y: float(x) + float(y), variances) / (len(v) - 1)
  std_dev = math.sqrt(variance)

  return std_dev


def CalculatePooledStandardError(work_sets):
  numerator = 0.0
  denominator1 = 0.0
  denominator2 = 0.0

  for current_set in work_sets:
    std_dev = CalculateStandardDeviation(current_set)
    numerator += (len(current_set) - 1) * std_dev ** 2
    denominator1 += len(current_set) - 1
    denominator2 += 1.0 / len(current_set)

  if denominator1:
    return math.sqrt(numerator / denominator1) * math.sqrt(denominator2)
  return 0.0


def CalculateStandardError(v):
  if len(v) <= 1:
    return 0.0

  std_dev = CalculateStandardDeviation(v)

  return std_dev / math.sqrt(len(v))


def IsStringFloat(string_to_check):
  """Checks whether or not the given string can be converted to a floating
  point number.

  Args:
    string_to_check: Input string to check if it can be converted to a float.

  Returns:
    True if the string can be converted to a float.
  """
  try:
    float(string_to_check)

    return True
  except ValueError:
    return False


def IsStringInt(string_to_check):
  """Checks whether or not the given string can be converted to a integer.

  Args:
    string_to_check: Input string to check if it can be converted to an int.

  Returns:
    True if the string can be converted to an int.
  """
  try:
    int(string_to_check)

    return True
  except ValueError:
    return False


def IsWindows():
  """Checks whether or not the script is running on Windows.

  Returns:
    True if running on Windows.
  """
  return sys.platform == 'cygwin' or sys.platform.startswith('win')


def Is64BitWindows():
  """Returns whether or not Windows is a 64-bit version.

  Returns:
    True if Windows is 64-bit, False if 32-bit.
  """
  platform = os.environ['PROCESSOR_ARCHITECTURE']
  try:
    platform = os.environ['PROCESSOR_ARCHITEW6432']
  except KeyError:
    # Must not be running in WoW64, so PROCESSOR_ARCHITECTURE is correct
    pass

  return platform in ['AMD64', 'I64']


def IsLinux():
  """Checks whether or not the script is running on Linux.

  Returns:
    True if running on Linux.
  """
  return sys.platform.startswith('linux')


def IsMac():
  """Checks whether or not the script is running on Mac.

  Returns:
    True if running on Mac.
  """
  return sys.platform.startswith('darwin')


def GetZipFileName(build_revision=None, target_arch='ia32'):
  """Gets the archive file name for the given revision."""
  def PlatformName():
    """Return a string to be used in paths for the platform."""
    if IsWindows():
      # Build archive for x64 is still stored with 'win32'suffix
      # (chromium_utils.PlatformName()).
      if Is64BitWindows() and target_arch == 'x64':
        return 'win32'
      return 'win32'
    if IsLinux():
      return 'linux'
    if IsMac():
      return 'mac'
    raise NotImplementedError('Unknown platform "%s".' % sys.platform)

  base_name = 'full-build-%s' % PlatformName()
  if not build_revision:
    return base_name
  return '%s_%s.zip' % (base_name, build_revision)


def GetRemoteBuildPath(build_revision, target_arch='ia32'):
  """Compute the url to download the build from."""
  def GetGSRootFolderName():
    """Gets Google Cloud Storage root folder names"""
    if IsWindows():
      if Is64BitWindows() and target_arch == 'x64':
        return 'Win x64 Builder'
      return 'Win Builder'
    if IsLinux():
      return 'Linux Builder'
    if IsMac():
      return 'Mac Builder'
    raise NotImplementedError('Unsupported Platform "%s".' % sys.platform)

  base_filename = GetZipFileName(build_revision, target_arch)
  builder_folder = GetGSRootFolderName()
  return '%s/%s' % (builder_folder, base_filename)


def FetchFromCloudStorage(bucket_name, source_path, destination_path):
  """Fetches file(s) from the Google Cloud Storage.

  Args:
    bucket_name: Google Storage bucket name.
    source_path: Source file path.
    destination_path: Destination file path.

  Returns:
    True if the fetching succeeds, otherwise False.
  """
  target_file = os.path.join(destination_path, os.path.basename(source_path))
  try:
    if cloud_storage.Exists(bucket_name, source_path):
      print 'Fetching file from gs//%s/%s ...' % (bucket_name, source_path)
      cloud_storage.Get(bucket_name, source_path, destination_path)
      if os.path.exists(target_file):
        return True
    else:
      print ('File gs://%s/%s not found in cloud storage.' % (
          bucket_name, source_path))
  except e:
    print 'Something went wrong while fetching file from cloud: %s' % e
    if os.path.exists(target_file):
       os.remove(target_file)
  return False


# This is copied from Chromium's project build/scripts/common/chromium_utils.py.
def MaybeMakeDirectory(*path):
  """Creates an entire path, if it doesn't already exist."""
  file_path = os.path.join(*path)
  try:
    os.makedirs(file_path)
  except OSError, e:
    if e.errno != errno.EEXIST:
      return False
  return True


# This is copied from Chromium's project build/scripts/common/chromium_utils.py.
def ExtractZip(filename, output_dir, verbose=True):
  """ Extract the zip archive in the output directory."""
  MaybeMakeDirectory(output_dir)

  # On Linux and Mac, we use the unzip command as it will
  # handle links and file bits (executable), which is much
  # easier then trying to do that with ZipInfo options.
  #
  # On Windows, try to use 7z if it is installed, otherwise fall back to python
  # zip module and pray we don't have files larger than 512MB to unzip.
  unzip_cmd = None
  if IsMac() or IsLinux():
    unzip_cmd = ['unzip', '-o']
  elif IsWindows() and os.path.exists('C:\\Program Files\\7-Zip\\7z.exe'):
    unzip_cmd = ['C:\\Program Files\\7-Zip\\7z.exe', 'x', '-y']

  if unzip_cmd:
    # Make sure path is absolute before changing directories.
    filepath = os.path.abspath(filename)
    saved_dir = os.getcwd()
    os.chdir(output_dir)
    command = unzip_cmd + [filepath]
    result = RunProcess(command)
    os.chdir(saved_dir)
    if result:
      raise IOError('unzip failed: %s => %s' % (str(command), result))
  else:
    assert IsWindows()
    zf = zipfile.ZipFile(filename)
    for name in zf.namelist():
      if verbose:
        print 'Extracting %s' % name
      zf.extract(name, output_dir)


def RunProcess(command):
  """Run an arbitrary command. If output from the call is needed, use
  RunProcessAndRetrieveOutput instead.

  Args:
    command: A list containing the command and args to execute.

  Returns:
    The return code of the call.
  """
  # On Windows, use shell=True to get PATH interpretation.
  shell = IsWindows()
  return subprocess.call(command, shell=shell)


def RunProcessAndRetrieveOutput(command, cwd=None):
  """Run an arbitrary command, returning its output and return code. Since
  output is collected via communicate(), there will be no output until the
  call terminates. If you need output while the program runs (ie. so
  that the buildbot doesn't terminate the script), consider RunProcess().

  Args:
    command: A list containing the command and args to execute.

  Returns:
    A tuple of the output and return code.
  """
  # On Windows, use shell=True to get PATH interpretation.
  shell = IsWindows()
  proc = subprocess.Popen(command, shell=shell, stdout=subprocess.PIPE, cwd=cwd)

  (output, _) = proc.communicate()

  return (output, proc.returncode)


def RunGit(command, cwd=None):
  """Run a git subcommand, returning its output and return code.

  Args:
    command: A list containing the args to git.

  Returns:
    A tuple of the output and return code.
  """
  command = ['git'] + command

  return RunProcessAndRetrieveOutput(command, cwd=cwd)


def CheckRunGit(command, cwd=None):
  """Run a git subcommand, returning its output and return code. Asserts if
  the return code of the call is non-zero.

  Args:
    command: A list containing the args to git.

  Returns:
    A tuple of the output and return code.
  """
  (output, return_code) = RunGit(command, cwd=cwd)

  assert not return_code, 'An error occurred while running'\
                          ' "git %s"' % ' '.join(command)
  return output


def SetBuildSystemDefault(build_system):
  """Sets up any environment variables needed to build with the specified build
  system.

  Args:
    build_system: A string specifying build system. Currently only 'ninja' or
        'make' are supported."""
  if build_system == 'ninja':
    gyp_var = os.getenv('GYP_GENERATORS')

    if not gyp_var or not 'ninja' in gyp_var:
      if gyp_var:
        os.environ['GYP_GENERATORS'] = gyp_var + ',ninja'
      else:
        os.environ['GYP_GENERATORS'] = 'ninja'

      if IsWindows():
        os.environ['GYP_DEFINES'] = 'component=shared_library '\
            'incremental_chrome_dll=1 disable_nacl=1 fastbuild=1 '\
            'chromium_win_pch=0'
  elif build_system == 'make':
    os.environ['GYP_GENERATORS'] = 'make'
  else:
    raise RuntimeError('%s build not supported.' % build_system)


def BuildWithMake(threads, targets):
  cmd = ['make', 'BUILDTYPE=Release']

  if threads:
    cmd.append('-j%d' % threads)

  cmd += targets

  return_code = RunProcess(cmd)

  return not return_code


def BuildWithNinja(threads, targets):
  cmd = ['ninja', '-C', os.path.join('out', 'Release')]

  if threads:
    cmd.append('-j%d' % threads)

  cmd += targets

  return_code = RunProcess(cmd)

  return not return_code


def BuildWithVisualStudio(targets):
  path_to_devenv = os.path.abspath(
      os.path.join(os.environ['VS100COMNTOOLS'], '..', 'IDE', 'devenv.com'))
  path_to_sln = os.path.join(os.getcwd(), 'chrome', 'chrome.sln')
  cmd = [path_to_devenv, '/build', 'Release', path_to_sln]

  for t in targets:
    cmd.extend(['/Project', t])

  return_code = RunProcess(cmd)

  return not return_code


class Builder(object):
  """Builder is used by the bisect script to build relevant targets and deploy.
  """
  def __init__(self, opts):
    """Performs setup for building with target build system.

    Args:
        opts: Options parsed from command line.
    """
    if IsWindows():
      if not opts.build_preference:
        opts.build_preference = 'msvs'

      if opts.build_preference == 'msvs':
        if not os.getenv('VS100COMNTOOLS'):
          raise RuntimeError(
              'Path to visual studio could not be determined.')
      else:
        SetBuildSystemDefault(opts.build_preference)
    else:
      if not opts.build_preference:
        if 'ninja' in os.getenv('GYP_GENERATORS'):
          opts.build_preference = 'ninja'
        else:
          opts.build_preference = 'make'

      SetBuildSystemDefault(opts.build_preference)

    if not bisect_utils.SetupPlatformBuildEnvironment(opts):
      raise RuntimeError('Failed to set platform environment.')

    bisect_utils.RunGClient(['runhooks'])

  @staticmethod
  def FromOpts(opts):
    builder = None
    if opts.target_platform == 'cros':
      builder = CrosBuilder(opts)
    elif opts.target_platform == 'android':
      builder = AndroidBuilder(opts)
    elif opts.target_platform == 'android-chrome':
      builder = AndroidChromeBuilder(opts)
    else:
      builder = DesktopBuilder(opts)
    return builder

  def Build(self, depot, opts):
    raise NotImplementedError()

  def GetBuildOutputDirectory(self, opts, src_dir=None):
    raise NotImplementedError()


class DesktopBuilder(Builder):
  """DesktopBuilder is used to build Chromium on linux/mac/windows."""
  def __init__(self, opts):
    super(DesktopBuilder, self).__init__(opts)

  def Build(self, depot, opts):
    """Builds chromium_builder_perf target using options passed into
    the script.

    Args:
        depot: Current depot being bisected.
        opts: The options parsed from the command line.

    Returns:
        True if build was successful.
    """
    targets = ['chromium_builder_perf']

    threads = None
    if opts.use_goma:
      threads = 64

    build_success = False
    if opts.build_preference == 'make':
      build_success = BuildWithMake(threads, targets)
    elif opts.build_preference == 'ninja':
      build_success = BuildWithNinja(threads, targets)
    elif opts.build_preference == 'msvs':
      assert IsWindows(), 'msvs is only supported on Windows.'
      build_success = BuildWithVisualStudio(targets)
    else:
      assert False, 'No build system defined.'
    return build_success

  def GetBuildOutputDirectory(self, opts, src_dir=None):
    """Returns the path to the build directory, relative to the checkout root.

      Assumes that the current working directory is the checkout root.
    """
    src_dir = src_dir or 'src'
    if opts.build_preference == 'ninja' or IsLinux():
      return os.path.join(src_dir, 'out')
    if IsMac():
      return os.path.join(src_dir, 'xcodebuild')
    if IsWindows():
      return os.path.join(src_dir, 'build')
    raise NotImplementedError('Unexpected platform %s' % sys.platform)


class AndroidBuilder(Builder):
  """AndroidBuilder is used to build on android."""
  def __init__(self, opts):
    super(AndroidBuilder, self).__init__(opts)

  def _GetTargets(self):
    return ['chrome_shell', 'cc_perftests_apk', 'android_tools']

  def Build(self, depot, opts):
    """Builds the android content shell and other necessary tools using options
    passed into the script.

    Args:
        depot: Current depot being bisected.
        opts: The options parsed from the command line.

    Returns:
        True if build was successful.
    """
    threads = None
    if opts.use_goma:
      threads = 64

    build_success = False
    if opts.build_preference == 'ninja':
      build_success = BuildWithNinja(threads, self._GetTargets())
    else:
      assert False, 'No build system defined.'

    return build_success


class AndroidChromeBuilder(AndroidBuilder):
  """AndroidBuilder is used to build on android's chrome."""
  def __init__(self, opts):
    super(AndroidChromeBuilder, self).__init__(opts)

  def _GetTargets(self):
    return AndroidBuilder._GetTargets(self) + ['chrome_apk']


class CrosBuilder(Builder):
  """CrosBuilder is used to build and image ChromeOS/Chromium when cros is the
  target platform."""
  def __init__(self, opts):
    super(CrosBuilder, self).__init__(opts)

  def ImageToTarget(self, opts):
    """Installs latest image to target specified by opts.cros_remote_ip.

    Args:
        opts: Program options containing cros_board and cros_remote_ip.

    Returns:
        True if successful.
    """
    try:
      # Keys will most likely be set to 0640 after wiping the chroot.
      os.chmod(CROS_SCRIPT_KEY_PATH, 0600)
      os.chmod(CROS_TEST_KEY_PATH, 0600)
      cmd = [CROS_SDK_PATH, '--', './bin/cros_image_to_target.py',
             '--remote=%s' % opts.cros_remote_ip,
             '--board=%s' % opts.cros_board, '--test', '--verbose']

      return_code = RunProcess(cmd)
      return not return_code
    except OSError, e:
      return False

  def BuildPackages(self, opts, depot):
    """Builds packages for cros.

    Args:
        opts: Program options containing cros_board.
        depot: The depot being bisected.

    Returns:
        True if successful.
    """
    cmd = [CROS_SDK_PATH]

    if depot != 'cros':
      path_to_chrome = os.path.join(os.getcwd(), '..')
      cmd += ['--chrome_root=%s' % path_to_chrome]

    cmd += ['--']

    if depot != 'cros':
      cmd += ['CHROME_ORIGIN=LOCAL_SOURCE']

    cmd += ['BUILDTYPE=Release', './build_packages',
        '--board=%s' % opts.cros_board]
    return_code = RunProcess(cmd)

    return not return_code

  def BuildImage(self, opts, depot):
    """Builds test image for cros.

    Args:
        opts: Program options containing cros_board.
        depot: The depot being bisected.

    Returns:
        True if successful.
    """
    cmd = [CROS_SDK_PATH]

    if depot != 'cros':
      path_to_chrome = os.path.join(os.getcwd(), '..')
      cmd += ['--chrome_root=%s' % path_to_chrome]

    cmd += ['--']

    if depot != 'cros':
      cmd += ['CHROME_ORIGIN=LOCAL_SOURCE']

    cmd += ['BUILDTYPE=Release', '--', './build_image',
        '--board=%s' % opts.cros_board, 'test']

    return_code = RunProcess(cmd)

    return not return_code

  def Build(self, depot, opts):
    """Builds targets using options passed into the script.

    Args:
        depot: Current depot being bisected.
        opts: The options parsed from the command line.

    Returns:
        True if build was successful.
    """
    if self.BuildPackages(opts, depot):
      if self.BuildImage(opts, depot):
        return self.ImageToTarget(opts)
    return False


class SourceControl(object):
  """SourceControl is an abstraction over the underlying source control
  system used for chromium. For now only git is supported, but in the
  future, the svn workflow could be added as well."""
  def __init__(self):
    super(SourceControl, self).__init__()

  def SyncToRevisionWithGClient(self, revision):
    """Uses gclient to sync to the specified revision.

    ie. gclient sync --revision <revision>

    Args:
      revision: The git SHA1 or svn CL (depending on workflow).

    Returns:
      The return code of the call.
    """
    return bisect_utils.RunGClient(['sync', '--revision',
        revision, '--verbose', '--nohooks', '--reset', '--force'])

  def SyncToRevisionWithRepo(self, timestamp):
    """Uses repo to sync all the underlying git depots to the specified
    time.

    Args:
      timestamp: The unix timestamp to sync to.

    Returns:
      The return code of the call.
    """
    return bisect_utils.RunRepoSyncAtTimestamp(timestamp)


class GitSourceControl(SourceControl):
  """GitSourceControl is used to query the underlying source control. """
  def __init__(self, opts):
    super(GitSourceControl, self).__init__()
    self.opts = opts

  def IsGit(self):
    return True

  def GetRevisionList(self, revision_range_end, revision_range_start, cwd=None):
    """Retrieves a list of revisions between |revision_range_start| and
    |revision_range_end|.

    Args:
      revision_range_end: The SHA1 for the end of the range.
      revision_range_start: The SHA1 for the beginning of the range.

    Returns:
      A list of the revisions between |revision_range_start| and
      |revision_range_end| (inclusive).
    """
    revision_range = '%s..%s' % (revision_range_start, revision_range_end)
    cmd = ['log', '--format=%H', '-10000', '--first-parent', revision_range]
    log_output = CheckRunGit(cmd, cwd=cwd)

    revision_hash_list = log_output.split()
    revision_hash_list.append(revision_range_start)

    return revision_hash_list

  def SyncToRevision(self, revision, sync_client=None):
    """Syncs to the specified revision.

    Args:
      revision: The revision to sync to.
      use_gclient: Specifies whether or not we should sync using gclient or
        just use source control directly.

    Returns:
      True if successful.
    """

    if not sync_client:
      results = RunGit(['checkout', revision])[1]
    elif sync_client == 'gclient':
      results = self.SyncToRevisionWithGClient(revision)
    elif sync_client == 'repo':
      results = self.SyncToRevisionWithRepo(revision)

    return not results

  def ResolveToRevision(self, revision_to_check, depot, search, cwd=None):
    """If an SVN revision is supplied, try to resolve it to a git SHA1.

    Args:
      revision_to_check: The user supplied revision string that may need to be
        resolved to a git SHA1.
      depot: The depot the revision_to_check is from.
      search: The number of changelists to try if the first fails to resolve
        to a git hash. If the value is negative, the function will search
        backwards chronologically, otherwise it will search forward.

    Returns:
      A string containing a git SHA1 hash, otherwise None.
    """
    # Android-chrome is git only, so no need to resolve this to anything else.
    if depot == 'android-chrome':
      return revision_to_check

    if depot != 'cros':
      if not IsStringInt(revision_to_check):
        return revision_to_check

      depot_svn = 'svn://svn.chromium.org/chrome/trunk/src'

      if depot != 'chromium':
        depot_svn = DEPOT_DEPS_NAME[depot]['svn']

      svn_revision = int(revision_to_check)
      git_revision = None

      if search > 0:
        search_range = xrange(svn_revision, svn_revision + search, 1)
      else:
        search_range = xrange(svn_revision, svn_revision + search, -1)

      for i in search_range:
        svn_pattern = 'git-svn-id: %s@%d' % (depot_svn, i)
        cmd = ['log', '--format=%H', '-1', '--grep', svn_pattern,
               'origin/master']

        (log_output, return_code) = RunGit(cmd, cwd=cwd)

        assert not return_code, 'An error occurred while running'\
                                ' "git %s"' % ' '.join(cmd)

        if not return_code:
          log_output = log_output.strip()

          if log_output:
            git_revision = log_output

            break

      return git_revision
    else:
      if IsStringInt(revision_to_check):
        return int(revision_to_check)
      else:
        cwd = os.getcwd()
        os.chdir(os.path.join(os.getcwd(), 'src', 'third_party',
            'chromiumos-overlay'))
        pattern = CROS_VERSION_PATTERN % revision_to_check
        cmd = ['log', '--format=%ct', '-1', '--grep', pattern]

        git_revision = None

        log_output = CheckRunGit(cmd, cwd=cwd)
        if log_output:
          git_revision = log_output
          git_revision = int(log_output.strip())
        os.chdir(cwd)

        return git_revision

  def IsInProperBranch(self):
    """Confirms they're in the master branch for performing the bisection.
    This is needed or gclient will fail to sync properly.

    Returns:
      True if the current branch on src is 'master'
    """
    cmd = ['rev-parse', '--abbrev-ref', 'HEAD']
    log_output = CheckRunGit(cmd)
    log_output = log_output.strip()

    return log_output == "master"

  def SVNFindRev(self, revision):
    """Maps directly to the 'git svn find-rev' command.

    Args:
      revision: The git SHA1 to use.

    Returns:
      An integer changelist #, otherwise None.
    """

    cmd = ['svn', 'find-rev', revision]

    output = CheckRunGit(cmd)
    svn_revision = output.strip()

    if IsStringInt(svn_revision):
      return int(svn_revision)

    return None

  def QueryRevisionInfo(self, revision, cwd=None):
    """Gathers information on a particular revision, such as author's name,
    email, subject, and date.

    Args:
      revision: Revision you want to gather information on.
    Returns:
      A dict in the following format:
      {
        'author': %s,
        'email': %s,
        'date': %s,
        'subject': %s,
        'body': %s,
      }
    """
    commit_info = {}

    formats = ['%cN', '%cE', '%s', '%cD', '%b']
    targets = ['author', 'email', 'subject', 'date', 'body']

    for i in xrange(len(formats)):
      cmd = ['log', '--format=%s' % formats[i], '-1', revision]
      output = CheckRunGit(cmd, cwd=cwd)
      commit_info[targets[i]] = output.rstrip()

    return commit_info

  def CheckoutFileAtRevision(self, file_name, revision, cwd=None):
    """Performs a checkout on a file at the given revision.

    Returns:
      True if successful.
    """
    return not RunGit(['checkout', revision, file_name], cwd=cwd)[1]

  def RevertFileToHead(self, file_name):
    """Unstages a file and returns it to HEAD.

    Returns:
      True if successful.
    """
    # Reset doesn't seem to return 0 on success.
    RunGit(['reset', 'HEAD', bisect_utils.FILE_DEPS_GIT])

    return not RunGit(['checkout', bisect_utils.FILE_DEPS_GIT])[1]

  def QueryFileRevisionHistory(self, filename, revision_start, revision_end):
    """Returns a list of commits that modified this file.

    Args:
        filename: Name of file.
        revision_start: Start of revision range.
        revision_end: End of revision range.

    Returns:
        Returns a list of commits that touched this file.
    """
    cmd = ['log', '--format=%H', '%s~1..%s' % (revision_start, revision_end),
           filename]
    output = CheckRunGit(cmd)

    return [o for o in output.split('\n') if o]

class BisectPerformanceMetrics(object):
  """BisectPerformanceMetrics performs a bisection against a list of range
  of revisions to narrow down where performance regressions may have
  occurred."""

  def __init__(self, source_control, opts):
    super(BisectPerformanceMetrics, self).__init__()

    self.opts = opts
    self.source_control = source_control
    self.src_cwd = os.getcwd()
    self.cros_cwd = os.path.join(os.getcwd(), '..', 'cros')
    self.depot_cwd = {}
    self.cleanup_commands = []
    self.warnings = []
    self.builder = Builder.FromOpts(opts)

    # This always starts true since the script grabs latest first.
    self.was_blink = True

    for d in DEPOT_NAMES:
      # The working directory of each depot is just the path to the depot, but
      # since we're already in 'src', we can skip that part.

      self.depot_cwd[d] = os.path.join(
          self.src_cwd, DEPOT_DEPS_NAME[d]['src'][4:])

  def PerformCleanup(self):
    """Performs cleanup when script is finished."""
    os.chdir(self.src_cwd)
    for c in self.cleanup_commands:
      if c[0] == 'mv':
        shutil.move(c[1], c[2])
      else:
        assert False, 'Invalid cleanup command.'

  def GetRevisionList(self, depot, bad_revision, good_revision):
    """Retrieves a list of all the commits between the bad revision and
    last known good revision."""

    revision_work_list = []

    if depot == 'cros':
      revision_range_start = good_revision
      revision_range_end = bad_revision

      cwd = os.getcwd()
      self.ChangeToDepotWorkingDirectory('cros')

      # Print the commit timestamps for every commit in the revision time
      # range. We'll sort them and bisect by that. There is a remote chance that
      # 2 (or more) commits will share the exact same timestamp, but it's
      # probably safe to ignore that case.
      cmd = ['repo', 'forall', '-c',
          'git log --format=%%ct --before=%d --after=%d' % (
          revision_range_end, revision_range_start)]
      (output, return_code) = RunProcessAndRetrieveOutput(cmd)

      assert not return_code, 'An error occurred while running'\
                              ' "%s"' % ' '.join(cmd)

      os.chdir(cwd)

      revision_work_list = list(set(
          [int(o) for o in output.split('\n') if IsStringInt(o)]))
      revision_work_list = sorted(revision_work_list, reverse=True)
    else:
      cwd = self._GetDepotDirectory(depot)
      revision_work_list = self.source_control.GetRevisionList(bad_revision,
          good_revision, cwd=cwd)

    return revision_work_list

  def _GetV8BleedingEdgeFromV8TrunkIfMappable(self, revision):
    svn_revision = self.source_control.SVNFindRev(revision)

    if IsStringInt(svn_revision):
      # V8 is tricky to bisect, in that there are only a few instances when
      # we can dive into bleeding_edge and get back a meaningful result.
      # Try to detect a V8 "business as usual" case, which is when:
      #  1. trunk revision N has description "Version X.Y.Z"
      #  2. bleeding_edge revision (N-1) has description "Prepare push to
      #     trunk. Now working on X.Y.(Z+1)."
      #
      # As of 01/24/2014, V8 trunk descriptions are formatted:
      # "Version 3.X.Y (based on bleeding_edge revision rZ)"
      # So we can just try parsing that out first and fall back to the old way.
      v8_dir = self._GetDepotDirectory('v8')
      v8_bleeding_edge_dir = self._GetDepotDirectory('v8_bleeding_edge')

      revision_info = self.source_control.QueryRevisionInfo(revision,
          cwd=v8_dir)

      version_re = re.compile("Version (?P<values>[0-9,.]+)")

      regex_results = version_re.search(revision_info['subject'])

      if regex_results:
        git_revision = None

        # Look for "based on bleeding_edge" and parse out revision
        if 'based on bleeding_edge' in revision_info['subject']:
          try:
            bleeding_edge_revision = revision_info['subject'].split(
                'bleeding_edge revision r')[1]
            bleeding_edge_revision = int(bleeding_edge_revision.split(')')[0])
            git_revision = self.source_control.ResolveToRevision(
                bleeding_edge_revision, 'v8_bleeding_edge', 1,
                cwd=v8_bleeding_edge_dir)
            return git_revision
          except IndexError, ValueError:
            pass

        if not git_revision:
          # Wasn't successful, try the old way of looking for "Prepare push to"
          git_revision = self.source_control.ResolveToRevision(
              int(svn_revision) - 1, 'v8_bleeding_edge', -1,
              cwd=v8_bleeding_edge_dir)

          if git_revision:
            revision_info = self.source_control.QueryRevisionInfo(git_revision,
                cwd=v8_bleeding_edge_dir)

            if 'Prepare push to trunk' in revision_info['subject']:
              return git_revision
    return None

  def _GetNearestV8BleedingEdgeFromTrunk(self, revision, search_forward=True):
    cwd = self._GetDepotDirectory('v8')
    cmd = ['log', '--format=%ct', '-1', revision]
    output = CheckRunGit(cmd, cwd=cwd)
    commit_time = int(output)
    commits = []

    if search_forward:
      cmd = ['log', '--format=%H', '-10', '--after=%d' % commit_time,
          'origin/master']
      output = CheckRunGit(cmd, cwd=cwd)
      output = output.split()
      commits = output
      commits = reversed(commits)
    else:
      cmd = ['log', '--format=%H', '-10', '--before=%d' % commit_time,
          'origin/master']
      output = CheckRunGit(cmd, cwd=cwd)
      output = output.split()
      commits = output

    bleeding_edge_revision = None

    for c in commits:
      bleeding_edge_revision = self._GetV8BleedingEdgeFromV8TrunkIfMappable(c)
      if bleeding_edge_revision:
        break

    return bleeding_edge_revision

  def Get3rdPartyRevisionsFromCurrentRevision(self, depot, revision):
    """Parses the DEPS file to determine WebKit/v8/etc... versions.

    Returns:
      A dict in the format {depot:revision} if successful, otherwise None.
    """

    cwd = os.getcwd()
    self.ChangeToDepotWorkingDirectory(depot)

    results = {}

    if depot == 'chromium' or depot == 'android-chrome':
      locals = {'Var': lambda _: locals["vars"][_],
                'From': lambda *args: None}
      execfile(bisect_utils.FILE_DEPS_GIT, {}, locals)

      os.chdir(cwd)

      rxp = re.compile(".git@(?P<revision>[a-fA-F0-9]+)")

      for d in DEPOT_NAMES:
        if DEPOT_DEPS_NAME[d].has_key('platform'):
          if DEPOT_DEPS_NAME[d]['platform'] != os.name:
            continue

        if (DEPOT_DEPS_NAME[d]['recurse'] and
            depot in DEPOT_DEPS_NAME[d]['from']):
          if (locals['deps'].has_key(DEPOT_DEPS_NAME[d]['src']) or
              locals['deps'].has_key(DEPOT_DEPS_NAME[d]['src_old'])):
            if locals['deps'].has_key(DEPOT_DEPS_NAME[d]['src']):
              re_results = rxp.search(locals['deps'][DEPOT_DEPS_NAME[d]['src']])
              self.depot_cwd[d] =\
                  os.path.join(self.src_cwd, DEPOT_DEPS_NAME[d]['src'][4:])
            elif locals['deps'].has_key(DEPOT_DEPS_NAME[d]['src_old']):
              re_results =\
                  rxp.search(locals['deps'][DEPOT_DEPS_NAME[d]['src_old']])
              self.depot_cwd[d] =\
                  os.path.join(self.src_cwd, DEPOT_DEPS_NAME[d]['src_old'][4:])

            if re_results:
              results[d] = re_results.group('revision')
            else:
              print 'Couldn\'t parse revision for %s.' % d
              print
              return None
          else:
            print 'Couldn\'t find %s while parsing .DEPS.git.' % d
            print
            return None
    elif depot == 'cros':
      cmd = [CROS_SDK_PATH, '--', 'portageq-%s' % self.opts.cros_board,
             'best_visible', '/build/%s' % self.opts.cros_board, 'ebuild',
             CROS_CHROMEOS_PATTERN]
      (output, return_code) = RunProcessAndRetrieveOutput(cmd)

      assert not return_code, 'An error occurred while running'\
                              ' "%s"' % ' '.join(cmd)

      if len(output) > CROS_CHROMEOS_PATTERN:
        output = output[len(CROS_CHROMEOS_PATTERN):]

      if len(output) > 1:
        output = output.split('_')[0]

        if len(output) > 3:
          contents = output.split('.')

          version = contents[2]

          if contents[3] != '0':
            warningText = 'Chrome version: %s.%s but using %s.0 to bisect.' %\
                (version, contents[3], version)
            if not warningText in self.warnings:
              self.warnings.append(warningText)

          cwd = os.getcwd()
          self.ChangeToDepotWorkingDirectory('chromium')
          return_code = CheckRunGit(['log', '-1', '--format=%H',
              '--author=chrome-release@google.com', '--grep=to %s' % version,
              'origin/master'])
          os.chdir(cwd)

          results['chromium'] = output.strip()
    elif depot == 'v8':
      # We can't try to map the trunk revision to bleeding edge yet, because
      # we don't know which direction to try to search in. Have to wait until
      # the bisect has narrowed the results down to 2 v8 rolls.
      results['v8_bleeding_edge'] = None

    return results

  def BackupOrRestoreOutputdirectory(self, restore=False, build_type='Release'):
    """Backs up or restores build output directory based on restore argument.

    Args:
      restore: Indicates whether to restore or backup. Default is False(Backup)
      build_type: Target build type ('Release', 'Debug', 'Release_x64' etc.)

    Returns:
      Path to backup or restored location as string. otherwise None if it fails.
    """
    build_dir = os.path.abspath(
        self.builder.GetBuildOutputDirectory(self.opts, self.src_cwd))
    source_dir = os.path.join(build_dir, build_type)
    destination_dir = os.path.join(build_dir, '%s.bak' % build_type)
    if restore:
      source_dir, destination_dir = destination_dir, source_dir
    if os.path.exists(source_dir):
      RmTreeAndMkDir(destination_dir, skip_makedir=True)
      shutil.move(source_dir, destination_dir)
      return destination_dir
    return None

  def DownloadCurrentBuild(self, revision, build_type='Release'):
    """Download the build archive for the given revision.

    Args:
      revision: The SVN revision to build.
      build_type: Target build type ('Release', 'Debug', 'Release_x64' etc.)

    Returns:
      True if download succeeds, otherwise False.
    """
    abs_build_dir = os.path.abspath(
        self.builder.GetBuildOutputDirectory(self.opts, self.src_cwd))
    target_build_output_dir = os.path.join(abs_build_dir, build_type)
    # Get build target architecture.
    build_arch = self.opts.target_arch
    # File path of the downloaded archive file.
    archive_file_dest = os.path.join(abs_build_dir,
                                     GetZipFileName(revision, build_arch))
    remote_build = GetRemoteBuildPath(revision, build_arch)
    fetch_build_func = lambda: FetchFromCloudStorage(self.opts.gs_bucket,
                                                     remote_build,
                                                     abs_build_dir)
    if not fetch_build_func():
      if not self.PostBuildRequestAndWait(revision, condition=fetch_build_func):
        raise RuntimeError('Somewthing went wrong while processing build'
                           'request for: %s' % revision)

    # Generic name for the archive, created when archive file is extracted.
    output_dir = os.path.join(abs_build_dir,
                              GetZipFileName(target_arch=build_arch))
    # Unzip build archive directory.
    try:
      RmTreeAndMkDir(output_dir, skip_makedir=True)
      ExtractZip(archive_file_dest, abs_build_dir)
      if os.path.exists(output_dir):
        self.BackupOrRestoreOutputdirectory(restore=False)
        print 'Moving build from %s to %s' % (
            output_dir, target_build_output_dir)
        shutil.move(output_dir, target_build_output_dir)
        return True
      raise IOError('Missing extracted folder %s ' % output_dir)
    except e:
      print 'Somewthing went wrong while extracting archive file: %s' % e
      self.BackupOrRestoreOutputdirectory(restore=True)
      # Cleanup any leftovers from unzipping.
      if os.path.exists(output_dir):
        RmTreeAndMkDir(output_dir, skip_makedir=True)
    finally:
      # Delete downloaded archive
      if os.path.exists(archive_file_dest):
        os.remove(archive_file_dest)
    return False

  def PostBuildRequestAndWait(self, revision, condition, patch=None):
    """POSTs the build request job to the tryserver instance."""

    def GetBuilderNameAndBuildTime(target_arch='ia32'):
      """Gets builder name and buildtime in seconds based on platform."""
      if IsWindows():
        if Is64BitWindows() and target_arch == 'x64':
          return ('Win x64 Bisect Builder', 3600)
        return ('Win Bisect Builder', 3600)
      if IsLinux():
        return ('Linux Bisect Builder', 1800)
      if IsMac():
        return ('Mac Bisect Builder', 2700)
      raise NotImplementedError('Unsupported Platform "%s".' % sys.platform)
    if not condition:
      return False

    bot_name, build_timeout = GetBuilderNameAndBuildTime(self.opts.target_arch)

    # Creates a try job description.
    job_args = {'host': self.opts.builder_host,
                'port': self.opts.builder_port,
                'revision': revision,
                'bot': bot_name,
                'name': 'Bisect Job-%s' % revision
               }
    # Update patch information if supplied.
    if patch:
      job_args['patch'] = patch
    # Posts job to build the revision on the server.
    if post_perf_builder_job.PostTryJob(job_args):
      poll_interval = 60
      start_time = time.time()
      while True:
        res = condition()
        if res:
          return res
        elapsed_time = time.time() - start_time
        if elapsed_time > build_timeout:
          raise RuntimeError('Timed out while waiting %ds for %s build.' %
                             (build_timeout, revision))
        print ('Time elapsed: %ss, still waiting for %s build' %
               (elapsed_time, revision))
        time.sleep(poll_interval)
    return False

  def BuildCurrentRevision(self, depot, revision=None):
    """Builds chrome and performance_ui_tests on the current revision.

    Returns:
      True if the build was successful.
    """
    if self.opts.debug_ignore_build:
      return True
    cwd = os.getcwd()
    os.chdir(self.src_cwd)
    # Fetch build archive for the given revision from the cloud storage when
    # the storage bucket is passed.
    if depot == 'chromium' and self.opts.gs_bucket and revision:
      # Get SVN revision for the given SHA, since builds are archived using SVN
      # revision.
      revision = self.source_control.SVNFindRev(revision)
      if not revision:
        raise RuntimeError(
            'Failed to determine SVN revision for %s' % sha_revision)
      if self.DownloadCurrentBuild(revision):
        os.chdir(cwd)
        return True
      raise RuntimeError('Failed to download build archive for revision %s.\n'
                         'Unfortunately, bisection couldn\'t continue any '
                         'further. Please try running script without '
                         '--gs_bucket flag to produce local builds.' % revision)


    build_success = self.builder.Build(depot, self.opts)
    os.chdir(cwd)
    return build_success

  def RunGClientHooks(self):
    """Runs gclient with runhooks command.

    Returns:
      True if gclient reports no errors.
    """

    if self.opts.debug_ignore_build:
      return True

    return not bisect_utils.RunGClient(['runhooks'], cwd=self.src_cwd)

  def TryParseHistogramValuesFromOutput(self, metric, text):
    """Attempts to parse a metric in the format HISTOGRAM <graph: <trace>.

    Args:
      metric: The metric as a list of [<trace>, <value>] strings.
      text: The text to parse the metric values from.

    Returns:
      A list of floating point numbers found.
    """
    metric_formatted = 'HISTOGRAM %s: %s= ' % (metric[0], metric[1])

    text_lines = text.split('\n')
    values_list = []

    for current_line in text_lines:
      if metric_formatted in current_line:
        current_line = current_line[len(metric_formatted):]

        try:
          histogram_values = eval(current_line)

          for b in histogram_values['buckets']:
            average_for_bucket = float(b['high'] + b['low']) * 0.5
            # Extends the list with N-elements with the average for that bucket.
            values_list.extend([average_for_bucket] * b['count'])
        except:
          pass

    return values_list

  def TryParseResultValuesFromOutput(self, metric, text):
    """Attempts to parse a metric in the format RESULT <graph: <trace>.

    Args:
      metric: The metric as a list of [<trace>, <value>] strings.
      text: The text to parse the metric values from.

    Returns:
      A list of floating point numbers found.
    """
    # Format is: RESULT <graph>: <trace>= <value> <units>
    metric_formatted = re.escape('RESULT %s: %s=' % (metric[0], metric[1]))

    text_lines = text.split('\n')
    values_list = []

    for current_line in text_lines:
      # Parse the output from the performance test for the metric we're
      # interested in.
      metric_re = metric_formatted +\
                  "(\s)*(?P<values>[0-9]+(\.[0-9]*)?)"
      metric_re = re.compile(metric_re)
      regex_results = metric_re.search(current_line)

      if not regex_results is None:
        values_list += [regex_results.group('values')]
      else:
        metric_re = metric_formatted +\
                    "(\s)*\[(\s)*(?P<values>[0-9,.]+)\]"
        metric_re = re.compile(metric_re)
        regex_results = metric_re.search(current_line)

        if not regex_results is None:
          metric_values = regex_results.group('values')

          values_list += metric_values.split(',')

    values_list = [float(v) for v in values_list if IsStringFloat(v)]

    # If the metric is times/t, we need to sum the timings in order to get
    # similar regression results as the try-bots.
    metrics_to_sum = [['times', 't'], ['times', 'page_load_time'],
        ['cold_times', 'page_load_time'], ['warm_times', 'page_load_time']]

    if metric in metrics_to_sum:
      if values_list:
        values_list = [reduce(lambda x, y: float(x) + float(y), values_list)]

    return values_list

  def ParseMetricValuesFromOutput(self, metric, text):
    """Parses output from performance_ui_tests and retrieves the results for
    a given metric.

    Args:
      metric: The metric as a list of [<trace>, <value>] strings.
      text: The text to parse the metric values from.

    Returns:
      A list of floating point numbers found.
    """
    metric_values = self.TryParseResultValuesFromOutput(metric, text)

    if not metric_values:
      metric_values = self.TryParseHistogramValuesFromOutput(metric, text)

    return metric_values

  def _GenerateProfileIfNecessary(self, command_args):
    """Checks the command line of the performance test for dependencies on
    profile generation, and runs tools/perf/generate_profile as necessary.

    Args:
      command_args: Command line being passed to performance test, as a list.

    Returns:
      False if profile generation was necessary and failed, otherwise True.
    """

    if '--profile-dir' in ' '.join(command_args):
      # If we were using python 2.7+, we could just use the argparse
      # module's parse_known_args to grab --profile-dir. Since some of the
      # bots still run 2.6, have to grab the arguments manually.
      arg_dict = {}
      args_to_parse = ['--profile-dir', '--browser']

      for arg_to_parse in args_to_parse:
        for i, current_arg in enumerate(command_args):
          if arg_to_parse in current_arg:
            current_arg_split = current_arg.split('=')

            # Check 2 cases, --arg=<val> and --arg <val>
            if len(current_arg_split) == 2:
              arg_dict[arg_to_parse] = current_arg_split[1]
            elif i + 1 < len(command_args):
              arg_dict[arg_to_parse] = command_args[i+1]

      path_to_generate = os.path.join('tools', 'perf', 'generate_profile')

      if arg_dict.has_key('--profile-dir') and arg_dict.has_key('--browser'):
        profile_path, profile_type = os.path.split(arg_dict['--profile-dir'])
        return not RunProcess(['python', path_to_generate,
            '--profile-type-to-generate', profile_type,
            '--browser', arg_dict['--browser'], '--output-dir', profile_path])
      return False
    return True

  def RunPerformanceTestAndParseResults(self, command_to_run, metric,
      reset_on_first_run=False, upload_on_last_run=False, results_label=None):
    """Runs a performance test on the current revision by executing the
    'command_to_run' and parses the results.

    Args:
      command_to_run: The command to be run to execute the performance test.
      metric: The metric to parse out from the results of the performance test.

    Returns:
      On success, it will return a tuple of the average value of the metric,
      and a success code of 0.
    """

    if self.opts.debug_ignore_perf_test:
      return ({'mean': 0.0, 'std_err': 0.0, 'std_dev': 0.0, 'values': [0.0]}, 0)

    if IsWindows():
      command_to_run = command_to_run.replace('/', r'\\')

    args = shlex.split(command_to_run)

    if not self._GenerateProfileIfNecessary(args):
      return ('Failed to generate profile for performance test.', -1)

    # If running a telemetry test for cros, insert the remote ip, and
    # identity parameters.
    is_telemetry = bisect_utils.IsTelemetryCommand(command_to_run)
    if self.opts.target_platform == 'cros' and is_telemetry:
      args.append('--remote=%s' % self.opts.cros_remote_ip)
      args.append('--identity=%s' % CROS_TEST_KEY_PATH)

    cwd = os.getcwd()
    os.chdir(self.src_cwd)

    start_time = time.time()

    metric_values = []
    output_of_all_runs = ''
    for i in xrange(self.opts.repeat_test_count):
      # Can ignore the return code since if the tests fail, it won't return 0.
      try:
        current_args = copy.copy(args)
        if is_telemetry:
          if i == 0 and reset_on_first_run:
            current_args.append('--reset-results')
          elif i == self.opts.repeat_test_count - 1 and upload_on_last_run:
            current_args.append('--upload-results')
          if results_label:
            current_args.append('--results-label=%s' % results_label)
        (output, return_code) = RunProcessAndRetrieveOutput(current_args)
      except OSError, e:
        if e.errno == errno.ENOENT:
          err_text  = ("Something went wrong running the performance test. "
              "Please review the command line:\n\n")
          if 'src/' in ' '.join(args):
            err_text += ("Check that you haven't accidentally specified a path "
                "with src/ in the command.\n\n")
          err_text += ' '.join(args)
          err_text += '\n'

          return (err_text, -1)
        raise

      output_of_all_runs += output
      if self.opts.output_buildbot_annotations:
        print output

      metric_values += self.ParseMetricValuesFromOutput(metric, output)

      elapsed_minutes = (time.time() - start_time) / 60.0

      if elapsed_minutes >= self.opts.max_time_minutes or not metric_values:
        break

    os.chdir(cwd)

    # Need to get the average value if there were multiple values.
    if metric_values:
      truncated_mean = CalculateTruncatedMean(metric_values,
          self.opts.truncate_percent)
      standard_err = CalculateStandardError(metric_values)
      standard_dev = CalculateStandardDeviation(metric_values)

      values = {
        'mean': truncated_mean,
        'std_err': standard_err,
        'std_dev': standard_dev,
        'values': metric_values,
      }

      print 'Results of performance test: %12f %12f' % (
          truncated_mean, standard_err)
      print
      return (values, 0, output_of_all_runs)
    else:
      return ('Invalid metric specified, or no values returned from '
          'performance test.', -1, output_of_all_runs)

  def FindAllRevisionsToSync(self, revision, depot):
    """Finds all dependant revisions and depots that need to be synced for a
    given revision. This is only useful in the git workflow, as an svn depot
    may be split into multiple mirrors.

    ie. skia is broken up into 3 git mirrors over skia/src, skia/gyp, and
    skia/include. To sync skia/src properly, one has to find the proper
    revisions in skia/gyp and skia/include.

    Args:
      revision: The revision to sync to.
      depot: The depot in use at the moment (probably skia).

    Returns:
      A list of [depot, revision] pairs that need to be synced.
    """
    revisions_to_sync = [[depot, revision]]

    is_base = ((depot == 'chromium') or (depot == 'cros') or
        (depot == 'android-chrome'))

    # Some SVN depots were split into multiple git depots, so we need to
    # figure out for each mirror which git revision to grab. There's no
    # guarantee that the SVN revision will exist for each of the dependant
    # depots, so we have to grep the git logs and grab the next earlier one.
    if not is_base and\
       DEPOT_DEPS_NAME[depot]['depends'] and\
       self.source_control.IsGit():
      svn_rev = self.source_control.SVNFindRev(revision)

      for d in DEPOT_DEPS_NAME[depot]['depends']:
        self.ChangeToDepotWorkingDirectory(d)

        dependant_rev = self.source_control.ResolveToRevision(svn_rev, d, -1000)

        if dependant_rev:
          revisions_to_sync.append([d, dependant_rev])

      num_resolved = len(revisions_to_sync)
      num_needed = len(DEPOT_DEPS_NAME[depot]['depends'])

      self.ChangeToDepotWorkingDirectory(depot)

      if not ((num_resolved - 1) == num_needed):
        return None

    return revisions_to_sync

  def PerformPreBuildCleanup(self):
    """Performs necessary cleanup between runs."""
    print 'Cleaning up between runs.'
    print

    # Having these pyc files around between runs can confuse the
    # perf tests and cause them to crash.
    for (path, dir, files) in os.walk(self.src_cwd):
      for cur_file in files:
        if cur_file.endswith('.pyc'):
          path_to_file = os.path.join(path, cur_file)
          os.remove(path_to_file)

  def PerformWebkitDirectoryCleanup(self, revision):
    """If the script is switching between Blink and WebKit during bisect,
    its faster to just delete the directory rather than leave it up to git
    to sync.

    Returns:
      True if successful.
    """
    if not self.source_control.CheckoutFileAtRevision(
        bisect_utils.FILE_DEPS_GIT, revision, cwd=self.src_cwd):
      return False

    cwd = os.getcwd()
    os.chdir(self.src_cwd)

    is_blink = bisect_utils.IsDepsFileBlink()

    os.chdir(cwd)

    if not self.source_control.RevertFileToHead(
        bisect_utils.FILE_DEPS_GIT):
      return False

    if self.was_blink != is_blink:
      self.was_blink = is_blink
      return bisect_utils.RemoveThirdPartyWebkitDirectory()
    return True

  def PerformCrosChrootCleanup(self):
    """Deletes the chroot.

    Returns:
        True if successful.
    """
    cwd = os.getcwd()
    self.ChangeToDepotWorkingDirectory('cros')
    cmd = [CROS_SDK_PATH, '--delete']
    return_code = RunProcess(cmd)
    os.chdir(cwd)
    return not return_code

  def CreateCrosChroot(self):
    """Creates a new chroot.

    Returns:
        True if successful.
    """
    cwd = os.getcwd()
    self.ChangeToDepotWorkingDirectory('cros')
    cmd = [CROS_SDK_PATH, '--create']
    return_code = RunProcess(cmd)
    os.chdir(cwd)
    return not return_code

  def PerformPreSyncCleanup(self, revision, depot):
    """Performs any necessary cleanup before syncing.

    Returns:
      True if successful.
    """
    if depot == 'chromium':
      if not bisect_utils.RemoveThirdPartyLibjingleDirectory():
        return False
      return self.PerformWebkitDirectoryCleanup(revision)
    elif depot == 'cros':
      return self.PerformCrosChrootCleanup()
    return True

  def RunPostSync(self, depot):
    """Performs any work after syncing.

    Returns:
      True if successful.
    """
    if self.opts.target_platform == 'android':
      if not bisect_utils.SetupAndroidBuildEnvironment(self.opts,
          path_to_src=self.src_cwd):
        return False

    if depot == 'cros':
      return self.CreateCrosChroot()
    else:
      return self.RunGClientHooks()
    return True

  def ShouldSkipRevision(self, depot, revision):
    """Some commits can be safely skipped (such as a DEPS roll), since the tool
    is git based those changes would have no effect.

    Args:
      depot: The depot being bisected.
      revision: Current revision we're synced to.

    Returns:
      True if we should skip building/testing this revision.
    """
    if depot == 'chromium':
      if self.source_control.IsGit():
        cmd = ['diff-tree', '--no-commit-id', '--name-only', '-r', revision]
        output = CheckRunGit(cmd)

        files = output.splitlines()

        if len(files) == 1 and files[0] == 'DEPS':
          return True

    return False

  def SyncBuildAndRunRevision(self, revision, depot, command_to_run, metric,
      skippable=False):
    """Performs a full sync/build/run of the specified revision.

    Args:
      revision: The revision to sync to.
      depot: The depot that's being used at the moment (src, webkit, etc.)
      command_to_run: The command to execute the performance test.
      metric: The performance metric being tested.

    Returns:
      On success, a tuple containing the results of the performance test.
      Otherwise, a tuple with the error message.
    """
    sync_client = None
    if depot == 'chromium' or depot == 'android-chrome':
      sync_client = 'gclient'
    elif depot == 'cros':
      sync_client = 'repo'

    revisions_to_sync = self.FindAllRevisionsToSync(revision, depot)

    if not revisions_to_sync:
      return ('Failed to resolve dependant depots.', BUILD_RESULT_FAIL)

    if not self.PerformPreSyncCleanup(revision, depot):
      return ('Failed to perform pre-sync cleanup.', BUILD_RESULT_FAIL)

    success = True

    if not self.opts.debug_ignore_sync:
      for r in revisions_to_sync:
        self.ChangeToDepotWorkingDirectory(r[0])

        if sync_client:
          self.PerformPreBuildCleanup()

        # If you're using gclient to sync, you need to specify the depot you
        # want so that all the dependencies sync properly as well.
        # ie. gclient sync src@<SHA1>
        current_revision = r[1]
        if sync_client == 'gclient':
          current_revision = '%s@%s' % (DEPOT_DEPS_NAME[depot]['src'],
              current_revision)
        if not self.source_control.SyncToRevision(current_revision,
            sync_client):
          success = False

          break

    if success:
      success = self.RunPostSync(depot)
      if success:
        if skippable and self.ShouldSkipRevision(depot, revision):
          return ('Skipped revision: [%s]' % str(revision),
              BUILD_RESULT_SKIPPED)

        start_build_time = time.time()
        if self.BuildCurrentRevision(depot, revision):
          after_build_time = time.time()
          results = self.RunPerformanceTestAndParseResults(command_to_run,
                                                           metric)
          # Restore build output directory once the tests are done, to avoid
          # any descrepancy.
          if depot == 'chromium' and self.opts.gs_bucket and revision:
            self.BackupOrRestoreOutputdirectory(restore=True)

          if results[1] == 0:
            external_revisions = self.Get3rdPartyRevisionsFromCurrentRevision(
                depot, revision)

            if not external_revisions is None:
              return (results[0], results[1], external_revisions,
                  time.time() - after_build_time, after_build_time -
                  start_build_time)
            else:
              return ('Failed to parse DEPS file for external revisions.',
                  BUILD_RESULT_FAIL)
          else:
            return results
        else:
          return ('Failed to build revision: [%s]' % (str(revision, )),
              BUILD_RESULT_FAIL)
      else:
        return ('Failed to run [gclient runhooks].', BUILD_RESULT_FAIL)
    else:
      return ('Failed to sync revision: [%s]' % (str(revision, )),
          BUILD_RESULT_FAIL)

  def CheckIfRunPassed(self, current_value, known_good_value, known_bad_value):
    """Given known good and bad values, decide if the current_value passed
    or failed.

    Args:
      current_value: The value of the metric being checked.
      known_bad_value: The reference value for a "failed" run.
      known_good_value: The reference value for a "passed" run.

    Returns:
      True if the current_value is closer to the known_good_value than the
      known_bad_value.
    """
    dist_to_good_value = abs(current_value['mean'] - known_good_value['mean'])
    dist_to_bad_value = abs(current_value['mean'] - known_bad_value['mean'])

    return dist_to_good_value < dist_to_bad_value

  def _GetDepotDirectory(self, depot_name):
    if depot_name == 'chromium':
      return self.src_cwd
    elif depot_name == 'cros':
      return self.cros_cwd
    elif depot_name in DEPOT_NAMES:
      return self.depot_cwd[depot_name]
    else:
      assert False, 'Unknown depot [ %s ] encountered. Possibly a new one'\
                    ' was added without proper support?' %\
                    (depot_name,)

  def ChangeToDepotWorkingDirectory(self, depot_name):
    """Given a depot, changes to the appropriate working directory.

    Args:
      depot_name: The name of the depot (see DEPOT_NAMES).
    """
    os.chdir(self._GetDepotDirectory(depot_name))

  def _FillInV8BleedingEdgeInfo(self, min_revision_data, max_revision_data):
    r1 = self._GetNearestV8BleedingEdgeFromTrunk(min_revision_data['revision'],
        search_forward=True)
    r2 = self._GetNearestV8BleedingEdgeFromTrunk(max_revision_data['revision'],
        search_forward=False)
    min_revision_data['external']['v8_bleeding_edge'] = r1
    max_revision_data['external']['v8_bleeding_edge'] = r2

    if (not self._GetV8BleedingEdgeFromV8TrunkIfMappable(
            min_revision_data['revision']) or
        not self._GetV8BleedingEdgeFromV8TrunkIfMappable(
            max_revision_data['revision'])):
      self.warnings.append('Trunk revisions in V8 did not map directly to '
          'bleeding_edge. Attempted to expand the range to find V8 rolls which '
          'did map directly to bleeding_edge revisions, but results might not '
          'be valid.')

  def _FindNextDepotToBisect(self, current_depot, current_revision,
      min_revision_data, max_revision_data):
    """Given the state of the bisect, decides which depot the script should
    dive into next (if any).

    Args:
      current_depot: Current depot being bisected.
      current_revision: Current revision synced to.
      min_revision_data: Data about the earliest revision in the bisect range.
      max_revision_data: Data about the latest revision in the bisect range.

    Returns:
      The depot to bisect next, or None.
    """
    external_depot = None
    for next_depot in DEPOT_NAMES:
      if DEPOT_DEPS_NAME[next_depot].has_key('platform'):
        if DEPOT_DEPS_NAME[next_depot]['platform'] != os.name:
          continue

      if not (DEPOT_DEPS_NAME[next_depot]["recurse"] and
          min_revision_data['depot'] in DEPOT_DEPS_NAME[next_depot]['from']):
        continue

      if current_depot == 'v8':
        # We grab the bleeding_edge info here rather than earlier because we
        # finally have the revision range. From that we can search forwards and
        # backwards to try to match trunk revisions to bleeding_edge.
        self._FillInV8BleedingEdgeInfo(min_revision_data, max_revision_data)

      if (min_revision_data['external'][next_depot] ==
          max_revision_data['external'][next_depot]):
        continue

      if (min_revision_data['external'][next_depot] and
          max_revision_data['external'][next_depot]):
        external_depot = next_depot
        break

    return external_depot

  def PrepareToBisectOnDepot(self,
                             current_depot,
                             end_revision,
                             start_revision,
                             previous_depot,
                             previous_revision):
    """Changes to the appropriate directory and gathers a list of revisions
    to bisect between |start_revision| and |end_revision|.

    Args:
      current_depot: The depot we want to bisect.
      end_revision: End of the revision range.
      start_revision: Start of the revision range.
      previous_depot: The depot we were previously bisecting.
      previous_revision: The last revision we synced to on |previous_depot|.

    Returns:
      A list containing the revisions between |start_revision| and
      |end_revision| inclusive.
    """
    # Change into working directory of external library to run
    # subsequent commands.
    self.ChangeToDepotWorkingDirectory(current_depot)

    # V8 (and possibly others) is merged in periodically. Bisecting
    # this directory directly won't give much good info.
    if DEPOT_DEPS_NAME[current_depot].has_key('custom_deps'):
      config_path = os.path.join(self.src_cwd, '..')
      if bisect_utils.RunGClientAndCreateConfig(self.opts,
          DEPOT_DEPS_NAME[current_depot]['custom_deps'], cwd=config_path):
        return []
      if bisect_utils.RunGClient(
          ['sync', '--revision', previous_revision], cwd=self.src_cwd):
        return []

    if current_depot == 'v8_bleeding_edge':
      self.ChangeToDepotWorkingDirectory('chromium')

      shutil.move('v8', 'v8.bak')
      shutil.move('v8_bleeding_edge', 'v8')

      self.cleanup_commands.append(['mv', 'v8', 'v8_bleeding_edge'])
      self.cleanup_commands.append(['mv', 'v8.bak', 'v8'])

      self.depot_cwd['v8_bleeding_edge'] = os.path.join(self.src_cwd, 'v8')
      self.depot_cwd['v8'] = os.path.join(self.src_cwd, 'v8.bak')

      self.ChangeToDepotWorkingDirectory(current_depot)

    depot_revision_list = self.GetRevisionList(current_depot,
                                               end_revision,
                                               start_revision)

    self.ChangeToDepotWorkingDirectory('chromium')

    return depot_revision_list

  def GatherReferenceValues(self, good_rev, bad_rev, cmd, metric, target_depot):
    """Gathers reference values by running the performance tests on the
    known good and bad revisions.

    Args:
      good_rev: The last known good revision where the performance regression
        has not occurred yet.
      bad_rev: A revision where the performance regression has already occurred.
      cmd: The command to execute the performance test.
      metric: The metric being tested for regression.

    Returns:
      A tuple with the results of building and running each revision.
    """
    bad_run_results = self.SyncBuildAndRunRevision(bad_rev,
                                                   target_depot,
                                                   cmd,
                                                   metric)

    good_run_results = None

    if not bad_run_results[1]:
      good_run_results = self.SyncBuildAndRunRevision(good_rev,
                                                      target_depot,
                                                      cmd,
                                                      metric)

    return (bad_run_results, good_run_results)

  def AddRevisionsIntoRevisionData(self, revisions, depot, sort, revision_data):
    """Adds new revisions to the revision_data dict and initializes them.

    Args:
      revisions: List of revisions to add.
      depot: Depot that's currently in use (src, webkit, etc...)
      sort: Sorting key for displaying revisions.
      revision_data: A dict to add the new revisions into. Existing revisions
        will have their sort keys offset.
    """

    num_depot_revisions = len(revisions)

    for k, v in revision_data.iteritems():
      if v['sort'] > sort:
        v['sort'] += num_depot_revisions

    for i in xrange(num_depot_revisions):
      r = revisions[i]

      revision_data[r] = {'revision' : r,
                          'depot' : depot,
                          'value' : None,
                          'perf_time' : 0,
                          'build_time' : 0,
                          'passed' : '?',
                          'sort' : i + sort + 1}

  def PrintRevisionsToBisectMessage(self, revision_list, depot):
    if self.opts.output_buildbot_annotations:
      step_name = 'Bisection Range: [%s - %s]' % (
          revision_list[len(revision_list)-1], revision_list[0])
      bisect_utils.OutputAnnotationStepStart(step_name)

    print
    print 'Revisions to bisect on [%s]:' % depot
    for revision_id in revision_list:
      print '  -> %s' % (revision_id, )
    print

    if self.opts.output_buildbot_annotations:
      bisect_utils.OutputAnnotationStepClosed()

  def NudgeRevisionsIfDEPSChange(self, bad_revision, good_revision):
    """Checks to see if changes to DEPS file occurred, and that the revision
    range also includes the change to .DEPS.git. If it doesn't, attempts to
    expand the revision range to include it.

    Args:
        bad_rev: First known bad revision.
        good_revision: Last known good revision.

    Returns:
        A tuple with the new bad and good revisions.
    """
    if self.source_control.IsGit() and self.opts.target_platform == 'chromium':
      changes_to_deps = self.source_control.QueryFileRevisionHistory(
          'DEPS', good_revision, bad_revision)

      if changes_to_deps:
        # DEPS file was changed, search from the oldest change to DEPS file to
        # bad_revision to see if there are matching .DEPS.git changes.
        oldest_deps_change = changes_to_deps[-1]
        changes_to_gitdeps = self.source_control.QueryFileRevisionHistory(
            bisect_utils.FILE_DEPS_GIT, oldest_deps_change, bad_revision)

        if len(changes_to_deps) != len(changes_to_gitdeps):
          # Grab the timestamp of the last DEPS change
          cmd = ['log', '--format=%ct', '-1', changes_to_deps[0]]
          output = CheckRunGit(cmd)
          commit_time = int(output)

          # Try looking for a commit that touches the .DEPS.git file in the
          # next 15 minutes after the DEPS file change.
          cmd = ['log', '--format=%H', '-1',
              '--before=%d' % (commit_time + 900), '--after=%d' % commit_time,
              'origin/master', bisect_utils.FILE_DEPS_GIT]
          output = CheckRunGit(cmd)
          output = output.strip()
          if output:
            self.warnings.append('Detected change to DEPS and modified '
                'revision range to include change to .DEPS.git')
            return (output, good_revision)
          else:
            self.warnings.append('Detected change to DEPS but couldn\'t find '
                'matching change to .DEPS.git')
    return (bad_revision, good_revision)

  def CheckIfRevisionsInProperOrder(self,
                                    target_depot,
                                    good_revision,
                                    bad_revision):
    """Checks that |good_revision| is an earlier revision than |bad_revision|.

    Args:
        good_revision: Number/tag of the known good revision.
        bad_revision: Number/tag of the known bad revision.

    Returns:
        True if the revisions are in the proper order (good earlier than bad).
    """
    if self.source_control.IsGit() and target_depot != 'cros':
      cmd = ['log', '--format=%ct', '-1', good_revision]
      cwd = self._GetDepotDirectory(target_depot)

      output = CheckRunGit(cmd, cwd=cwd)
      good_commit_time = int(output)

      cmd = ['log', '--format=%ct', '-1', bad_revision]
      output = CheckRunGit(cmd, cwd=cwd)
      bad_commit_time = int(output)

      return good_commit_time <= bad_commit_time
    else:
      # Cros/svn use integers
      return int(good_revision) <= int(bad_revision)

  def Run(self, command_to_run, bad_revision_in, good_revision_in, metric):
    """Given known good and bad revisions, run a binary search on all
    intermediate revisions to determine the CL where the performance regression
    occurred.

    Args:
        command_to_run: Specify the command to execute the performance test.
        good_revision: Number/tag of the known good revision.
        bad_revision: Number/tag of the known bad revision.
        metric: The performance metric to monitor.

    Returns:
        A dict with 2 members, 'revision_data' and 'error'. On success,
        'revision_data' will contain a dict mapping revision ids to
        data about that revision. Each piece of revision data consists of a
        dict with the following keys:

        'passed': Represents whether the performance test was successful at
            that revision. Possible values include: 1 (passed), 0 (failed),
            '?' (skipped), 'F' (build failed).
        'depot': The depot that this revision is from (ie. WebKit)
        'external': If the revision is a 'src' revision, 'external' contains
            the revisions of each of the external libraries.
        'sort': A sort value for sorting the dict in order of commits.

        For example:
        {
          'error':None,
          'revision_data':
          {
            'CL #1':
            {
              'passed':False,
              'depot':'chromium',
              'external':None,
              'sort':0
            }
          }
        }

        If an error occurred, the 'error' field will contain the message and
        'revision_data' will be empty.
    """
    results = {'revision_data' : {},
               'error' : None}

    # Choose depot to bisect first
    target_depot = 'chromium'
    if self.opts.target_platform == 'cros':
      target_depot = 'cros'
    elif self.opts.target_platform == 'android-chrome':
      target_depot = 'android-chrome'

    cwd = os.getcwd()
    self.ChangeToDepotWorkingDirectory(target_depot)

    # If they passed SVN CL's, etc... we can try match them to git SHA1's.
    bad_revision = self.source_control.ResolveToRevision(bad_revision_in,
                                                         target_depot, 100)
    good_revision = self.source_control.ResolveToRevision(good_revision_in,
                                                          target_depot, -100)

    os.chdir(cwd)


    if bad_revision is None:
      results['error'] = 'Could\'t resolve [%s] to SHA1.' % (bad_revision_in,)
      return results

    if good_revision is None:
      results['error'] = 'Could\'t resolve [%s] to SHA1.' % (good_revision_in,)
      return results

    # Check that they didn't accidentally swap good and bad revisions.
    if not self.CheckIfRevisionsInProperOrder(
        target_depot, good_revision, bad_revision):
      results['error'] = 'bad_revision < good_revision, did you swap these '\
          'by mistake?'
      return results

    (bad_revision, good_revision) = self.NudgeRevisionsIfDEPSChange(
        bad_revision, good_revision)

    if self.opts.output_buildbot_annotations:
      bisect_utils.OutputAnnotationStepStart('Gathering Revisions')

    print 'Gathering revision range for bisection.'
    # Retrieve a list of revisions to do bisection on.
    src_revision_list = self.GetRevisionList(target_depot,
                                             bad_revision,
                                             good_revision)

    if self.opts.output_buildbot_annotations:
      bisect_utils.OutputAnnotationStepClosed()

    if src_revision_list:
      # revision_data will store information about a revision such as the
      # depot it came from, the webkit/V8 revision at that time,
      # performance timing, build state, etc...
      revision_data = results['revision_data']

      # revision_list is the list we're binary searching through at the moment.
      revision_list = []

      sort_key_ids = 0

      for current_revision_id in src_revision_list:
        sort_key_ids += 1

        revision_data[current_revision_id] = {'value' : None,
                                              'passed' : '?',
                                              'depot' : target_depot,
                                              'external' : None,
                                              'perf_time' : 0,
                                              'build_time' : 0,
                                              'sort' : sort_key_ids}
        revision_list.append(current_revision_id)

      min_revision = 0
      max_revision = len(revision_list) - 1

      self.PrintRevisionsToBisectMessage(revision_list, target_depot)

      if self.opts.output_buildbot_annotations:
        bisect_utils.OutputAnnotationStepStart('Gathering Reference Values')

      print 'Gathering reference values for bisection.'

      # Perform the performance tests on the good and bad revisions, to get
      # reference values.
      (bad_results, good_results) = self.GatherReferenceValues(good_revision,
                                                               bad_revision,
                                                               command_to_run,
                                                               metric,
                                                               target_depot)

      if self.opts.output_buildbot_annotations:
        bisect_utils.OutputAnnotationStepClosed()

      if bad_results[1]:
        results['error'] = ('An error occurred while building and running '
            'the \'bad\' reference value. The bisect cannot continue without '
            'a working \'bad\' revision to start from.\n\nError: %s' %
                bad_results[0])
        return results

      if good_results[1]:
        results['error'] = ('An error occurred while building and running '
            'the \'good\' reference value. The bisect cannot continue without '
            'a working \'good\' revision to start from.\n\nError: %s' %
                good_results[0])
        return results


      # We need these reference values to determine if later runs should be
      # classified as pass or fail.
      known_bad_value = bad_results[0]
      known_good_value = good_results[0]

      # Can just mark the good and bad revisions explicitly here since we
      # already know the results.
      bad_revision_data = revision_data[revision_list[0]]
      bad_revision_data['external'] = bad_results[2]
      bad_revision_data['perf_time'] = bad_results[3]
      bad_revision_data['build_time'] = bad_results[4]
      bad_revision_data['passed'] = False
      bad_revision_data['value'] = known_bad_value

      good_revision_data = revision_data[revision_list[max_revision]]
      good_revision_data['external'] = good_results[2]
      good_revision_data['perf_time'] = good_results[3]
      good_revision_data['build_time'] = good_results[4]
      good_revision_data['passed'] = True
      good_revision_data['value'] = known_good_value

      next_revision_depot = target_depot

      while True:
        if not revision_list:
          break

        min_revision_data = revision_data[revision_list[min_revision]]
        max_revision_data = revision_data[revision_list[max_revision]]

        if max_revision - min_revision <= 1:
          current_depot = min_revision_data['depot']
          if min_revision_data['passed'] == '?':
            next_revision_index = min_revision
          elif max_revision_data['passed'] == '?':
            next_revision_index = max_revision
          elif current_depot in ['android-chrome', 'cros', 'chromium', 'v8']:
            previous_revision = revision_list[min_revision]
            # If there were changes to any of the external libraries we track,
            # should bisect the changes there as well.
            external_depot = self._FindNextDepotToBisect(current_depot,
                previous_revision, min_revision_data, max_revision_data)

            # If there was no change in any of the external depots, the search
            # is over.
            if not external_depot:
              if current_depot == 'v8':
                self.warnings.append('Unfortunately, V8 bisection couldn\'t '
                    'continue any further. The script can only bisect into '
                    'V8\'s bleeding_edge repository if both the current and '
                    'previous revisions in trunk map directly to revisions in '
                    'bleeding_edge.')
              break

            earliest_revision = max_revision_data['external'][external_depot]
            latest_revision = min_revision_data['external'][external_depot]

            new_revision_list = self.PrepareToBisectOnDepot(external_depot,
                                                            latest_revision,
                                                            earliest_revision,
                                                            next_revision_depot,
                                                            previous_revision)

            if not new_revision_list:
              results['error'] = 'An error occurred attempting to retrieve'\
                                 ' revision range: [%s..%s]' %\
                                 (earliest_revision, latest_revision)
              return results

            self.AddRevisionsIntoRevisionData(new_revision_list,
                                              external_depot,
                                              min_revision_data['sort'],
                                              revision_data)

            # Reset the bisection and perform it on the newly inserted
            # changelists.
            revision_list = new_revision_list
            min_revision = 0
            max_revision = len(revision_list) - 1
            sort_key_ids += len(revision_list)

            print 'Regression in metric:%s appears to be the result of changes'\
                  ' in [%s].' % (metric, external_depot)

            self.PrintRevisionsToBisectMessage(revision_list, external_depot)

            continue
          else:
            break
        else:
          next_revision_index = int((max_revision - min_revision) / 2) +\
                                min_revision

        next_revision_id = revision_list[next_revision_index]
        next_revision_data = revision_data[next_revision_id]
        next_revision_depot = next_revision_data['depot']

        self.ChangeToDepotWorkingDirectory(next_revision_depot)

        if self.opts.output_buildbot_annotations:
          step_name = 'Working on [%s]' % next_revision_id
          bisect_utils.OutputAnnotationStepStart(step_name)

        print 'Working on revision: [%s]' % next_revision_id

        run_results = self.SyncBuildAndRunRevision(next_revision_id,
                                                   next_revision_depot,
                                                   command_to_run,
                                                   metric, skippable=True)

        # If the build is successful, check whether or not the metric
        # had regressed.
        if not run_results[1]:
          if len(run_results) > 2:
            next_revision_data['external'] = run_results[2]
            next_revision_data['perf_time'] = run_results[3]
            next_revision_data['build_time'] = run_results[4]

          passed_regression = self.CheckIfRunPassed(run_results[0],
                                                    known_good_value,
                                                    known_bad_value)

          next_revision_data['passed'] = passed_regression
          next_revision_data['value'] = run_results[0]

          if passed_regression:
            max_revision = next_revision_index
          else:
            min_revision = next_revision_index
        else:
          if run_results[1] == BUILD_RESULT_SKIPPED:
            next_revision_data['passed'] = 'Skipped'
          elif run_results[1] == BUILD_RESULT_FAIL:
            next_revision_data['passed'] = 'Build Failed'

          print run_results[0]

          # If the build is broken, remove it and redo search.
          revision_list.pop(next_revision_index)

          max_revision -= 1

        if self.opts.output_buildbot_annotations:
          self._PrintPartialResults(results)
          bisect_utils.OutputAnnotationStepClosed()
    else:
      # Weren't able to sync and retrieve the revision range.
      results['error'] = 'An error occurred attempting to retrieve revision '\
                         'range: [%s..%s]' % (good_revision, bad_revision)

    return results

  def _PrintPartialResults(self, results_dict):
    revision_data = results_dict['revision_data']
    revision_data_sorted = sorted(revision_data.iteritems(),
                                  key = lambda x: x[1]['sort'])
    results_dict = self._GetResultsDict(revision_data, revision_data_sorted)
    first_working_revision = results_dict['first_working_revision']
    last_broken_revision = results_dict['last_broken_revision']

    self._PrintTestedCommitsTable(revision_data_sorted,
                                  results_dict['first_working_revision'],
                                  results_dict['last_broken_revision'],
                                  100, final_step=False)

  def _PrintConfidence(self, results_dict):
    # The perf dashboard specifically looks for the string
    # "Confidence in Bisection Results: 100%" to decide whether or not
    # to cc the author(s). If you change this, please update the perf
    # dashboard as well.
    print 'Confidence in Bisection Results: %d%%' % results_dict['confidence']

  def _PrintBanner(self, results_dict):
    print
    print " __o_\___          Aw Snap! We hit a speed bump!"
    print "=-O----O-'__.~.___________________________________"
    print
    print 'Bisect reproduced a %.02f%% (+-%.02f%%) change in the %s metric.' % (
        results_dict['regression_size'], results_dict['regression_std_err'],
        '/'.join(self.opts.metric))
    self._PrintConfidence(results_dict)

  def _PrintFailedBanner(self, results_dict):
    print
    print ('Bisect could not reproduce a change in the '
        '%s/%s metric.' % (self.opts.metric[0], self.opts.metric[1]))
    print
    self._PrintConfidence(results_dict)

  def _GetViewVCLinkFromDepotAndHash(self, cl, depot):
    info = self.source_control.QueryRevisionInfo(cl,
        self._GetDepotDirectory(depot))
    if depot and DEPOT_DEPS_NAME[depot].has_key('viewvc'):
      try:
        # Format is "git-svn-id: svn://....@123456 <other data>"
        svn_line = [i for i in info['body'].splitlines() if 'git-svn-id:' in i]
        svn_revision = svn_line[0].split('@')
        svn_revision = svn_revision[1].split(' ')[0]
        return DEPOT_DEPS_NAME[depot]['viewvc'] + svn_revision
      except IndexError:
        return ''
    return ''

  def _PrintRevisionInfo(self, cl, info, depot=None):
    # The perf dashboard specifically looks for the string
    # "Author  : " to parse out who to cc on a bug. If you change the
    # formatting here, please update the perf dashboard as well.
    print
    print 'Subject : %s' % info['subject']
    print 'Author  : %s' % info['author']
    if not info['email'].startswith(info['author']):
      print 'Email   : %s' % info['email']
    commit_link = self._GetViewVCLinkFromDepotAndHash(cl, depot)
    if commit_link:
      print 'Link    : %s' % commit_link
    else:
      print
      print 'Failed to parse svn revision from body:'
      print
      print info['body']
      print
    print 'Commit  : %s' % cl
    print 'Date    : %s' % info['date']

  def _PrintTestedCommitsTable(self, revision_data_sorted,
      first_working_revision, last_broken_revision, confidence,
      final_step=True):
    print
    if final_step:
      print 'Tested commits:'
    else:
      print 'Partial results:'
    print '  %20s  %70s  %12s %14s %13s' % ('Depot'.center(20, ' '),
        'Commit SHA'.center(70, ' '), 'Mean'.center(12, ' '),
        'Std. Error'.center(14, ' '), 'State'.center(13, ' '))
    state = 0
    for current_id, current_data in revision_data_sorted:
      if current_data['value']:
        if (current_id == last_broken_revision or
            current_id == first_working_revision):
          # If confidence is too low, don't add this empty line since it's
          # used to put focus on a suspected CL.
          if confidence and final_step:
            print
          state += 1
          if state == 2 and not final_step:
            # Just want a separation between "bad" and "good" cl's.
            print

        state_str = 'Bad'
        if state == 1 and final_step:
          state_str = 'Suspected CL'
        elif state == 2:
          state_str = 'Good'

        # If confidence is too low, don't bother outputting good/bad.
        if not confidence:
          state_str = ''
        state_str = state_str.center(13, ' ')

        std_error = ('+-%.02f' %
            current_data['value']['std_err']).center(14, ' ')
        mean = ('%.02f' % current_data['value']['mean']).center(12, ' ')
        cl_link = self._GetViewVCLinkFromDepotAndHash(current_id,
            current_data['depot'])
        if not cl_link:
          cl_link = current_id
        print '  %20s  %70s  %12s %14s %13s' % (
            current_data['depot'].center(20, ' '), cl_link.center(70, ' '),
            mean, std_error, state_str)

  def _PrintReproSteps(self):
    print
    print 'To reproduce locally:'
    print '$ ' + self.opts.command
    if bisect_utils.IsTelemetryCommand(self.opts.command):
      print
      print 'Also consider passing --profiler=list to see available profilers.'

  def _PrintOtherRegressions(self, other_regressions, revision_data):
    print
    print 'Other regressions may have occurred:'
    print '  %8s  %70s  %10s' % ('Depot'.center(8, ' '),
        'Range'.center(70, ' '), 'Confidence'.center(10, ' '))
    for regression in other_regressions:
      current_id, previous_id, confidence = regression
      current_data = revision_data[current_id]
      previous_data = revision_data[previous_id]

      current_link = self._GetViewVCLinkFromDepotAndHash(current_id,
          current_data['depot'])
      previous_link = self._GetViewVCLinkFromDepotAndHash(previous_id,
          previous_data['depot'])

      # If we can't map it to a viewable URL, at least show the original hash.
      if not current_link:
        current_link = current_id
      if not previous_link:
        previous_link = previous_id

      print '  %8s  %70s %s' % (
          current_data['depot'], current_link,
          ('%d%%' % confidence).center(10, ' '))
      print '  %8s  %70s' % (
          previous_data['depot'], previous_link)
      print

  def _PrintStepTime(self, revision_data_sorted):
    step_perf_time_avg = 0.0
    step_build_time_avg = 0.0
    step_count = 0.0
    for _, current_data in revision_data_sorted:
      if current_data['value']:
        step_perf_time_avg += current_data['perf_time']
        step_build_time_avg += current_data['build_time']
        step_count += 1
    if step_count:
      step_perf_time_avg = step_perf_time_avg / step_count
      step_build_time_avg = step_build_time_avg / step_count
    print
    print 'Average build time : %s' % datetime.timedelta(
        seconds=int(step_build_time_avg))
    print 'Average test time  : %s' % datetime.timedelta(
        seconds=int(step_perf_time_avg))

  def _PrintWarnings(self):
    if not self.warnings:
      return
    print
    print 'WARNINGS:'
    for w in set(self.warnings):
      print '  !!! %s' % w

  def _FindOtherRegressions(self, revision_data_sorted, bad_greater_than_good):
    other_regressions = []
    previous_values = []
    previous_id = None
    for current_id, current_data in revision_data_sorted:
      current_values = current_data['value']
      if current_values:
        current_values = current_values['values']
        if previous_values:
          confidence = self._CalculateConfidence(previous_values,
              [current_values])
          mean_of_prev_runs = CalculateTruncatedMean(
              sum(previous_values, []), 0)
          mean_of_current_runs = CalculateTruncatedMean(current_values, 0)

          # Check that the potential regression is in the same direction as
          # the overall regression. If the mean of the previous runs < the
          # mean of the current runs, this local regression is in same
          # direction.
          prev_less_than_current = mean_of_prev_runs < mean_of_current_runs
          is_same_direction = (prev_less_than_current if
              bad_greater_than_good else not prev_less_than_current)

          # Only report potential regressions with high confidence.
          if is_same_direction and confidence > 50:
            other_regressions.append([current_id, previous_id, confidence])
        previous_values.append(current_values)
        previous_id = current_id
    return other_regressions

  def _CalculateConfidence(self, working_means, broken_means):
    bounds_working = []
    bounds_broken = []
    for m in working_means:
      current_mean = CalculateTruncatedMean(m, 0)
      if bounds_working:
        bounds_working[0] = min(current_mean, bounds_working[0])
        bounds_working[1] = max(current_mean, bounds_working[0])
      else:
        bounds_working = [current_mean, current_mean]
    for m in broken_means:
      current_mean = CalculateTruncatedMean(m, 0)
      if bounds_broken:
        bounds_broken[0] = min(current_mean, bounds_broken[0])
        bounds_broken[1] = max(current_mean, bounds_broken[0])
      else:
        bounds_broken = [current_mean, current_mean]
    dist_between_groups = min(math.fabs(bounds_broken[1] - bounds_working[0]),
        math.fabs(bounds_broken[0] - bounds_working[1]))
    working_mean = sum(working_means, [])
    broken_mean = sum(broken_means, [])
    len_working_group = CalculateStandardDeviation(working_mean)
    len_broken_group = CalculateStandardDeviation(broken_mean)

    confidence = (dist_between_groups / (
        max(0.0001, (len_broken_group + len_working_group ))))
    confidence = int(min(1.0, max(confidence, 0.0)) * 100.0)
    return confidence

  def _GetResultsDict(self, revision_data, revision_data_sorted):
    # Find range where it possibly broke.
    first_working_revision = None
    first_working_revision_index = -1
    last_broken_revision = None
    last_broken_revision_index = -1

    for i in xrange(len(revision_data_sorted)):
      k, v = revision_data_sorted[i]
      if v['passed'] == 1:
        if not first_working_revision:
          first_working_revision = k
          first_working_revision_index = i

      if not v['passed']:
        last_broken_revision = k
        last_broken_revision_index = i

    if last_broken_revision != None and first_working_revision != None:
      broken_means = []
      for i in xrange(0, last_broken_revision_index + 1):
        if revision_data_sorted[i][1]['value']:
          broken_means.append(revision_data_sorted[i][1]['value']['values'])

      working_means = []
      for i in xrange(first_working_revision_index, len(revision_data_sorted)):
        if revision_data_sorted[i][1]['value']:
          working_means.append(revision_data_sorted[i][1]['value']['values'])

      # Flatten the lists to calculate mean of all values.
      working_mean = sum(working_means, [])
      broken_mean = sum(broken_means, [])

      # Calculate the approximate size of the regression
      mean_of_bad_runs = CalculateTruncatedMean(broken_mean, 0.0)
      mean_of_good_runs = CalculateTruncatedMean(working_mean, 0.0)

      regression_size = math.fabs(max(mean_of_good_runs, mean_of_bad_runs) /
          max(0.0001, min(mean_of_good_runs, mean_of_bad_runs))) * 100.0 - 100.0

      regression_std_err = math.fabs(CalculatePooledStandardError(
          [working_mean, broken_mean]) /
          max(0.0001, min(mean_of_good_runs, mean_of_bad_runs))) * 100.0

      # Give a "confidence" in the bisect. At the moment we use how distinct the
      # values are before and after the last broken revision, and how noisy the
      # overall graph is.
      confidence = self._CalculateConfidence(working_means, broken_means)

      culprit_revisions = []

      cwd = os.getcwd()
      self.ChangeToDepotWorkingDirectory(
          revision_data[last_broken_revision]['depot'])

      if revision_data[last_broken_revision]['depot'] == 'cros':
        # Want to get a list of all the commits and what depots they belong
        # to so that we can grab info about each.
        cmd = ['repo', 'forall', '-c',
            'pwd ; git log --pretty=oneline --before=%d --after=%d' % (
            last_broken_revision, first_working_revision + 1)]
        (output, return_code) = RunProcessAndRetrieveOutput(cmd)

        changes = []
        assert not return_code, 'An error occurred while running'\
                                ' "%s"' % ' '.join(cmd)
        last_depot = None
        cwd = os.getcwd()
        for l in output.split('\n'):
          if l:
            # Output will be in form:
            # /path_to_depot
            # /path_to_other_depot
            # <SHA1>
            # /path_again
            # <SHA1>
            # etc.
            if l[0] == '/':
              last_depot = l
            else:
              contents = l.split(' ')
              if len(contents) > 1:
                changes.append([last_depot, contents[0]])
        for c in changes:
          os.chdir(c[0])
          info = self.source_control.QueryRevisionInfo(c[1])
          culprit_revisions.append((c[1], info, None))
      else:
        for i in xrange(last_broken_revision_index, len(revision_data_sorted)):
          k, v = revision_data_sorted[i]
          if k == first_working_revision:
            break
          self.ChangeToDepotWorkingDirectory(v['depot'])
          info = self.source_control.QueryRevisionInfo(k)
          culprit_revisions.append((k, info, v['depot']))
      os.chdir(cwd)

      # Check for any other possible regression ranges
      other_regressions = self._FindOtherRegressions(revision_data_sorted,
          mean_of_bad_runs > mean_of_good_runs)

    # Check for warnings:
    if len(culprit_revisions) > 1:
      self.warnings.append('Due to build errors, regression range could '
                           'not be narrowed down to a single commit.')
    if self.opts.repeat_test_count == 1:
      self.warnings.append('Tests were only set to run once. This may '
                           'be insufficient to get meaningful results.')
    if confidence < 100:
      if confidence:
        self.warnings.append(
            'Confidence is less than 100%. There could be other candidates for '
            'this regression. Try bisecting again with increased repeat_count '
            'or on a sub-metric that shows the regression more clearly.')
      else:
        self.warnings.append(
          'Confidence is 0%. Try bisecting again on another platform, with '
          'increased repeat_count or on a sub-metric that shows the regression '
          'more clearly.')

    return {
        'first_working_revision': first_working_revision,
        'last_broken_revision': last_broken_revision,
        'culprit_revisions': culprit_revisions,
        'other_regressions': other_regressions,
        'regression_size': regression_size,
        'regression_std_err': regression_std_err,
        'confidence': confidence,
        }

  def FormatAndPrintResults(self, bisect_results):
    """Prints the results from a bisection run in a readable format.

    Args
      bisect_results: The results from a bisection test run.
    """
    revision_data = bisect_results['revision_data']
    revision_data_sorted = sorted(revision_data.iteritems(),
                                  key = lambda x: x[1]['sort'])
    results_dict = self._GetResultsDict(revision_data, revision_data_sorted)

    if self.opts.output_buildbot_annotations:
      bisect_utils.OutputAnnotationStepStart('Build Status Per Revision')

    print
    print 'Full results of bisection:'
    for current_id, current_data  in revision_data_sorted:
      build_status = current_data['passed']

      if type(build_status) is bool:
        if build_status:
          build_status = 'Good'
        else:
          build_status = 'Bad'

      print '  %20s  %40s  %s' % (current_data['depot'],
                                  current_id, build_status)
    print

    if self.opts.output_buildbot_annotations:
      bisect_utils.OutputAnnotationStepClosed()
      # The perf dashboard scrapes the "results" step in order to comment on
      # bugs. If you change this, please update the perf dashboard as well.
      bisect_utils.OutputAnnotationStepStart('Results')

    if results_dict['culprit_revisions'] and results_dict['confidence']:
      self._PrintBanner(results_dict)
      for culprit in results_dict['culprit_revisions']:
        cl, info, depot = culprit
        self._PrintRevisionInfo(cl, info, depot)
      self._PrintReproSteps()
      if results_dict['other_regressions']:
        self._PrintOtherRegressions(results_dict['other_regressions'],
                                    revision_data)
    else:
      self._PrintFailedBanner(results_dict)
      self._PrintReproSteps()

    self._PrintTestedCommitsTable(revision_data_sorted,
                                  results_dict['first_working_revision'],
                                  results_dict['last_broken_revision'],
                                  results_dict['confidence'])
    self._PrintStepTime(revision_data_sorted)
    self._PrintWarnings()

    if self.opts.output_buildbot_annotations:
      bisect_utils.OutputAnnotationStepClosed()


def DetermineAndCreateSourceControl(opts):
  """Attempts to determine the underlying source control workflow and returns
  a SourceControl object.

  Returns:
    An instance of a SourceControl object, or None if the current workflow
    is unsupported.
  """

  (output, return_code) = RunGit(['rev-parse', '--is-inside-work-tree'])

  if output.strip() == 'true':
    return GitSourceControl(opts)

  return None


def IsPlatformSupported(opts):
  """Checks that this platform and build system are supported.

  Args:
    opts: The options parsed from the command line.

  Returns:
    True if the platform and build system are supported.
  """
  # Haven't tested the script out on any other platforms yet.
  supported = ['posix', 'nt']
  return os.name in supported


def RmTreeAndMkDir(path_to_dir, skip_makedir=False):
  """Removes the directory tree specified, and then creates an empty
  directory in the same location (if not specified to skip).

  Args:
    path_to_dir: Path to the directory tree.
    skip_makedir: Whether to skip creating empty directory, default is False.

  Returns:
    True if successful, False if an error occurred.
  """
  try:
    if os.path.exists(path_to_dir):
      shutil.rmtree(path_to_dir)
  except OSError, e:
    if e.errno != errno.ENOENT:
      return False

  if not skip_makedir:
    return MaybeMakeDirectory(path_to_dir)

  return True


def RemoveBuildFiles():
  """Removes build files from previous runs."""
  if RmTreeAndMkDir(os.path.join('out', 'Release')):
    if RmTreeAndMkDir(os.path.join('build', 'Release')):
      return True
  return False


class BisectOptions(object):
  """Options to be used when running bisection."""
  def __init__(self):
    super(BisectOptions, self).__init__()

    self.target_platform = 'chromium'
    self.build_preference = None
    self.good_revision = None
    self.bad_revision = None
    self.use_goma = None
    self.cros_board = None
    self.cros_remote_ip = None
    self.repeat_test_count = 20
    self.truncate_percent = 25
    self.max_time_minutes = 20
    self.metric = None
    self.command = None
    self.output_buildbot_annotations = None
    self.no_custom_deps = False
    self.working_directory = None
    self.extra_src = None
    self.debug_ignore_build = None
    self.debug_ignore_sync = None
    self.debug_ignore_perf_test = None
    self.gs_bucket = None
    self.target_arch = 'ia32'
    self.builder_host = None
    self.builder_port = None

  def _CreateCommandLineParser(self):
    """Creates a parser with bisect options.

    Returns:
      An instance of optparse.OptionParser.
    """
    usage = ('%prog [options] [-- chromium-options]\n'
             'Perform binary search on revision history to find a minimal '
             'range of revisions where a peformance metric regressed.\n')

    parser = optparse.OptionParser(usage=usage)

    group = optparse.OptionGroup(parser, 'Bisect options')
    group.add_option('-c', '--command',
                     type='str',
                     help='A command to execute your performance test at' +
                     ' each point in the bisection.')
    group.add_option('-b', '--bad_revision',
                     type='str',
                     help='A bad revision to start bisection. ' +
                     'Must be later than good revision. May be either a git' +
                     ' or svn revision.')
    group.add_option('-g', '--good_revision',
                     type='str',
                     help='A revision to start bisection where performance' +
                     ' test is known to pass. Must be earlier than the ' +
                     'bad revision. May be either a git or svn revision.')
    group.add_option('-m', '--metric',
                     type='str',
                     help='The desired metric to bisect on. For example ' +
                     '"vm_rss_final_b/vm_rss_f_b"')
    group.add_option('-r', '--repeat_test_count',
                     type='int',
                     default=20,
                     help='The number of times to repeat the performance '
                     'test. Values will be clamped to range [1, 100]. '
                     'Default value is 20.')
    group.add_option('--max_time_minutes',
                     type='int',
                     default=20,
                     help='The maximum time (in minutes) to take running the '
                     'performance tests. The script will run the performance '
                     'tests according to --repeat_test_count, so long as it '
                     'doesn\'t exceed --max_time_minutes. Values will be '
                     'clamped to range [1, 60].'
                     'Default value is 20.')
    group.add_option('-t', '--truncate_percent',
                     type='int',
                     default=25,
                     help='The highest/lowest % are discarded to form a '
                     'truncated mean. Values will be clamped to range [0, '
                     '25]. Default value is 25 (highest/lowest 25% will be '
                     'discarded).')
    parser.add_option_group(group)

    group = optparse.OptionGroup(parser, 'Build options')
    group.add_option('-w', '--working_directory',
                     type='str',
                     help='Path to the working directory where the script '
                     'will do an initial checkout of the chromium depot. The '
                     'files will be placed in a subdirectory "bisect" under '
                     'working_directory and that will be used to perform the '
                     'bisection. This parameter is optional, if it is not '
                     'supplied, the script will work from the current depot.')
    group.add_option('--build_preference',
                     type='choice',
                     choices=['msvs', 'ninja', 'make'],
                     help='The preferred build system to use. On linux/mac '
                     'the options are make/ninja. On Windows, the options '
                     'are msvs/ninja.')
    group.add_option('--target_platform',
                     type='choice',
                     choices=['chromium', 'cros', 'android', 'android-chrome'],
                     default='chromium',
                     help='The target platform. Choices are "chromium" '
                     '(current platform), "cros", or "android". If you '
                     'specify something other than "chromium", you must be '
                     'properly set up to build that platform.')
    group.add_option('--no_custom_deps',
                     dest='no_custom_deps',
                     action="store_true",
                     default=False,
                     help='Run the script with custom_deps or not.')
    group.add_option('--extra_src',
                     type='str',
                     help='Path to a script which can be used to modify '
                     'the bisect script\'s behavior.')
    group.add_option('--cros_board',
                     type='str',
                     help='The cros board type to build.')
    group.add_option('--cros_remote_ip',
                     type='str',
                     help='The remote machine to image to.')
    group.add_option('--use_goma',
                     action="store_true",
                     help='Add a bunch of extra threads for goma.')
    group.add_option('--output_buildbot_annotations',
                     action="store_true",
                     help='Add extra annotation output for buildbot.')
    group.add_option('--gs_bucket',
                     default='',
                     dest='gs_bucket',
                     type='str',
                     help=('Name of Google Storage bucket to upload or '
                     'download build. e.g., chrome-perf'))
    group.add_option('--target_arch',
                     type='choice',
                     choices=['ia32', 'x64', 'arm'],
                     default='ia32',
                     dest='target_arch',
                     help=('The target build architecture. Choices are "ia32" '
                     '(default), "x64" or "arm".'))
    group.add_option('--builder_host',
                     dest='builder_host',
                     type='str',
                     help=('Host address of server to produce build by posting'
                           ' try job request.'))
    group.add_option('--builder_port',
                     dest='builder_port',
                     type='int',
                     help=('HTTP port of the server to produce build by posting'
                           ' try job request.'))
    parser.add_option_group(group)

    group = optparse.OptionGroup(parser, 'Debug options')
    group.add_option('--debug_ignore_build',
                     action="store_true",
                     help='DEBUG: Don\'t perform builds.')
    group.add_option('--debug_ignore_sync',
                     action="store_true",
                     help='DEBUG: Don\'t perform syncs.')
    group.add_option('--debug_ignore_perf_test',
                     action="store_true",
                     help='DEBUG: Don\'t perform performance tests.')
    parser.add_option_group(group)
    return parser

  def ParseCommandLine(self):
    """Parses the command line for bisect options."""
    parser = self._CreateCommandLineParser()
    (opts, args) = parser.parse_args()

    try:
      if not opts.command:
        raise RuntimeError('missing required parameter: --command')

      if not opts.good_revision:
        raise RuntimeError('missing required parameter: --good_revision')

      if not opts.bad_revision:
        raise RuntimeError('missing required parameter: --bad_revision')

      if not opts.metric:
        raise RuntimeError('missing required parameter: --metric')

      if opts.gs_bucket:
        if not cloud_storage.List(opts.gs_bucket):
          raise RuntimeError('Invalid Google Storage: gs://%s' % opts.gs_bucket)
        if not opts.builder_host:
          raise RuntimeError('Must specify try server hostname, when '
                             'gs_bucket is used: --builder_host')
        if not opts.builder_port:
          raise RuntimeError('Must specify try server port number, when '
                             'gs_bucket is used: --builder_port')
      if opts.target_platform == 'cros':
        # Run sudo up front to make sure credentials are cached for later.
        print 'Sudo is required to build cros:'
        print
        RunProcess(['sudo', 'true'])

        if not opts.cros_board:
          raise RuntimeError('missing required parameter: --cros_board')

        if not opts.cros_remote_ip:
          raise RuntimeError('missing required parameter: --cros_remote_ip')

        if not opts.working_directory:
          raise RuntimeError('missing required parameter: --working_directory')

      metric_values = opts.metric.split('/')
      if len(metric_values) != 2:
        raise RuntimeError("Invalid metric specified: [%s]" % opts.metric)

      opts.metric = metric_values
      opts.repeat_test_count = min(max(opts.repeat_test_count, 1), 100)
      opts.max_time_minutes = min(max(opts.max_time_minutes, 1), 60)
      opts.truncate_percent = min(max(opts.truncate_percent, 0), 25)
      opts.truncate_percent = opts.truncate_percent / 100.0

      for k, v in opts.__dict__.iteritems():
        assert hasattr(self, k), "Invalid %s attribute in BisectOptions." % k
        setattr(self, k, v)
    except RuntimeError, e:
      output_string = StringIO.StringIO()
      parser.print_help(file=output_string)
      error_message = '%s\n\n%s' % (e.message, output_string.getvalue())
      output_string.close()
      raise RuntimeError(error_message)

  @staticmethod
  def FromDict(values):
    """Creates an instance of BisectOptions with the values parsed from a
    .cfg file.

    Args:
      values: a dict containing options to set.

    Returns:
      An instance of BisectOptions.
    """
    opts = BisectOptions()
    for k, v in values.iteritems():
      assert hasattr(opts, k), 'Invalid %s attribute in '\
          'BisectOptions.' % k
      setattr(opts, k, v)

    metric_values = opts.metric.split('/')
    if len(metric_values) != 2:
      raise RuntimeError("Invalid metric specified: [%s]" % opts.metric)

    opts.metric = metric_values
    opts.repeat_test_count = min(max(opts.repeat_test_count, 1), 100)
    opts.max_time_minutes = min(max(opts.max_time_minutes, 1), 60)
    opts.truncate_percent = min(max(opts.truncate_percent, 0), 25)
    opts.truncate_percent = opts.truncate_percent / 100.0

    return opts


def main():

  try:
    opts = BisectOptions()
    parse_results = opts.ParseCommandLine()

    if opts.extra_src:
      extra_src = bisect_utils.LoadExtraSrc(opts.extra_src)
      if not extra_src:
        raise RuntimeError("Invalid or missing --extra_src.")
      _AddAdditionalDepotInfo(extra_src.GetAdditionalDepotInfo())

    if opts.working_directory:
      custom_deps = bisect_utils.DEFAULT_GCLIENT_CUSTOM_DEPS
      if opts.no_custom_deps:
        custom_deps = None
      bisect_utils.CreateBisectDirectoryAndSetupDepot(opts, custom_deps)

      os.chdir(os.path.join(os.getcwd(), 'src'))

      if not RemoveBuildFiles():
        raise RuntimeError('Something went wrong removing the build files.')

    if not IsPlatformSupported(opts):
      raise RuntimeError("Sorry, this platform isn't supported yet.")

    # Check what source control method they're using. Only support git workflow
    # at the moment.
    source_control = DetermineAndCreateSourceControl(opts)

    if not source_control:
      raise RuntimeError("Sorry, only the git workflow is supported at the "
          "moment.")

    # gClient sync seems to fail if you're not in master branch.
    if (not source_control.IsInProperBranch() and
        not opts.debug_ignore_sync and
        not opts.working_directory):
      raise RuntimeError("You must switch to master branch to run bisection.")
    bisect_test = BisectPerformanceMetrics(source_control, opts)
    try:
      bisect_results = bisect_test.Run(opts.command,
                                       opts.bad_revision,
                                       opts.good_revision,
                                       opts.metric)
      if bisect_results['error']:
        raise RuntimeError(bisect_results['error'])
      bisect_test.FormatAndPrintResults(bisect_results)
      return 0
    finally:
      bisect_test.PerformCleanup()
  except RuntimeError, e:
    if opts.output_buildbot_annotations:
      # The perf dashboard scrapes the "results" step in order to comment on
      # bugs. If you change this, please update the perf dashboard as well.
      bisect_utils.OutputAnnotationStepStart('Results')
    print 'Error: %s' % e.message
    if opts.output_buildbot_annotations:
      bisect_utils.OutputAnnotationStepClosed()
  return 1

if __name__ == '__main__':
  sys.exit(main())
