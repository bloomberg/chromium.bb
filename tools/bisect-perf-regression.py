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
import hashlib
import math
import optparse
import os
import re
import shlex
import shutil
import StringIO
import sys
import time
import zipfile

sys.path.append(os.path.join(os.path.dirname(__file__), 'telemetry'))

from auto_bisect import bisect_utils
from auto_bisect import math_utils
from auto_bisect import post_perf_builder_job as bisect_builder
from auto_bisect import source_control as source_control_module
from auto_bisect import ttest
from telemetry.util import cloud_storage

# Below is the map of "depot" names to information about each depot. Each depot
# is a repository, and in the process of bisecting, revision ranges in these
# repositories may also be bisected.
#
# Each depot information dictionary may contain:
#   src: Path to the working directory.
#   recurse: True if this repository will get bisected.
#   depends: A list of other repositories that are actually part of the same
#       repository in svn. If the repository has any dependent repositories
#       (e.g. skia/src needs skia/include and skia/gyp to be updated), then
#       they are specified here.
#   svn: URL of SVN repository. Needed for git workflow to resolve hashes to
#       SVN revisions.
#   from: Parent depot that must be bisected before this is bisected.
#   deps_var: Key name in vars variable in DEPS file that has revision
#       information.
DEPOT_DEPS_NAME = {
    'chromium': {
        'src': 'src',
        'recurse': True,
        'depends': None,
        'from': ['cros', 'android-chrome'],
        'viewvc':
            'http://src.chromium.org/viewvc/chrome?view=revision&revision=',
        'deps_var': 'chromium_rev'
    },
    'webkit': {
        'src': 'src/third_party/WebKit',
        'recurse': True,
        'depends': None,
        'from': ['chromium'],
        'viewvc':
            'http://src.chromium.org/viewvc/blink?view=revision&revision=',
        'deps_var': 'webkit_revision'
    },
    'angle': {
        'src': 'src/third_party/angle',
        'src_old': 'src/third_party/angle_dx11',
        'recurse': True,
        'depends': None,
        'from': ['chromium'],
        'platform': 'nt',
        'deps_var': 'angle_revision'
    },
    'v8': {
        'src': 'src/v8',
        'recurse': True,
        'depends': None,
        'from': ['chromium'],
        'custom_deps': bisect_utils.GCLIENT_CUSTOM_DEPS_V8,
        'viewvc': 'https://code.google.com/p/v8/source/detail?r=',
        'deps_var': 'v8_revision'
    },
    'v8_bleeding_edge': {
        'src': 'src/v8_bleeding_edge',
        'recurse': True,
        'depends': None,
        'svn': 'https://v8.googlecode.com/svn/branches/bleeding_edge',
        'from': ['v8'],
        'viewvc': 'https://code.google.com/p/v8/source/detail?r=',
        'deps_var': 'v8_revision'
    },
    'skia/src': {
        'src': 'src/third_party/skia/src',
        'recurse': True,
        'svn': 'http://skia.googlecode.com/svn/trunk/src',
        'depends': ['skia/include', 'skia/gyp'],
        'from': ['chromium'],
        'viewvc': 'https://code.google.com/p/skia/source/detail?r=',
        'deps_var': 'skia_revision'
    },
    'skia/include': {
        'src': 'src/third_party/skia/include',
        'recurse': False,
        'svn': 'http://skia.googlecode.com/svn/trunk/include',
        'depends': None,
        'from': ['chromium'],
        'viewvc': 'https://code.google.com/p/skia/source/detail?r=',
        'deps_var': 'None'
    },
    'skia/gyp': {
        'src': 'src/third_party/skia/gyp',
        'recurse': False,
        'svn': 'http://skia.googlecode.com/svn/trunk/gyp',
        'depends': None,
        'from': ['chromium'],
        'viewvc': 'https://code.google.com/p/skia/source/detail?r=',
        'deps_var': 'None'
    }
}

DEPOT_NAMES = DEPOT_DEPS_NAME.keys()

CROS_SDK_PATH = os.path.join('..', 'cros', 'chromite', 'bin', 'cros_sdk')
CROS_CHROMEOS_PATTERN = 'chromeos-base/chromeos-chrome'
CROS_TEST_KEY_PATH = os.path.join('..', 'cros', 'chromite', 'ssh_keys',
                                  'testing_rsa')
CROS_SCRIPT_KEY_PATH = os.path.join('..', 'cros', 'src', 'scripts',
                                    'mod_for_test_scripts', 'ssh_keys',
                                    'testing_rsa')

# Possible return values from BisectPerformanceMetrics.SyncBuildAndRunRevision.
BUILD_RESULT_SUCCEED = 0
BUILD_RESULT_FAIL = 1
BUILD_RESULT_SKIPPED = 2

# Maximum time in seconds to wait after posting build request to tryserver.
# TODO: Change these values based on the actual time taken by buildbots on
# the tryserver.
MAX_MAC_BUILD_TIME = 14400
MAX_WIN_BUILD_TIME = 14400
MAX_LINUX_BUILD_TIME = 14400

# Patch template to add a new file, DEPS.sha under src folder.
# This file contains SHA1 value of the DEPS changes made while bisecting
# dependency repositories. This patch send along with DEPS patch to tryserver.
# When a build requested is posted with a patch, bisect builders on tryserver,
# once build is produced, it reads SHA value from this file and appends it
# to build archive filename.
DEPS_SHA_PATCH = """diff --git src/DEPS.sha src/DEPS.sha
new file mode 100644
--- /dev/null
+++ src/DEPS.sha
@@ -0,0 +1 @@
+%(deps_sha)s
"""

# The possible values of the --bisect_mode flag, which determines what to
# use when classifying a revision as "good" or "bad".
BISECT_MODE_MEAN = 'mean'
BISECT_MODE_STD_DEV = 'std_dev'
BISECT_MODE_RETURN_CODE = 'return_code'

# The perf dashboard specifically looks for the string
# "Estimated Confidence: 95%" to decide whether or not to cc the author(s).
# If you change this, please update the perf dashboard as well.
RESULTS_BANNER = """
===== BISECT JOB RESULTS =====
Status: %(status)s

Test Command: %(command)s
Test Metric: %(metrics)s
Relative Change: %(change)s
Estimated Confidence: %(confidence)d%%"""

# The perf dashboard specifically looks for the string
# "Author  : " to parse out who to cc on a bug. If you change the
# formatting here, please update the perf dashboard as well.
RESULTS_REVISION_INFO = """
===== SUSPECTED CL(s) =====
Subject : %(subject)s
Author  : %(author)s%(email_info)s%(commit_info)s
Commit  : %(cl)s
Date    : %(cl_date)s"""

REPRO_STEPS_LOCAL = """
==== INSTRUCTIONS TO REPRODUCE ====
To run locally:
$%(command)s"""

REPRO_STEPS_TRYJOB = """
To reproduce on Performance trybot:
1. Create new git branch or check out existing branch.
2. Edit tools/run-perf-test.cfg (instructions in file) or \
third_party/WebKit/Tools/run-perf-test.cfg.
  a) Take care to strip any src/ directories from the head of \
relative path names.
  b) On desktop, only --browser=release is supported, on android \
--browser=android-chromium-testshell.
  c) Test command to use: %(command)s
3. Upload your patch. --bypass-hooks is necessary to upload the changes you \
committed locally to run-perf-test.cfg.
   Note: *DO NOT* commit run-perf-test.cfg changes to the project repository.
   $ git cl upload --bypass-hooks
4. Send your try job to the tryserver. \
[Please make sure to use appropriate bot to reproduce]
   $ git cl try -m tryserver.chromium.perf -b <bot>

For more details please visit
https://sites.google.com/a/chromium.org/dev/developers/performance-try-bots"""

RESULTS_THANKYOU = """
===== THANK YOU FOR CHOOSING BISECT AIRLINES =====
Visit http://www.chromium.org/developers/core-principles for Chrome's policy
on perf regressions.
Contact chrome-perf-dashboard-team with any questions or suggestions about
bisecting.
.                   .------.
.     .---.         \       \==)
.     |PERF\         \       \\
.     |     ---------'-------'-----------.
.     . 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 `-.
.     \______________.-------._______________)
.                   /       /
.                  /       /
.                 /       /==)
.                ._______."""


def _AddAdditionalDepotInfo(depot_info):
  """Adds additional depot info to the global depot variables."""
  global DEPOT_DEPS_NAME
  global DEPOT_NAMES
  DEPOT_DEPS_NAME = dict(DEPOT_DEPS_NAME.items() + depot_info.items())
  DEPOT_NAMES = DEPOT_DEPS_NAME.keys()


def ConfidenceScore(good_results_lists, bad_results_lists):
  """Calculates a confidence score.

  This score is a percentage which represents our degree of confidence in the
  proposition that the good results and bad results are distinct groups, and
  their differences aren't due to chance alone.


  Args:
    good_results_lists: A list of lists of "good" result numbers.
    bad_results_lists: A list of lists of "bad" result numbers.

  Returns:
    A number in the range [0, 100].
  """
  if not good_results_lists or not bad_results_lists:
    return 0.0

  # Flatten the lists of results lists.
  sample1 = sum(good_results_lists, [])
  sample2 = sum(bad_results_lists, [])
  if not sample1 or not sample2:
    return 0.0

  # The p-value is approximately the probability of obtaining the given set
  # of good and bad values just by chance.
  _, _, p_value = ttest.WelchsTTest(sample1, sample2)
  return 100.0 * (1.0 - p_value)


def GetSHA1HexDigest(contents):
  """Returns SHA1 hex digest of the given string."""
  return hashlib.sha1(contents).hexdigest()


def GetZipFileName(build_revision=None, target_arch='ia32', patch_sha=None):
  """Gets the archive file name for the given revision."""
  def PlatformName():
    """Return a string to be used in paths for the platform."""
    if bisect_utils.IsWindowsHost():
      # Build archive for x64 is still stored with 'win32'suffix
      # (chromium_utils.PlatformName()).
      if bisect_utils.Is64BitWindows() and target_arch == 'x64':
        return 'win32'
      return 'win32'
    if bisect_utils.IsLinuxHost():
      # Android builds too are archived with full-build-linux* prefix.
      return 'linux'
    if bisect_utils.IsMacHost():
      return 'mac'
    raise NotImplementedError('Unknown platform "%s".' % sys.platform)

  base_name = 'full-build-%s' % PlatformName()
  if not build_revision:
    return base_name
  if patch_sha:
    build_revision = '%s_%s' % (build_revision , patch_sha)
  return '%s_%s.zip' % (base_name, build_revision)


def GetRemoteBuildPath(build_revision, target_platform='chromium',
                       target_arch='ia32', patch_sha=None):
  """Compute the url to download the build from."""
  def GetGSRootFolderName(target_platform):
    """Gets Google Cloud Storage root folder names"""
    if bisect_utils.IsWindowsHost():
      if bisect_utils.Is64BitWindows() and target_arch == 'x64':
        return 'Win x64 Builder'
      return 'Win Builder'
    if bisect_utils.IsLinuxHost():
      if target_platform == 'android':
        return 'android_perf_rel'
      return 'Linux Builder'
    if bisect_utils.IsMacHost():
      return 'Mac Builder'
    raise NotImplementedError('Unsupported Platform "%s".' % sys.platform)

  base_filename = GetZipFileName(
      build_revision, target_arch, patch_sha)
  builder_folder = GetGSRootFolderName(target_platform)
  return '%s/%s' % (builder_folder, base_filename)


def FetchFromCloudStorage(bucket_name, source_path, destination_path):
  """Fetches file(s) from the Google Cloud Storage.

  Args:
    bucket_name: Google Storage bucket name.
    source_path: Source file path.
    destination_path: Destination file path.

  Returns:
    Downloaded file path if exisits, otherwise None.
  """
  target_file = os.path.join(destination_path, os.path.basename(source_path))
  try:
    if cloud_storage.Exists(bucket_name, source_path):
      print 'Fetching file from gs//%s/%s ...' % (bucket_name, source_path)
      cloud_storage.Get(bucket_name, source_path, destination_path)
      if os.path.exists(target_file):
        return target_file
    else:
      print ('File gs://%s/%s not found in cloud storage.' % (
          bucket_name, source_path))
  except Exception as e:
    print 'Something went wrong while fetching file from cloud: %s' % e
    if os.path.exists(target_file):
      os.remove(target_file)
  return None


# This is copied from Chromium's project build/scripts/common/chromium_utils.py.
def MaybeMakeDirectory(*path):
  """Creates an entire path, if it doesn't already exist."""
  file_path = os.path.join(*path)
  try:
    os.makedirs(file_path)
  except OSError as e:
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
  # The Mac Version of unzip unfortunately does not support Zip64, whereas
  # the python module does, so we have to fallback to the python zip module
  # on Mac if the filesize is greater than 4GB.
  #
  # On Windows, try to use 7z if it is installed, otherwise fall back to python
  # zip module and pray we don't have files larger than 512MB to unzip.
  unzip_cmd = None
  if ((bisect_utils.IsMacHost()
       and os.path.getsize(filename) < 4 * 1024 * 1024 * 1024)
      or bisect_utils.IsLinuxHost()):
    unzip_cmd = ['unzip', '-o']
  elif (bisect_utils.IsWindowsHost()
        and os.path.exists('C:\\Program Files\\7-Zip\\7z.exe')):
    unzip_cmd = ['C:\\Program Files\\7-Zip\\7z.exe', 'x', '-y']

  if unzip_cmd:
    # Make sure path is absolute before changing directories.
    filepath = os.path.abspath(filename)
    saved_dir = os.getcwd()
    os.chdir(output_dir)
    command = unzip_cmd + [filepath]
    result = bisect_utils.RunProcess(command)
    os.chdir(saved_dir)
    if result:
      raise IOError('unzip failed: %s => %s' % (str(command), result))
  else:
    assert bisect_utils.IsWindowsHost() or bisect_utils.IsMacHost()
    zf = zipfile.ZipFile(filename)
    for name in zf.namelist():
      if verbose:
        print 'Extracting %s' % name
      zf.extract(name, output_dir)
      if bisect_utils.IsMacHost():
        # Restore permission bits.
        os.chmod(os.path.join(output_dir, name),
                 zf.getinfo(name).external_attr >> 16L)


def SetBuildSystemDefault(build_system, use_goma, goma_dir):
  """Sets up any environment variables needed to build with the specified build
  system.

  Args:
    build_system: A string specifying build system. Currently only 'ninja' or
        'make' are supported.
  """
  if build_system == 'ninja':
    gyp_var = os.getenv('GYP_GENERATORS', default='')

    if not gyp_var or not 'ninja' in gyp_var:
      if gyp_var:
        os.environ['GYP_GENERATORS'] = gyp_var + ',ninja'
      else:
        os.environ['GYP_GENERATORS'] = 'ninja'

      if bisect_utils.IsWindowsHost():
        os.environ['GYP_DEFINES'] = ('component=shared_library '
                                     'incremental_chrome_dll=1 '
                                     'disable_nacl=1 fastbuild=1 '
                                     'chromium_win_pch=0')

  elif build_system == 'make':
    os.environ['GYP_GENERATORS'] = 'make'
  else:
    raise RuntimeError('%s build not supported.' % build_system)

  if use_goma:
    os.environ['GYP_DEFINES'] = '%s %s' % (os.getenv('GYP_DEFINES', default=''),
                                           'use_goma=1')
    if goma_dir:
      os.environ['GYP_DEFINES'] += ' gomadir=%s' % goma_dir


def BuildWithMake(threads, targets, build_type='Release'):
  cmd = ['make', 'BUILDTYPE=%s' % build_type]

  if threads:
    cmd.append('-j%d' % threads)

  cmd += targets

  return_code = bisect_utils.RunProcess(cmd)

  return not return_code


def BuildWithNinja(threads, targets, build_type='Release'):
  cmd = ['ninja', '-C', os.path.join('out', build_type)]

  if threads:
    cmd.append('-j%d' % threads)

  cmd += targets

  return_code = bisect_utils.RunProcess(cmd)

  return not return_code


def BuildWithVisualStudio(targets, build_type='Release'):
  path_to_devenv = os.path.abspath(
      os.path.join(os.environ['VS100COMNTOOLS'], '..', 'IDE', 'devenv.com'))
  path_to_sln = os.path.join(os.getcwd(), 'chrome', 'chrome.sln')
  cmd = [path_to_devenv, '/build', build_type, path_to_sln]

  for t in targets:
    cmd.extend(['/Project', t])

  return_code = bisect_utils.RunProcess(cmd)

  return not return_code


def WriteStringToFile(text, file_name):
  try:
    with open(file_name, 'wb') as f:
      f.write(text)
  except IOError:
    raise RuntimeError('Error writing to file [%s]' % file_name )


def ReadStringFromFile(file_name):
  try:
    with open(file_name) as f:
      return f.read()
  except IOError:
    raise RuntimeError('Error reading file [%s]' % file_name )


def ChangeBackslashToSlashInPatch(diff_text):
  """Formats file paths in the given text to unix-style paths."""
  if diff_text:
    diff_lines = diff_text.split('\n')
    for i in range(len(diff_lines)):
      if (diff_lines[i].startswith('--- ') or
          diff_lines[i].startswith('+++ ')):
        diff_lines[i] = diff_lines[i].replace('\\', '/')
    return '\n'.join(diff_lines)
  return None


class Builder(object):
  """Builder is used by the bisect script to build relevant targets and deploy.
  """
  def __init__(self, opts):
    """Performs setup for building with target build system.

    Args:
        opts: Options parsed from command line.
    """
    if bisect_utils.IsWindowsHost():
      if not opts.build_preference:
        opts.build_preference = 'msvs'

      if opts.build_preference == 'msvs':
        if not os.getenv('VS100COMNTOOLS'):
          raise RuntimeError(
              'Path to visual studio could not be determined.')
      else:
        SetBuildSystemDefault(opts.build_preference, opts.use_goma,
                              opts.goma_dir)
    else:
      if not opts.build_preference:
        if 'ninja' in os.getenv('GYP_GENERATORS', default=''):
          opts.build_preference = 'ninja'
        else:
          opts.build_preference = 'make'

      SetBuildSystemDefault(opts.build_preference, opts.use_goma, opts.goma_dir)

    if not bisect_utils.SetupPlatformBuildEnvironment(opts):
      raise RuntimeError('Failed to set platform environment.')

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
    """Returns the path to the build directory, relative to the checkout root.

      Assumes that the current working directory is the checkout root.
    """
    src_dir = src_dir or 'src'
    if opts.build_preference == 'ninja' or bisect_utils.IsLinuxHost():
      return os.path.join(src_dir, 'out')
    if bisect_utils.IsMacHost():
      return os.path.join(src_dir, 'xcodebuild')
    if bisect_utils.IsWindowsHost():
      return os.path.join(src_dir, 'build')
    raise NotImplementedError('Unexpected platform %s' % sys.platform)


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
      build_success = BuildWithMake(threads, targets, opts.target_build_type)
    elif opts.build_preference == 'ninja':
      build_success = BuildWithNinja(threads, targets, opts.target_build_type)
    elif opts.build_preference == 'msvs':
      assert bisect_utils.IsWindowsHost(), 'msvs is only supported on Windows.'
      build_success = BuildWithVisualStudio(targets, opts.target_build_type)
    else:
      assert False, 'No build system defined.'
    return build_success


class AndroidBuilder(Builder):
  """AndroidBuilder is used to build on android."""
  def __init__(self, opts):
    super(AndroidBuilder, self).__init__(opts)

  def _GetTargets(self):
    return ['chrome_shell_apk', 'cc_perftests_apk', 'android_tools']

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
      build_success = BuildWithNinja(
          threads, self._GetTargets(), opts.target_build_type)
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

      return_code = bisect_utils.RunProcess(cmd)
      return not return_code
    except OSError:
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

    cmd += ['BUILDTYPE=%s' % opts.target_build_type, './build_packages',
        '--board=%s' % opts.cros_board]
    return_code = bisect_utils.RunProcess(cmd)

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

    cmd += ['BUILDTYPE=%s' % opts.target_build_type, '--', './build_image',
        '--board=%s' % opts.cros_board, 'test']

    return_code = bisect_utils.RunProcess(cmd)

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


def _ParseRevisionsFromDEPSFileManually(deps_file_contents):
  """Parses the vars section of the DEPS file with regex.

  Args:
    deps_file_contents: The DEPS file contents as a string.

  Returns:
    A dict in the format {depot:revision} if successful, otherwise None.
  """
  # We'll parse the "vars" section of the DEPS file.
  rxp = re.compile('vars = {(?P<vars_body>[^}]+)', re.MULTILINE)
  re_results = rxp.search(deps_file_contents)

  if not re_results:
    return None

  # We should be left with a series of entries in the vars component of
  # the DEPS file with the following format:
  # 'depot_name': 'revision',
  vars_body = re_results.group('vars_body')
  rxp = re.compile("'(?P<depot_body>[\w_-]+)':[\s]+'(?P<rev_body>[\w@]+)'",
                   re.MULTILINE)
  re_results = rxp.findall(vars_body)

  return dict(re_results)


def _WaitUntilBuildIsReady(
    fetch_build, bot_name, builder_host, builder_port, build_request_id,
    max_timeout):
  """Waits until build is produced by bisect builder on tryserver.

  Args:
    fetch_build: Function to check and download build from cloud storage.
    bot_name: Builder bot name on tryserver.
    builder_host Tryserver hostname.
    builder_port: Tryserver port.
    build_request_id: A unique ID of the build request posted to tryserver.
    max_timeout: Maximum time to wait for the build.

  Returns:
     Downloaded archive file path if exists, otherwise None.
  """
  # Build number on the tryserver.
  build_num = None
  # Interval to check build on cloud storage.
  poll_interval = 60
  # Interval to check build status on tryserver.
  status_check_interval = 600
  last_status_check = time.time()
  start_time = time.time()
  while True:
    # Checks for build on gs://chrome-perf and download if exists.
    res = fetch_build()
    if res:
      return (res, 'Build successfully found')
    elapsed_status_check = time.time() - last_status_check
    # To avoid overloading tryserver with status check requests, we check
    # build status for every 10 mins.
    if elapsed_status_check > status_check_interval:
      last_status_check = time.time()
      if not build_num:
        # Get the build number on tryserver for the current build.
        build_num = bisect_builder.GetBuildNumFromBuilder(
            build_request_id, bot_name, builder_host, builder_port)
      # Check the status of build using the build number.
      # Note: Build is treated as PENDING if build number is not found
      # on the the tryserver.
      build_status, status_link = bisect_builder.GetBuildStatus(
          build_num, bot_name, builder_host, builder_port)
      if build_status == bisect_builder.FAILED:
        return (None, 'Failed to produce build, log: %s' % status_link)
    elapsed_time = time.time() - start_time
    if elapsed_time > max_timeout:
      return (None, 'Timed out: %ss without build' % max_timeout)

    print 'Time elapsed: %ss without build.' % elapsed_time
    time.sleep(poll_interval)
    # For some reason, mac bisect bots were not flushing stdout periodically.
    # As a result buildbot command is timed-out. Flush stdout on all platforms
    # while waiting for build.
    sys.stdout.flush()


def _UpdateV8Branch(deps_content):
  """Updates V8 branch in DEPS file to process v8_bleeding_edge.

  Check for "v8_branch" in DEPS file if exists update its value
  with v8_bleeding_edge branch. Note: "v8_branch" is added to DEPS
  variable from DEPS revision 254916, therefore check for "src/v8":
  <v8 source path> in DEPS in order to support prior DEPS revisions
  and update it.

  Args:
    deps_content: DEPS file contents to be modified.

  Returns:
    Modified DEPS file contents as a string.
  """
  new_branch = r'branches/bleeding_edge'
  v8_branch_pattern = re.compile(r'(?<="v8_branch": ")(.*)(?=")')
  if re.search(v8_branch_pattern, deps_content):
    deps_content = re.sub(v8_branch_pattern, new_branch, deps_content)
  else:
    # Replaces the branch assigned to "src/v8" key in DEPS file.
    # Format of "src/v8" in DEPS:
    # "src/v8":
    #    (Var("googlecode_url") % "v8") + "/trunk@" + Var("v8_revision"),
    # So, "/trunk@" is replace with "/branches/bleeding_edge@"
    v8_src_pattern = re.compile(
        r'(?<="v8"\) \+ "/)(.*)(?=@" \+ Var\("v8_revision"\))', re.MULTILINE)
    if re.search(v8_src_pattern, deps_content):
      deps_content = re.sub(v8_src_pattern, new_branch, deps_content)
  return deps_content


def _UpdateDEPSForAngle(revision, depot, deps_file):
  """Updates DEPS file with new revision for Angle repository.

  This is a hack for Angle depot case because, in DEPS file "vars" dictionary
  variable contains "angle_revision" key that holds git hash instead of
  SVN revision.

  And sometimes "angle_revision" key is not specified in "vars" variable,
  in such cases check "deps" dictionary variable that matches
  angle.git@[a-fA-F0-9]{40}$ and replace git hash.
  """
  deps_var = DEPOT_DEPS_NAME[depot]['deps_var']
  try:
    deps_contents = ReadStringFromFile(deps_file)
    # Check whether the depot and revision pattern in DEPS file vars variable
    # e.g. "angle_revision": "fa63e947cb3eccf463648d21a05d5002c9b8adfa".
    angle_rev_pattern = re.compile(r'(?<="%s": ")([a-fA-F0-9]{40})(?=")' %
                                   deps_var, re.MULTILINE)
    match = re.search(angle_rev_pattern % deps_var, deps_contents)
    if match:
      # Update the revision information for the given depot
      new_data = re.sub(angle_rev_pattern, revision, deps_contents)
    else:
      # Check whether the depot and revision pattern in DEPS file deps
      # variable. e.g.,
      # "src/third_party/angle": Var("chromium_git") +
      # "/angle/angle.git@fa63e947cb3eccf463648d21a05d5002c9b8adfa",.
      angle_rev_pattern = re.compile(
          r'(?<=angle\.git@)([a-fA-F0-9]{40})(?=")', re.MULTILINE)
      match = re.search(angle_rev_pattern, deps_contents)
      if not match:
        print 'Could not find angle revision information in DEPS file.'
        return False
      new_data = re.sub(angle_rev_pattern, revision, deps_contents)
    # Write changes to DEPS file
    WriteStringToFile(new_data, deps_file)
    return True
  except IOError, e:
    print 'Something went wrong while updating DEPS file, %s' % e
  return False


def _TryParseHistogramValuesFromOutput(metric, text):
  """Attempts to parse a metric in the format HISTOGRAM <graph: <trace>.

  Args:
    metric: The metric as a list of [<trace>, <value>] strings.
    text: The text to parse the metric values from.

  Returns:
    A list of floating point numbers found, [] if none were found.
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
      except Exception:
        pass

  return values_list


def _TryParseResultValuesFromOutput(metric, text):
  """Attempts to parse a metric in the format RESULT <graph>: <trace>= ...

  Args:
    metric: The metric as a list of [<trace>, <value>] string pairs.
    text: The text to parse the metric values from.

  Returns:
    A list of floating point numbers found.
  """
  # Format is: RESULT <graph>: <trace>= <value> <units>
  metric_re = re.escape('RESULT %s: %s=' % (metric[0], metric[1]))

  # The log will be parsed looking for format:
  # <*>RESULT <graph_name>: <trace_name>= <value>
  single_result_re = re.compile(
      metric_re + '\s*(?P<VALUE>[-]?\d*(\.\d*)?)')

  # The log will be parsed looking for format:
  # <*>RESULT <graph_name>: <trace_name>= [<value>,value,value,...]
  multi_results_re = re.compile(
      metric_re + '\s*\[\s*(?P<VALUES>[-]?[\d\., ]+)\s*\]')

  # The log will be parsed looking for format:
  # <*>RESULT <graph_name>: <trace_name>= {<mean>, <std deviation>}
  mean_stddev_re = re.compile(
      metric_re +
      '\s*\{\s*(?P<MEAN>[-]?\d*(\.\d*)?),\s*(?P<STDDEV>\d+(\.\d*)?)\s*\}')

  text_lines = text.split('\n')
  values_list = []
  for current_line in text_lines:
    # Parse the output from the performance test for the metric we're
    # interested in.
    single_result_match = single_result_re.search(current_line)
    multi_results_match = multi_results_re.search(current_line)
    mean_stddev_match = mean_stddev_re.search(current_line)
    if (not single_result_match is None and
        single_result_match.group('VALUE')):
      values_list += [single_result_match.group('VALUE')]
    elif (not multi_results_match is None and
          multi_results_match.group('VALUES')):
      metric_values = multi_results_match.group('VALUES')
      values_list += metric_values.split(',')
    elif (not mean_stddev_match is None and
          mean_stddev_match.group('MEAN')):
      values_list += [mean_stddev_match.group('MEAN')]

  values_list = [float(v) for v in values_list
                 if bisect_utils.IsStringFloat(v)]

  # If the metric is times/t, we need to sum the timings in order to get
  # similar regression results as the try-bots.
  metrics_to_sum = [
      ['times', 't'],
      ['times', 'page_load_time'],
      ['cold_times', 'page_load_time'],
      ['warm_times', 'page_load_time'],
  ]

  if metric in metrics_to_sum:
    if values_list:
      values_list = [reduce(lambda x, y: float(x) + float(y), values_list)]

  return values_list


def _ParseMetricValuesFromOutput(metric, text):
  """Parses output from performance_ui_tests and retrieves the results for
  a given metric.

  Args:
    metric: The metric as a list of [<trace>, <value>] strings.
    text: The text to parse the metric values from.

  Returns:
    A list of floating point numbers found.
  """
  metric_values = _TryParseResultValuesFromOutput(metric, text)

  if not metric_values:
    metric_values = _TryParseHistogramValuesFromOutput(metric, text)

  return metric_values


def _GenerateProfileIfNecessary(command_args):
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
      return not bisect_utils.RunProcess(['python', path_to_generate,
          '--profile-type-to-generate', profile_type,
          '--browser', arg_dict['--browser'], '--output-dir', profile_path])
    return False
  return True


def _AddRevisionsIntoRevisionData(revisions, depot, sort, revision_data):
  """Adds new revisions to the revision_data dict and initializes them.

  Args:
    revisions: List of revisions to add.
    depot: Depot that's currently in use (src, webkit, etc...)
    sort: Sorting key for displaying revisions.
    revision_data: A dict to add the new revisions into. Existing revisions
      will have their sort keys offset.
  """
  num_depot_revisions = len(revisions)

  for _, v in revision_data.iteritems():
    if v['sort'] > sort:
      v['sort'] += num_depot_revisions

  for i in xrange(num_depot_revisions):
    r = revisions[i]
    revision_data[r] = {
        'revision' : r,
        'depot' : depot,
        'value' : None,
        'perf_time' : 0,
        'build_time' : 0,
        'passed' : '?',
        'sort' : i + sort + 1,
    }


def _PrintThankYou():
  print RESULTS_THANKYOU


def _PrintTableRow(column_widths, row_data):
  """Prints out a row in a formatted table that has columns aligned.

  Args:
    column_widths: A list of column width numbers.
    row_data: A list of items for each column in this row.
  """
  assert len(column_widths) == len(row_data)
  text = ''
  for i in xrange(len(column_widths)):
    current_row_data = row_data[i].center(column_widths[i], ' ')
    text += ('%%%ds' % column_widths[i]) % current_row_data
  print text


def _PrintStepTime(revision_data_sorted):
  """Prints information about how long various steps took.

  Args:
    revision_data_sorted: The sorted list of revision data dictionaries."""
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

def _FindOtherRegressions(revision_data_sorted, bad_greater_than_good):
  """Compiles a list of other possible regressions from the revision data.

  Args:
    revision_data_sorted: Sorted list of (revision, revision data dict) pairs.
    bad_greater_than_good: Whether the result value at the "bad" revision is
        numerically greater than the result value at the "good" revision.

  Returns:
    A list of [current_rev, previous_rev, confidence] for other places where
    there may have been a regression.
  """
  other_regressions = []
  previous_values = []
  previous_id = None
  for current_id, current_data in revision_data_sorted:
    current_values = current_data['value']
    if current_values:
      current_values = current_values['values']
      if previous_values:
        confidence = ConfidenceScore(previous_values, [current_values])
        mean_of_prev_runs = math_utils.Mean(sum(previous_values, []))
        mean_of_current_runs = math_utils.Mean(current_values)

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

class BisectPerformanceMetrics(object):
  """This class contains functionality to perform a bisection of a range of
  revisions to narrow down where performance regressions may have occurred.

  The main entry-point is the Run method.
  """

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
      output, return_code = bisect_utils.RunProcessAndRetrieveOutput(cmd)

      assert not return_code, ('An error occurred while running '
                               '"%s"' % ' '.join(cmd))

      os.chdir(cwd)

      revision_work_list = list(set(
          [int(o) for o in output.split('\n') if bisect_utils.IsStringInt(o)]))
      revision_work_list = sorted(revision_work_list, reverse=True)
    else:
      cwd = self._GetDepotDirectory(depot)
      revision_work_list = self.source_control.GetRevisionList(bad_revision,
          good_revision, cwd=cwd)

    return revision_work_list

  def _GetV8BleedingEdgeFromV8TrunkIfMappable(self, revision):
    svn_revision = self.source_control.SVNFindRev(revision)

    if bisect_utils.IsStringInt(svn_revision):
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
                bleeding_edge_revision, 'v8_bleeding_edge', DEPOT_DEPS_NAME, 1,
                cwd=v8_bleeding_edge_dir)
            return git_revision
          except (IndexError, ValueError):
            pass

        if not git_revision:
          # Wasn't successful, try the old way of looking for "Prepare push to"
          git_revision = self.source_control.ResolveToRevision(
              int(svn_revision) - 1, 'v8_bleeding_edge', DEPOT_DEPS_NAME, -1,
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
    output = bisect_utils.CheckRunGit(cmd, cwd=cwd)
    commit_time = int(output)
    commits = []

    if search_forward:
      cmd = ['log', '--format=%H', '-10', '--after=%d' % commit_time,
          'origin/master']
      output = bisect_utils.CheckRunGit(cmd, cwd=cwd)
      output = output.split()
      commits = output
      commits = reversed(commits)
    else:
      cmd = ['log', '--format=%H', '-10', '--before=%d' % commit_time,
          'origin/master']
      output = bisect_utils.CheckRunGit(cmd, cwd=cwd)
      output = output.split()
      commits = output

    bleeding_edge_revision = None

    for c in commits:
      bleeding_edge_revision = self._GetV8BleedingEdgeFromV8TrunkIfMappable(c)
      if bleeding_edge_revision:
        break

    return bleeding_edge_revision

  def _ParseRevisionsFromDEPSFile(self, depot):
    """Parses the local DEPS file to determine blink/skia/v8 revisions which may
    be needed if the bisect recurses into those depots later.

    Args:
      depot: Name of depot being bisected.

    Returns:
      A dict in the format {depot:revision} if successful, otherwise None.
    """
    try:
      deps_data = {
          'Var': lambda _: deps_data["vars"][_],
          'From': lambda *args: None,
      }
      execfile(bisect_utils.FILE_DEPS_GIT, {}, deps_data)
      deps_data = deps_data['deps']

      rxp = re.compile(".git@(?P<revision>[a-fA-F0-9]+)")
      results = {}
      for depot_name, depot_data in DEPOT_DEPS_NAME.iteritems():
        if (depot_data.get('platform') and
            depot_data.get('platform') != os.name):
          continue

        if (depot_data.get('recurse') and depot in depot_data.get('from')):
          depot_data_src = depot_data.get('src') or depot_data.get('src_old')
          src_dir = deps_data.get(depot_data_src)
          if src_dir:
            self.depot_cwd[depot_name] = os.path.join(self.src_cwd,
                                                      depot_data_src[4:])
            re_results = rxp.search(src_dir)
            if re_results:
              results[depot_name] = re_results.group('revision')
            else:
              warning_text = ('Couldn\'t parse revision for %s while bisecting '
                              '%s' % (depot_name, depot))
              if not warning_text in self.warnings:
                self.warnings.append(warning_text)
          else:
            results[depot_name] = None
      return results
    except ImportError:
      deps_file_contents = ReadStringFromFile(bisect_utils.FILE_DEPS_GIT)
      parse_results = _ParseRevisionsFromDEPSFileManually(deps_file_contents)
      results = {}
      for depot_name, depot_revision in parse_results.iteritems():
        depot_revision = depot_revision.strip('@')
        print depot_name, depot_revision
        for current_name, current_data in DEPOT_DEPS_NAME.iteritems():
          if (current_data.has_key('deps_var') and
              current_data['deps_var'] == depot_name):
            src_name = current_name
            results[src_name] = depot_revision
            break
      return results

  def _Get3rdPartyRevisions(self, depot):
    """Parses the DEPS file to determine WebKit/v8/etc... versions.

    Returns:
      A dict in the format {depot:revision} if successful, otherwise None.
    """
    cwd = os.getcwd()
    self.ChangeToDepotWorkingDirectory(depot)

    results = {}

    if depot == 'chromium' or depot == 'android-chrome':
      results = self._ParseRevisionsFromDEPSFile(depot)
      os.chdir(cwd)
    elif depot == 'cros':
      cmd = [CROS_SDK_PATH, '--', 'portageq-%s' % self.opts.cros_board,
             'best_visible', '/build/%s' % self.opts.cros_board, 'ebuild',
             CROS_CHROMEOS_PATTERN]
      output, return_code = bisect_utils.RunProcessAndRetrieveOutput(cmd)

      assert not return_code, ('An error occurred while running '
                               '"%s"' % ' '.join(cmd))

      if len(output) > CROS_CHROMEOS_PATTERN:
        output = output[len(CROS_CHROMEOS_PATTERN):]

      if len(output) > 1:
        output = output.split('_')[0]

        if len(output) > 3:
          contents = output.split('.')

          version = contents[2]

          if contents[3] != '0':
            warningText = ('Chrome version: %s.%s but using %s.0 to bisect.' %
                           (version, contents[3], version))
            if not warningText in self.warnings:
              self.warnings.append(warningText)

          cwd = os.getcwd()
          self.ChangeToDepotWorkingDirectory('chromium')
          cmd = ['log', '-1', '--format=%H',
                 '--author=chrome-release@google.com',
                 '--grep=to %s' % version, 'origin/master']
          return_code = bisect_utils.CheckRunGit(cmd)
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

  def GetBuildArchiveForRevision(self, revision, gs_bucket, target_arch,
                                 patch_sha, out_dir):
    """Checks and downloads build archive for a given revision.

    Checks for build archive with Git hash or SVN revision. If either of the
    file exists, then downloads the archive file.

    Args:
      revision: A Git hash revision.
      gs_bucket: Cloud storage bucket name
      target_arch: 32 or 64 bit build target
      patch: A DEPS patch (used while bisecting 3rd party repositories).
      out_dir: Build output directory where downloaded file is stored.

    Returns:
      Downloaded archive file path if exists, otherwise None.
    """
    # Source archive file path on cloud storage using Git revision.
    source_file = GetRemoteBuildPath(
        revision, self.opts.target_platform, target_arch, patch_sha)
    downloaded_archive = FetchFromCloudStorage(gs_bucket, source_file, out_dir)
    if not downloaded_archive:
      # Get SVN revision for the given SHA.
      svn_revision = self.source_control.SVNFindRev(revision)
      if svn_revision:
        # Source archive file path on cloud storage using SVN revision.
        source_file = GetRemoteBuildPath(
            svn_revision, self.opts.target_platform, target_arch, patch_sha)
        return FetchFromCloudStorage(gs_bucket, source_file, out_dir)
    return downloaded_archive

  def DownloadCurrentBuild(self, revision, build_type='Release', patch=None):
    """Downloads the build archive for the given revision.

    Args:
      revision: The Git revision to download or build.
      build_type: Target build type ('Release', 'Debug', 'Release_x64' etc.)
      patch: A DEPS patch (used while bisecting 3rd party repositories).

    Returns:
      True if download succeeds, otherwise False.
    """
    patch_sha = None
    if patch:
      # Get the SHA of the DEPS changes patch.
      patch_sha = GetSHA1HexDigest(patch)

      # Update the DEPS changes patch with a patch to create a new file named
      # 'DEPS.sha' and add patch_sha evaluated above to it.
      patch = '%s\n%s' % (patch, DEPS_SHA_PATCH % {'deps_sha': patch_sha})

    # Get Build output directory
    abs_build_dir = os.path.abspath(
        self.builder.GetBuildOutputDirectory(self.opts, self.src_cwd))

    fetch_build_func = lambda: self.GetBuildArchiveForRevision(
      revision, self.opts.gs_bucket, self.opts.target_arch,
      patch_sha, abs_build_dir)

    # Downloaded archive file path, downloads build archive for given revision.
    downloaded_file = fetch_build_func()

    # When build archive doesn't exists, post a build request to tryserver
    # and wait for the build to be produced.
    if not downloaded_file:
      downloaded_file = self.PostBuildRequestAndWait(
          revision, fetch_build=fetch_build_func, patch=patch)
      if not downloaded_file:
        return False

    # Generic name for the archive, created when archive file is extracted.
    output_dir = os.path.join(
        abs_build_dir, GetZipFileName(target_arch=self.opts.target_arch))
    # Unzip build archive directory.
    try:
      RmTreeAndMkDir(output_dir, skip_makedir=True)
      self.BackupOrRestoreOutputdirectory(restore=False)
      # Build output directory based on target(e.g. out/Release, out/Debug).
      target_build_output_dir = os.path.join(abs_build_dir, build_type)
      ExtractZip(downloaded_file, abs_build_dir)
      if not os.path.exists(output_dir):
        # Due to recipe changes, the builds extract folder contains
        # out/Release instead of full-build-<platform>/Release.
        if os.path.exists(os.path.join(abs_build_dir, 'out', build_type)):
          output_dir = os.path.join(abs_build_dir, 'out', build_type)
        else:
          raise IOError('Missing extracted folder %s ' % output_dir)

      print 'Moving build from %s to %s' % (
          output_dir, target_build_output_dir)
      shutil.move(output_dir, target_build_output_dir)
      return True
    except Exception as e:
      print 'Something went wrong while extracting archive file: %s' % e
      self.BackupOrRestoreOutputdirectory(restore=True)
      # Cleanup any leftovers from unzipping.
      if os.path.exists(output_dir):
        RmTreeAndMkDir(output_dir, skip_makedir=True)
    finally:
      # Delete downloaded archive
      if os.path.exists(downloaded_file):
        os.remove(downloaded_file)
    return False

  def PostBuildRequestAndWait(self, revision, fetch_build, patch=None):
    """POSTs the build request job to the tryserver instance.

    A try job build request is posted to tryserver.chromium.perf master,
    and waits for the binaries to be produced and archived on cloud storage.
    Once the build is ready and stored onto cloud, build archive is downloaded
    into the output folder.

    Args:
      revision: A Git hash revision.
      fetch_build: Function to check and download build from cloud storage.
      patch: A DEPS patch (used while bisecting 3rd party repositories).

    Returns:
      Downloaded archive file path when requested build exists and download is
      successful, otherwise None.
    """
    # Get SVN revision for the given SHA.
    svn_revision = self.source_control.SVNFindRev(revision)
    if not svn_revision:
      raise RuntimeError(
          'Failed to determine SVN revision for %s' % revision)

    def GetBuilderNameAndBuildTime(target_platform, target_arch='ia32'):
      """Gets builder bot name and build time in seconds based on platform."""
      # Bot names should match the one listed in tryserver.chromium's
      # master.cfg which produces builds for bisect.
      if bisect_utils.IsWindowsHost():
        if bisect_utils.Is64BitWindows() and target_arch == 'x64':
          return ('win_perf_bisect_builder', MAX_WIN_BUILD_TIME)
        return ('win_perf_bisect_builder', MAX_WIN_BUILD_TIME)
      if bisect_utils.IsLinuxHost():
        if target_platform == 'android':
          return ('android_perf_bisect_builder', MAX_LINUX_BUILD_TIME)
        return ('linux_perf_bisect_builder', MAX_LINUX_BUILD_TIME)
      if bisect_utils.IsMacHost():
        return ('mac_perf_bisect_builder', MAX_MAC_BUILD_TIME)
      raise NotImplementedError('Unsupported Platform "%s".' % sys.platform)
    if not fetch_build:
      return False

    bot_name, build_timeout = GetBuilderNameAndBuildTime(
       self.opts.target_platform, self.opts.target_arch)
    builder_host = self.opts.builder_host
    builder_port = self.opts.builder_port
    # Create a unique ID for each build request posted to tryserver builders.
    # This ID is added to "Reason" property in build's json.
    build_request_id = GetSHA1HexDigest(
        '%s-%s-%s' % (svn_revision, patch, time.time()))

    # Creates a try job description.
    job_args = {
        'host': builder_host,
        'port': builder_port,
        'revision': 'src@%s' % svn_revision,
        'bot': bot_name,
        'name': build_request_id,
    }
    # Update patch information if supplied.
    if patch:
      job_args['patch'] = patch
    # Posts job to build the revision on the server.
    if bisect_builder.PostTryJob(job_args):
      target_file, error_msg = _WaitUntilBuildIsReady(
          fetch_build, bot_name, builder_host, builder_port, build_request_id,
          build_timeout)
      if not target_file:
        print '%s [revision: %s]' % (error_msg, svn_revision)
        return None
      return target_file
    print 'Failed to post build request for revision: [%s]' % svn_revision
    return None

  def IsDownloadable(self, depot):
    """Checks if build is downloadable based on target platform and depot."""
    if (self.opts.target_platform in ['chromium', 'android'] and
        self.opts.gs_bucket):
      return (depot == 'chromium' or
              'chromium' in DEPOT_DEPS_NAME[depot]['from'] or
              'v8' in DEPOT_DEPS_NAME[depot]['from'])
    return False

  def UpdateDeps(self, revision, depot, deps_file):
    """Updates DEPS file with new revision of dependency repository.

    This method search DEPS for a particular pattern in which depot revision
    is specified (e.g "webkit_revision": "123456"). If a match is found then
    it resolves the given git hash to SVN revision and replace it in DEPS file.

    Args:
      revision: A git hash revision of the dependency repository.
      depot: Current depot being bisected.
      deps_file: Path to DEPS file.

    Returns:
      True if DEPS file is modified successfully, otherwise False.
    """
    if not os.path.exists(deps_file):
      return False

    deps_var = DEPOT_DEPS_NAME[depot]['deps_var']
    # Don't update DEPS file if deps_var is not set in DEPOT_DEPS_NAME.
    if not deps_var:
      print 'DEPS update not supported for Depot: %s', depot
      return False

    # Hack for Angle repository. In the DEPS file, "vars" dictionary variable
    # contains "angle_revision" key that holds git hash instead of SVN revision.
    # And sometime "angle_revision" key is not specified in "vars" variable.
    # In such cases check, "deps" dictionary variable that matches
    # angle.git@[a-fA-F0-9]{40}$ and replace git hash.
    if depot == 'angle':
      return _UpdateDEPSForAngle(revision, depot, deps_file)

    try:
      deps_contents = ReadStringFromFile(deps_file)
      # Check whether the depot and revision pattern in DEPS file vars
      # e.g. for webkit the format is "webkit_revision": "12345".
      deps_revision = re.compile(r'(?<="%s": ")([0-9]+)(?=")' % deps_var,
                                 re.MULTILINE)
      match = re.search(deps_revision, deps_contents)
      if match:
        svn_revision = self.source_control.SVNFindRev(
            revision, self._GetDepotDirectory(depot))
        if not svn_revision:
          print 'Could not determine SVN revision for %s' % revision
          return False
        # Update the revision information for the given depot
        new_data = re.sub(deps_revision, str(svn_revision), deps_contents)

        # For v8_bleeding_edge revisions change V8 branch in order
        # to fetch bleeding edge revision.
        if depot == 'v8_bleeding_edge':
          new_data = _UpdateV8Branch(new_data)
          if not new_data:
            return False
        # Write changes to DEPS file
        WriteStringToFile(new_data, deps_file)
        return True
    except IOError, e:
      print 'Something went wrong while updating DEPS file. [%s]' % e
    return False

  def CreateDEPSPatch(self, depot, revision):
    """Modifies DEPS and returns diff as text.

    Args:
      depot: Current depot being bisected.
      revision: A git hash revision of the dependency repository.

    Returns:
      A tuple with git hash of chromium revision and DEPS patch text.
    """
    deps_file_path = os.path.join(self.src_cwd, bisect_utils.FILE_DEPS)
    if not os.path.exists(deps_file_path):
      raise RuntimeError('DEPS file does not exists.[%s]' % deps_file_path)
    # Get current chromium revision (git hash).
    cmd = ['rev-parse', 'HEAD']
    chromium_sha = bisect_utils.CheckRunGit(cmd).strip()
    if not chromium_sha:
      raise RuntimeError('Failed to determine Chromium revision for %s' %
                         revision)
    if ('chromium' in DEPOT_DEPS_NAME[depot]['from'] or
        'v8' in DEPOT_DEPS_NAME[depot]['from']):
      # Checkout DEPS file for the current chromium revision.
      if self.source_control.CheckoutFileAtRevision(
          bisect_utils.FILE_DEPS, chromium_sha, cwd=self.src_cwd):
        if self.UpdateDeps(revision, depot, deps_file_path):
          diff_command = [
              'diff',
              '--src-prefix=src/',
              '--dst-prefix=src/',
              '--no-ext-diff',
               bisect_utils.FILE_DEPS,
          ]
          diff_text = bisect_utils.CheckRunGit(diff_command, cwd=self.src_cwd)
          return (chromium_sha, ChangeBackslashToSlashInPatch(diff_text))
        else:
          raise RuntimeError(
              'Failed to update DEPS file for chromium: [%s]' % chromium_sha)
      else:
        raise RuntimeError(
            'DEPS checkout Failed for chromium revision : [%s]' % chromium_sha)
    return (None, None)

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
    if self.IsDownloadable(depot) and revision:
      deps_patch = None
      if depot != 'chromium':
        # Create a DEPS patch with new revision for dependency repository.
        revision, deps_patch = self.CreateDEPSPatch(depot, revision)
      if self.DownloadCurrentBuild(revision, patch=deps_patch):
        os.chdir(cwd)
        if deps_patch:
          # Reverts the changes to DEPS file.
          self.source_control.CheckoutFileAtRevision(
              bisect_utils.FILE_DEPS, revision, cwd=self.src_cwd)
        return True
      return False

    # These codes are executed when bisect bots builds binaries locally.
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

  def _IsBisectModeUsingMetric(self):
    return self.opts.bisect_mode in [BISECT_MODE_MEAN, BISECT_MODE_STD_DEV]

  def _IsBisectModeReturnCode(self):
    return self.opts.bisect_mode in [BISECT_MODE_RETURN_CODE]

  def _IsBisectModeStandardDeviation(self):
    return self.opts.bisect_mode in [BISECT_MODE_STD_DEV]

  def GetCompatibleCommand(self, command_to_run, revision, depot):
    # Prior to crrev.com/274857 *only* android-chromium-testshell
    # Then until crrev.com/276628 *both* (android-chromium-testshell and
    # android-chrome-shell) work. After that rev 276628 *only*
    # android-chrome-shell works. bisect-perf-reggresion.py script should
    # handle these cases and set appropriate browser type based on revision.
    if self.opts.target_platform in ['android']:
      # When its a third_party depot, get the chromium revision.
      if depot != 'chromium':
        revision = bisect_utils.CheckRunGit(
            ['rev-parse', 'HEAD'], cwd=self.src_cwd).strip()
      svn_revision = self.source_control.SVNFindRev(revision, cwd=self.src_cwd)
      if not svn_revision:
        return command_to_run
      cmd_re = re.compile('--browser=(?P<browser_type>\S+)')
      matches = cmd_re.search(command_to_run)
      if bisect_utils.IsStringInt(svn_revision) and matches:
        cmd_browser = matches.group('browser_type')
        if svn_revision <= 274857 and cmd_browser == 'android-chrome-shell':
          return command_to_run.replace(cmd_browser,
                                        'android-chromium-testshell')
        elif (svn_revision >= 276628 and
              cmd_browser == 'android-chromium-testshell'):
          return command_to_run.replace(cmd_browser,
                                        'android-chrome-shell')
    return command_to_run

  def RunPerformanceTestAndParseResults(
      self, command_to_run, metric, reset_on_first_run=False,
      upload_on_last_run=False, results_label=None):
    """Runs a performance test on the current revision and parses the results.

    Args:
      command_to_run: The command to be run to execute the performance test.
      metric: The metric to parse out from the results of the performance test.
          This is the result chart name and trace name, separated by slash.
      reset_on_first_run: If True, pass the flag --reset-results on first run.
      upload_on_last_run: If True, pass the flag --upload-results on last run.
      results_label: A value for the option flag --results-label.
          The arguments reset_on_first_run, upload_on_last_run and results_label
          are all ignored if the test is not a Telemetry test.

    Returns:
      (values dict, 0) if --debug_ignore_perf_test was passed.
      (values dict, 0, test output) if the test was run successfully.
      (error message, -1) if the test couldn't be run.
      (error message, -1, test output) if the test ran but there was an error.
    """
    success_code, failure_code = 0, -1

    if self.opts.debug_ignore_perf_test:
      fake_results = {
          'mean': 0.0,
          'std_err': 0.0,
          'std_dev': 0.0,
          'values': [0.0]
      }
      return (fake_results, success_code)

    # For Windows platform set posix=False, to parse windows paths correctly.
    # On Windows, path separators '\' or '\\' are replace by '' when posix=True,
    # refer to http://bugs.python.org/issue1724822. By default posix=True.
    args = shlex.split(command_to_run, posix=not bisect_utils.IsWindowsHost())

    if not _GenerateProfileIfNecessary(args):
      err_text = 'Failed to generate profile for performance test.'
      return (err_text, failure_code)

    # If running a Telemetry test for Chrome OS, insert the remote IP and
    # identity parameters.
    is_telemetry = bisect_utils.IsTelemetryCommand(command_to_run)
    if self.opts.target_platform == 'cros' and is_telemetry:
      args.append('--remote=%s' % self.opts.cros_remote_ip)
      args.append('--identity=%s' % CROS_TEST_KEY_PATH)

    start_time = time.time()

    metric_values = []
    output_of_all_runs = ''
    for i in xrange(self.opts.repeat_test_count):
      # Can ignore the return code since if the tests fail, it won't return 0.
      current_args = copy.copy(args)
      if is_telemetry:
        if i == 0 and reset_on_first_run:
          current_args.append('--reset-results')
        elif i == self.opts.repeat_test_count - 1 and upload_on_last_run:
          current_args.append('--upload-results')
        if results_label:
          current_args.append('--results-label=%s' % results_label)
      try:
        output, return_code = bisect_utils.RunProcessAndRetrieveOutput(
            current_args, cwd=self.src_cwd)
      except OSError, e:
        if e.errno == errno.ENOENT:
          err_text  = ('Something went wrong running the performance test. '
                       'Please review the command line:\n\n')
          if 'src/' in ' '.join(args):
            err_text += ('Check that you haven\'t accidentally specified a '
                         'path with src/ in the command.\n\n')
          err_text += ' '.join(args)
          err_text += '\n'

          return (err_text, failure_code)
        raise

      output_of_all_runs += output
      if self.opts.output_buildbot_annotations:
        print output

      if self._IsBisectModeUsingMetric():
        metric_values += _ParseMetricValuesFromOutput(metric, output)
        # If we're bisecting on a metric (ie, changes in the mean or
        # standard deviation) and no metric values are produced, bail out.
        if not metric_values:
          break
      elif self._IsBisectModeReturnCode():
        metric_values.append(return_code)

      elapsed_minutes = (time.time() - start_time) / 60.0
      if elapsed_minutes >= self.opts.max_time_minutes:
        break

    if len(metric_values) == 0:
      err_text = 'Metric %s was not found in the test output.' % metric
      # TODO(qyearsley): Consider also getting and displaying a list of metrics
      # that were found in the output here.
      return (err_text, failure_code, output_of_all_runs)

    # If we're bisecting on return codes, we're really just looking for zero vs
    # non-zero.
    if self._IsBisectModeReturnCode():
      # If any of the return codes is non-zero, output 1.
      overall_return_code = 0 if (
          all(current_value == 0 for current_value in metric_values)) else 1

      values = {
          'mean': overall_return_code,
          'std_err': 0.0,
          'std_dev': 0.0,
          'values': metric_values,
      }

      print 'Results of performance test: Command returned with %d' % (
          overall_return_code)
      print
    else:
      # Need to get the average value if there were multiple values.
      truncated_mean = math_utils.TruncatedMean(
          metric_values, self.opts.truncate_percent)
      standard_err = math_utils.StandardError(metric_values)
      standard_dev = math_utils.StandardDeviation(metric_values)

      if self._IsBisectModeStandardDeviation():
        metric_values = [standard_dev]

      values = {
          'mean': truncated_mean,
          'std_err': standard_err,
          'std_dev': standard_dev,
          'values': metric_values,
      }

      print 'Results of performance test: %12f %12f' % (
          truncated_mean, standard_err)
      print
    return (values, success_code, output_of_all_runs)

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
    if (not is_base
        and DEPOT_DEPS_NAME[depot]['depends']
        and self.source_control.IsGit()):
      svn_rev = self.source_control.SVNFindRev(revision)

      for d in DEPOT_DEPS_NAME[depot]['depends']:
        self.ChangeToDepotWorkingDirectory(d)

        dependant_rev = self.source_control.ResolveToRevision(
            svn_rev, d, DEPOT_DEPS_NAME, -1000)

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
    for (path, _, files) in os.walk(self.src_cwd):
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
      # Removes third_party/Webkit directory.
      return bisect_utils.RemoveThirdPartyDirectory('Webkit')
    return True

  def PerformCrosChrootCleanup(self):
    """Deletes the chroot.

    Returns:
        True if successful.
    """
    cwd = os.getcwd()
    self.ChangeToDepotWorkingDirectory('cros')
    cmd = [CROS_SDK_PATH, '--delete']
    return_code = bisect_utils.RunProcess(cmd)
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
    return_code = bisect_utils.RunProcess(cmd)
    os.chdir(cwd)
    return not return_code

  def PerformPreSyncCleanup(self, revision, depot):
    """Performs any necessary cleanup before syncing.

    Returns:
      True if successful.
    """
    if depot == 'chromium' or depot == 'android-chrome':
      # Removes third_party/libjingle. At some point, libjingle was causing
      # issues syncing when using the git workflow (crbug.com/266324).
      os.chdir(self.src_cwd)
      if not bisect_utils.RemoveThirdPartyDirectory('libjingle'):
        return False
      # Removes third_party/skia. At some point, skia was causing
      #  issues syncing when using the git workflow (crbug.com/377951).
      if not bisect_utils.RemoveThirdPartyDirectory('skia'):
        return False
      if depot == 'chromium':
        # The fast webkit cleanup doesn't work for android_chrome
        # The switch from Webkit to Blink that this deals with now happened
        # quite a long time ago so this is unlikely to be a problem.
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
        output = bisect_utils.CheckRunGit(cmd)

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
          # Hack to support things that got changed.
          command_to_run = self.GetCompatibleCommand(
              command_to_run, revision, depot)
          results = self.RunPerformanceTestAndParseResults(command_to_run,
                                                           metric)
          # Restore build output directory once the tests are done, to avoid
          # any descrepancy.
          if self.IsDownloadable(depot) and revision:
            self.BackupOrRestoreOutputdirectory(restore=True)

          if results[1] == 0:
            external_revisions = self._Get3rdPartyRevisions(depot)

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
          return ('Failed to build revision: [%s]' % str(revision),
                  BUILD_RESULT_FAIL)
      else:
        return ('Failed to run [gclient runhooks].', BUILD_RESULT_FAIL)
    else:
      return ('Failed to sync revision: [%s]' % str(revision),
              BUILD_RESULT_FAIL)

  def _CheckIfRunPassed(self, current_value, known_good_value, known_bad_value):
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
    if self.opts.bisect_mode == BISECT_MODE_STD_DEV:
      dist_to_good_value = abs(current_value['std_dev'] -
          known_good_value['std_dev'])
      dist_to_bad_value = abs(current_value['std_dev'] -
          known_bad_value['std_dev'])
    else:
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
      assert False, ('Unknown depot [ %s ] encountered. Possibly a new one '
                     'was added without proper support?' % depot_name)

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
            min_revision_data['revision'])
        or not self._GetV8BleedingEdgeFromV8TrunkIfMappable(
            max_revision_data['revision'])):
      self.warnings.append(
          'Trunk revisions in V8 did not map directly to bleeding_edge. '
          'Attempted to expand the range to find V8 rolls which did map '
          'directly to bleeding_edge revisions, but results might not be '
          'valid.')

  def _FindNextDepotToBisect(
      self, current_depot, min_revision_data, max_revision_data):
    """Decides which depot the script should dive into next (if any).

    Args:
      current_depot: Current depot being bisected.
      min_revision_data: Data about the earliest revision in the bisect range.
      max_revision_data: Data about the latest revision in the bisect range.

    Returns:
      Name of the depot to bisect next, or None.
    """
    external_depot = None
    for next_depot in DEPOT_NAMES:
      if DEPOT_DEPS_NAME[next_depot].has_key('platform'):
        if DEPOT_DEPS_NAME[next_depot]['platform'] != os.name:
          continue

      if not (DEPOT_DEPS_NAME[next_depot]['recurse']
              and min_revision_data['depot']
              in DEPOT_DEPS_NAME[next_depot]['from']):
        continue

      if current_depot == 'v8':
        # We grab the bleeding_edge info here rather than earlier because we
        # finally have the revision range. From that we can search forwards and
        # backwards to try to match trunk revisions to bleeding_edge.
        self._FillInV8BleedingEdgeInfo(min_revision_data, max_revision_data)

      if (min_revision_data['external'].get(next_depot) ==
          max_revision_data['external'].get(next_depot)):
        continue

      if (min_revision_data['external'].get(next_depot) and
          max_revision_data['external'].get(next_depot)):
        external_depot = next_depot
        break

    return external_depot

  def PrepareToBisectOnDepot(
      self, current_depot, end_revision, start_revision, previous_revision):
    """Changes to the appropriate directory and gathers a list of revisions
    to bisect between |start_revision| and |end_revision|.

    Args:
      current_depot: The depot we want to bisect.
      end_revision: End of the revision range.
      start_revision: Start of the revision range.
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
    bad_run_results = self.SyncBuildAndRunRevision(
        bad_rev, target_depot, cmd, metric)

    good_run_results = None

    if not bad_run_results[1]:
      good_run_results = self.SyncBuildAndRunRevision(
          good_rev, target_depot, cmd, metric)

    return (bad_run_results, good_run_results)

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
          output = bisect_utils.CheckRunGit(cmd)
          commit_time = int(output)

          # Try looking for a commit that touches the .DEPS.git file in the
          # next 15 minutes after the DEPS file change.
          cmd = ['log', '--format=%H', '-1',
              '--before=%d' % (commit_time + 900), '--after=%d' % commit_time,
              'origin/master', bisect_utils.FILE_DEPS_GIT]
          output = bisect_utils.CheckRunGit(cmd)
          output = output.strip()
          if output:
            self.warnings.append('Detected change to DEPS and modified '
                'revision range to include change to .DEPS.git')
            return (output, good_revision)
          else:
            self.warnings.append('Detected change to DEPS but couldn\'t find '
                'matching change to .DEPS.git')
    return (bad_revision, good_revision)

  def CheckIfRevisionsInProperOrder(
      self, target_depot, good_revision, bad_revision):
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

      output = bisect_utils.CheckRunGit(cmd, cwd=cwd)
      good_commit_time = int(output)

      cmd = ['log', '--format=%ct', '-1', bad_revision]
      output = bisect_utils.CheckRunGit(cmd, cwd=cwd)
      bad_commit_time = int(output)

      return good_commit_time <= bad_commit_time
    else:
      # Cros/svn use integers
      return int(good_revision) <= int(bad_revision)

  def CanPerformBisect(self, revision_to_check):
    """Checks whether a given revision is bisectable.

    Note: At present it checks whether a given revision is bisectable on
    android bots(refer crbug.com/385324).

    Args:
      revision_to_check: Known good revision.

    Returns:
      A dictionary indicating the result. If revision is not bisectable,
      this will contain the field "error", otherwise None.
    """
    if self.opts.target_platform == 'android':
      revision_to_check = self.source_control.SVNFindRev(revision_to_check)
      if (bisect_utils.IsStringInt(revision_to_check)
          and revision_to_check < 265549):
        return {'error': (
            'Bisect cannot conitnue for the given revision range.\n'
            'It is impossible to bisect Android regressions '
            'prior to r265549, which allows the bisect bot to '
            'rely on Telemetry to do apk installation of the most recently '
            'built local ChromeShell(refer to crbug.com/385324).\n'
            'Please try bisecting revisions greater than or equal to r265549.')}
    return None

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
    results = {
        'revision_data' : {},
        'error' : None,
    }

    # Choose depot to bisect first
    target_depot = 'chromium'
    if self.opts.target_platform == 'cros':
      target_depot = 'cros'
    elif self.opts.target_platform == 'android-chrome':
      target_depot = 'android-chrome'

    cwd = os.getcwd()
    self.ChangeToDepotWorkingDirectory(target_depot)

    # If they passed SVN CL's, etc... we can try match them to git SHA1's.
    bad_revision = self.source_control.ResolveToRevision(
        bad_revision_in, target_depot, DEPOT_DEPS_NAME, 100)
    good_revision = self.source_control.ResolveToRevision(
        good_revision_in, target_depot, DEPOT_DEPS_NAME, -100)

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
      results['error'] = ('bad_revision < good_revision, did you swap these '
                          'by mistake?')
      return results

    bad_revision, good_revision = self.NudgeRevisionsIfDEPSChange(
        bad_revision, good_revision)

    if self.opts.output_buildbot_annotations:
      bisect_utils.OutputAnnotationStepStart('Gathering Revisions')

    cannot_bisect = self.CanPerformBisect(good_revision)
    if cannot_bisect:
      results['error'] = cannot_bisect.get('error')
      return results

    print 'Gathering revision range for bisection.'
    # Retrieve a list of revisions to do bisection on.
    src_revision_list = self.GetRevisionList(
        target_depot, bad_revision, good_revision)

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

        revision_data[current_revision_id] = {
            'value' : None,
            'passed' : '?',
            'depot' : target_depot,
            'external' : None,
            'perf_time' : 0,
            'build_time' : 0,
            'sort' : sort_key_ids,
        }
        revision_list.append(current_revision_id)

      min_revision = 0
      max_revision = len(revision_list) - 1

      self.PrintRevisionsToBisectMessage(revision_list, target_depot)

      if self.opts.output_buildbot_annotations:
        bisect_utils.OutputAnnotationStepStart('Gathering Reference Values')

      print 'Gathering reference values for bisection.'

      # Perform the performance tests on the good and bad revisions, to get
      # reference values.
      bad_results, good_results = self.GatherReferenceValues(good_revision,
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
            external_depot = self._FindNextDepotToBisect(
                current_depot, min_revision_data, max_revision_data)

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

            new_revision_list = self.PrepareToBisectOnDepot(
                external_depot, latest_revision, earliest_revision,
                previous_revision)

            if not new_revision_list:
              results['error'] = ('An error occurred attempting to retrieve '
                                  'revision range: [%s..%s]' %
                                  (earliest_revision, latest_revision))
              return results

            _AddRevisionsIntoRevisionData(
                new_revision_list, external_depot, min_revision_data['sort'],
                revision_data)

            # Reset the bisection and perform it on the newly inserted
            # changelists.
            revision_list = new_revision_list
            min_revision = 0
            max_revision = len(revision_list) - 1
            sort_key_ids += len(revision_list)

            print ('Regression in metric %s appears to be the result of '
                   'changes in [%s].' % (metric, external_depot))

            self.PrintRevisionsToBisectMessage(revision_list, external_depot)

            continue
          else:
            break
        else:
          next_revision_index = (int((max_revision - min_revision) / 2) +
                                 min_revision)

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

          passed_regression = self._CheckIfRunPassed(run_results[0],
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
      results['error'] = ('An error occurred attempting to retrieve revision '
                          'range: [%s..%s]' % (good_revision, bad_revision))

    return results

  def _PrintPartialResults(self, results_dict):
    revision_data = results_dict['revision_data']
    revision_data_sorted = sorted(revision_data.iteritems(),
                                  key = lambda x: x[1]['sort'])
    results_dict = self._GetResultsDict(revision_data, revision_data_sorted)

    self._PrintTestedCommitsTable(revision_data_sorted,
                                  results_dict['first_working_revision'],
                                  results_dict['last_broken_revision'],
                                  100, final_step=False)

  def _ConfidenceLevelStatus(self, results_dict):
    if not results_dict['confidence']:
      return None
    confidence_status = 'Successful with %(level)s confidence%(warning)s.'
    if results_dict['confidence'] >= 95:
      level = 'high'
    else:
      level = 'low'
    warning = ' and warnings'
    if not self.warnings:
      warning = ''
    return confidence_status % {'level': level, 'warning': warning}

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
    email_info = ''
    if not info['email'].startswith(info['author']):
      email_info = '\nEmail   : %s' % info['email']
    commit_link = self._GetViewVCLinkFromDepotAndHash(cl, depot)
    if commit_link:
      commit_info = '\nLink    : %s' % commit_link
    else:
      commit_info = ('\nFailed to parse svn revision from body:\n%s' %
                     info['body'])
    print RESULTS_REVISION_INFO % {
        'subject': info['subject'],
        'author': info['author'],
        'email_info': email_info,
        'commit_info': commit_info,
        'cl': cl,
        'cl_date': info['date']
    }

  def _PrintTestedCommitsHeader(self):
    if self.opts.bisect_mode == BISECT_MODE_MEAN:
      _PrintTableRow(
          [20, 70, 14, 12, 13],
          ['Depot', 'Commit SHA', 'Mean', 'Std. Error', 'State'])
    elif self.opts.bisect_mode == BISECT_MODE_STD_DEV:
      _PrintTableRow(
          [20, 70, 14, 12, 13],
          ['Depot', 'Commit SHA', 'Std. Error', 'Mean', 'State'])
    elif self.opts.bisect_mode == BISECT_MODE_RETURN_CODE:
      _PrintTableRow(
          [20, 70, 14, 13],
          ['Depot', 'Commit SHA', 'Return Code', 'State'])
    else:
      assert False, 'Invalid bisect_mode specified.'

  def _PrintTestedCommitsEntry(self, current_data, cl_link, state_str):
    if self.opts.bisect_mode == BISECT_MODE_MEAN:
      std_error = '+-%.02f' % current_data['value']['std_err']
      mean = '%.02f' % current_data['value']['mean']
      _PrintTableRow(
          [20, 70, 12, 14, 13],
          [current_data['depot'], cl_link, mean, std_error, state_str])
    elif self.opts.bisect_mode == BISECT_MODE_STD_DEV:
      std_error = '+-%.02f' % current_data['value']['std_err']
      mean = '%.02f' % current_data['value']['mean']
      _PrintTableRow(
          [20, 70, 12, 14, 13],
          [current_data['depot'], cl_link, std_error, mean, state_str])
    elif self.opts.bisect_mode == BISECT_MODE_RETURN_CODE:
      mean = '%d' % current_data['value']['mean']
      _PrintTableRow(
          [20, 70, 14, 13],
          [current_data['depot'], cl_link, mean, state_str])

  def _PrintTestedCommitsTable(
      self, revision_data_sorted, first_working_revision, last_broken_revision,
      confidence, final_step=True):
    print
    if final_step:
      print '===== TESTED COMMITS ====='
    else:
      print '===== PARTIAL RESULTS ====='
    self._PrintTestedCommitsHeader()
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

        cl_link = self._GetViewVCLinkFromDepotAndHash(current_id,
            current_data['depot'])
        if not cl_link:
          cl_link = current_id
        self._PrintTestedCommitsEntry(current_data, cl_link, state_str)

  def _PrintReproSteps(self):
    """Prints out a section of the results explaining how to run the test.

    This message includes the command used to run the test.
    """
    command = '$ ' + self.opts.command
    if bisect_utils.IsTelemetryCommand(self.opts.command):
      command += ('\nAlso consider passing --profiler=list to see available '
                  'profilers.')
    print REPRO_STEPS_LOCAL % {'command': command}
    print REPRO_STEPS_TRYJOB % {'command': command}

  def _PrintOtherRegressions(self, other_regressions, revision_data):
    """Prints a section of the results about other potential regressions."""
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

  def _GetResultsDict(self, revision_data, revision_data_sorted):
    # Find range where it possibly broke.
    first_working_revision = None
    first_working_revision_index = -1
    last_broken_revision = None
    last_broken_revision_index = -1

    culprit_revisions = []
    other_regressions = []
    regression_size = 0.0
    regression_std_err = 0.0
    confidence = 0.0

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
      mean_of_bad_runs = math_utils.Mean(broken_mean)
      mean_of_good_runs = math_utils.Mean(working_mean)

      regression_size = 100 * math_utils.RelativeChange(mean_of_good_runs,
                                                      mean_of_bad_runs)
      if math.isnan(regression_size):
        regression_size = 'zero-to-nonzero'

      regression_std_err = math.fabs(math_utils.PooledStandardError(
          [working_mean, broken_mean]) /
          max(0.0001, min(mean_of_good_runs, mean_of_bad_runs))) * 100.0

      # Give a "confidence" in the bisect. At the moment we use how distinct the
      # values are before and after the last broken revision, and how noisy the
      # overall graph is.
      confidence = ConfidenceScore(working_means, broken_means)

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
        output, return_code = bisect_utils.RunProcessAndRetrieveOutput(cmd)

        changes = []
        assert not return_code, ('An error occurred while running '
                                 '"%s"' % ' '.join(cmd))
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

      # Check for any other possible regression ranges.
      other_regressions = _FindOtherRegressions(
          revision_data_sorted, mean_of_bad_runs > mean_of_good_runs)

    return {
        'first_working_revision': first_working_revision,
        'last_broken_revision': last_broken_revision,
        'culprit_revisions': culprit_revisions,
        'other_regressions': other_regressions,
        'regression_size': regression_size,
        'regression_std_err': regression_std_err,
        'confidence': confidence,
    }

  def _CheckForWarnings(self, results_dict):
    if len(results_dict['culprit_revisions']) > 1:
      self.warnings.append('Due to build errors, regression range could '
                           'not be narrowed down to a single commit.')
    if self.opts.repeat_test_count == 1:
      self.warnings.append('Tests were only set to run once. This may '
                           'be insufficient to get meaningful results.')
    if results_dict['confidence'] < 100:
      if results_dict['confidence']:
        self.warnings.append(
            'Confidence is less than 100%. There could be other candidates '
            'for this regression. Try bisecting again with increased '
            'repeat_count or on a sub-metric that shows the regression more '
            'clearly.')
      else:
        self.warnings.append(
          'Confidence is 0%. Try bisecting again on another platform, with '
          'increased repeat_count or on a sub-metric that shows the '
          'regression more clearly.')

  def FormatAndPrintResults(self, bisect_results):
    """Prints the results from a bisection run in a readable format.

    Args:
      bisect_results: The results from a bisection test run.
    """
    revision_data = bisect_results['revision_data']
    revision_data_sorted = sorted(revision_data.iteritems(),
                                  key = lambda x: x[1]['sort'])
    results_dict = self._GetResultsDict(revision_data, revision_data_sorted)

    self._CheckForWarnings(results_dict)

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

    self._PrintBanner(results_dict)
    self._PrintWarnings()

    if results_dict['culprit_revisions'] and results_dict['confidence']:
      for culprit in results_dict['culprit_revisions']:
        cl, info, depot = culprit
        self._PrintRevisionInfo(cl, info, depot)
      if results_dict['other_regressions']:
        self._PrintOtherRegressions(results_dict['other_regressions'],
                                    revision_data)
    self._PrintTestedCommitsTable(revision_data_sorted,
                                  results_dict['first_working_revision'],
                                  results_dict['last_broken_revision'],
                                  results_dict['confidence'])
    _PrintStepTime(revision_data_sorted)
    self._PrintReproSteps()
    _PrintThankYou()
    if self.opts.output_buildbot_annotations:
      bisect_utils.OutputAnnotationStepClosed()

  def _PrintBanner(self, results_dict):
    if self._IsBisectModeReturnCode():
      metrics = 'N/A'
      change = 'Yes'
    else:
      metrics = '/'.join(self.opts.metric)
      change = '%.02f%% (+/-%.02f%%)' % (
          results_dict['regression_size'], results_dict['regression_std_err'])

    if results_dict['culprit_revisions'] and results_dict['confidence']:
      status = self._ConfidenceLevelStatus(results_dict)
    else:
      status = 'Failure, could not reproduce.'
      change = 'Bisect could not reproduce a change.'

    print RESULTS_BANNER % {
        'status': status,
        'command': self.opts.command,
        'metrics': metrics,
        'change': change,
        'confidence': results_dict['confidence'],
    }

  def _PrintWarnings(self):
    """Prints a list of warning strings if there are any."""
    if not self.warnings:
      return
    print
    print 'WARNINGS:'
    for w in set(self.warnings):
      print '  ! %s' % w


def _IsPlatformSupported():
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


def RemoveBuildFiles(build_type):
  """Removes build files from previous runs."""
  if RmTreeAndMkDir(os.path.join('out', build_type)):
    if RmTreeAndMkDir(os.path.join('build', build_type)):
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
    self.goma_dir = None
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
    self.target_build_type = 'Release'
    self.builder_host = None
    self.builder_port = None
    self.bisect_mode = BISECT_MODE_MEAN

  @staticmethod
  def _CreateCommandLineParser():
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
    group.add_option('--bisect_mode',
                     type='choice',
                     choices=[BISECT_MODE_MEAN, BISECT_MODE_STD_DEV,
                        BISECT_MODE_RETURN_CODE],
                     default=BISECT_MODE_MEAN,
                     help='The bisect mode. Choices are to bisect on the '
                     'difference in mean, std_dev, or return_code.')
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
                     action='store_true',
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
                     action='store_true',
                     help='Add a bunch of extra threads for goma, and enable '
                     'goma')
    group.add_option('--goma_dir',
                     help='Path to goma tools (or system default if not '
                     'specified).')
    group.add_option('--output_buildbot_annotations',
                     action='store_true',
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
    group.add_option('--target_build_type',
                     type='choice',
                     choices=['Release', 'Debug'],
                     default='Release',
                     help='The target build type. Choices are "Release" '
                     '(default), or "Debug".')
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
                     action='store_true',
                     help='DEBUG: Don\'t perform builds.')
    group.add_option('--debug_ignore_sync',
                     action='store_true',
                     help='DEBUG: Don\'t perform syncs.')
    group.add_option('--debug_ignore_perf_test',
                     action='store_true',
                     help='DEBUG: Don\'t perform performance tests.')
    parser.add_option_group(group)
    return parser

  def ParseCommandLine(self):
    """Parses the command line for bisect options."""
    parser = self._CreateCommandLineParser()
    opts, _ = parser.parse_args()

    try:
      if not opts.command:
        raise RuntimeError('missing required parameter: --command')

      if not opts.good_revision:
        raise RuntimeError('missing required parameter: --good_revision')

      if not opts.bad_revision:
        raise RuntimeError('missing required parameter: --bad_revision')

      if not opts.metric and opts.bisect_mode != BISECT_MODE_RETURN_CODE:
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
        bisect_utils.RunProcess(['sudo', 'true'])

        if not opts.cros_board:
          raise RuntimeError('missing required parameter: --cros_board')

        if not opts.cros_remote_ip:
          raise RuntimeError('missing required parameter: --cros_remote_ip')

        if not opts.working_directory:
          raise RuntimeError('missing required parameter: --working_directory')

      metric_values = opts.metric.split('/')
      if (len(metric_values) != 2 and
          opts.bisect_mode != BISECT_MODE_RETURN_CODE):
        raise RuntimeError('Invalid metric specified: [%s]' % opts.metric)

      opts.metric = metric_values
      opts.repeat_test_count = min(max(opts.repeat_test_count, 1), 100)
      opts.max_time_minutes = min(max(opts.max_time_minutes, 1), 60)
      opts.truncate_percent = min(max(opts.truncate_percent, 0), 25)
      opts.truncate_percent = opts.truncate_percent / 100.0

      for k, v in opts.__dict__.iteritems():
        assert hasattr(self, k), 'Invalid %s attribute in BisectOptions.' % k
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
      assert hasattr(opts, k), 'Invalid %s attribute in BisectOptions.' % k
      setattr(opts, k, v)

    metric_values = opts.metric.split('/')
    if len(metric_values) != 2:
      raise RuntimeError('Invalid metric specified: [%s]' % opts.metric)

    opts.metric = metric_values
    opts.repeat_test_count = min(max(opts.repeat_test_count, 1), 100)
    opts.max_time_minutes = min(max(opts.max_time_minutes, 1), 60)
    opts.truncate_percent = min(max(opts.truncate_percent, 0), 25)
    opts.truncate_percent = opts.truncate_percent / 100.0

    return opts


def main():

  try:
    opts = BisectOptions()
    opts.ParseCommandLine()

    if opts.extra_src:
      extra_src = bisect_utils.LoadExtraSrc(opts.extra_src)
      if not extra_src:
        raise RuntimeError('Invalid or missing --extra_src.')
      _AddAdditionalDepotInfo(extra_src.GetAdditionalDepotInfo())

    if opts.working_directory:
      custom_deps = bisect_utils.DEFAULT_GCLIENT_CUSTOM_DEPS
      if opts.no_custom_deps:
        custom_deps = None
      bisect_utils.CreateBisectDirectoryAndSetupDepot(opts, custom_deps)

      os.chdir(os.path.join(os.getcwd(), 'src'))

      if not RemoveBuildFiles(opts.target_build_type):
        raise RuntimeError('Something went wrong removing the build files.')

    if not _IsPlatformSupported():
      raise RuntimeError('Sorry, this platform isn\'t supported yet.')

    # Check what source control method is being used, and create a
    # SourceControl object if possible.
    source_control = source_control_module.DetermineAndCreateSourceControl(opts)

    if not source_control:
      raise RuntimeError(
          'Sorry, only the git workflow is supported at the moment.')

    # gClient sync seems to fail if you're not in master branch.
    if (not source_control.IsInProperBranch() and
        not opts.debug_ignore_sync and
        not opts.working_directory):
      raise RuntimeError('You must switch to master branch to run bisection.')
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
