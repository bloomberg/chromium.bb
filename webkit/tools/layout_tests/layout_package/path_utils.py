# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This package contains utility methods for manipulating paths and
filenames for test results and baselines. It also contains wrappers
of a few routines in platform_utils.py so that platform_utils.py can
be considered a 'protected' package - i.e., this file should be
the only file that ever includes platform_utils. This leads to
us including a few things that don't really have anything to do
 with paths, unfortunately."""

import errno
import os
import stat
import sys

import platform_utils
import platform_utils_win
import platform_utils_mac
import platform_utils_linux

# Cache some values so we don't have to recalculate them. _basedir is
# used by PathFromBase() and caches the full (native) path to the top
# of the source tree (/src). _baseline_search_path is used by
# ExpectedBaseline() and caches the list of native paths to search
# for baseline results.
_basedir = None
_baseline_search_path = None

class PathNotFound(Exception): pass

def LayoutTestsDir(path=None):
  """Returns the fully-qualified path to the directory containing the input
  data for the specified layout test."""
  return PathFromBase('third_party', 'WebKit');

def ChromiumBaselinePath(platform=None):
  """Returns the full path to the directory containing expected
  baseline results from chromium ports. If |platform| is None, the
  currently executing platform is used."""
  if platform is None:
    platform = platform_utils.PlatformName()
  return PathFromBase('webkit', 'data', 'layout_tests', 'platform', platform)

def WebKitBaselinePath(platform):
  """Returns the full path to the directory containing expected
  baseline results from WebKit ports."""
  return PathFromBase('third_party', 'WebKit', 'LayoutTests',
                      'platform', platform)

def BaselineSearchPath(platform=None):
  """Returns the list of directories to search for baselines/results for a
  given platform, in order of preference. Paths are relative to the top of the
  source tree. If parameter platform is None, returns the list for the current
  platform that the script is running on."""
  if platform is None:
    return platform_utils.BaselineSearchPath(False)
  elif platform.startswith('mac'):
    return platform_utils_mac.BaselineSearchPath(True)
  elif platform.startswith('win'):
    return platform_utils_win.BaselineSearchPath(True)
  elif platform.startswith('linux'):
    return platform_utils_linux.BaselineSearchPath(True)
  else:
    return platform_utils.BaselineSearchPath(False)

def ExpectedBaseline(filename, suffix, platform=None, all_baselines=False):
  """Given a test name, finds where the baseline result is located. The
  result is returned as a pair of values, the absolute path to top of the test
  results directory, and the relative path from there to the results file.

  Both return values will be in the format appropriate for the
  current platform (e.g., "\\" for path separators on Windows).

  If the results file is not found, then None will be returned for the
  directory, but the expected relative pathname will still be returned.

  Args:
     filename: absolute filename to test file
     suffix: file suffix of the expected results, including dot; e.g. '.txt'
         or '.png'.  This should not be None, but may be an empty string.
     platform: layout test platform: 'win', 'linux' or 'mac'. Defaults to the
               current platform.
     all_baselines: If True, return an ordered list of all baseline paths
                    for the given platform. If False, return only the first
                    one.
  Returns
     a list of ( platform_dir, results_filename ), where
       platform_dir - abs path to the top of the results tree (or test tree)
       results_filename - relative path from top of tree to the results file
         (os.path.join of the two gives you the full path to the file, unless
          None was returned.)
  """
  global _baseline_search_path
  global _search_path_platform
  testname = os.path.splitext(RelativeTestFilename(filename))[0]

  # While we still have tests in both LayoutTests/ and chrome/ we need
  # to strip that outer directory.
  # TODO(pamg): Once we upstream all of chrome/, clean this up.
  platform_filename = testname + '-expected' + suffix
  testdir, base_filename = platform_filename.split('/', 1)

  if (_baseline_search_path is None) or (_search_path_platform != platform):
    _baseline_search_path = BaselineSearchPath(platform)
    _search_path_platform = platform

  current_platform_dir = ChromiumBaselinePath(PlatformName(platform))

  baselines = []
  foundCurrentPlatform = False
  for platform_dir in _baseline_search_path:
    # Find current platform from baseline search paths and start from there.
    if platform_dir == current_platform_dir:
      foundCurrentPlatform = True

    if foundCurrentPlatform:
      # TODO(pamg): Clean this up once we upstream everything in chrome/.
      if os.path.basename(platform_dir).startswith('chromium'):
        if os.path.exists(os.path.join(platform_dir, platform_filename)):
          baselines.append((platform_dir, platform_filename))
      else:
        if os.path.exists(os.path.join(platform_dir, base_filename)):
          baselines.append((platform_dir, base_filename))

      if not all_baselines and baselines:
        return baselines

  # If it wasn't found in a platform directory, return the expected result
  # in the test directory, even if no such file actually exists.
  platform_dir = LayoutTestsDir(filename)
  if os.path.exists(os.path.join(platform_dir, platform_filename)):
    baselines.append((platform_dir, platform_filename))

  if baselines:
    return baselines

  return [(None, platform_filename)]

def ExpectedFilename(filename, suffix):
  """Given a test name, returns an absolute path to its expected results.

  If no expected results are found in any of the searched directories, the
  directory in which the test itself is located will be returned. The return
  value is in the format appropriate for the platform (e.g., "\\" for
  path separators on windows).

  Args:
     filename: absolute filename to test file
     suffix: file suffix of the expected results, including dot; e.g. '.txt'
         or '.png'.  This should not be None, but may be an empty string.
     platform: the most-specific directory name to use to build the
         search list of directories, e.g., 'chromium-win', or
         'chromium-mac-leopard' (we follow the WebKit format)
  """
  platform_dir, platform_filename = ExpectedBaseline(filename, suffix)[0]
  if platform_dir:
    return os.path.join(platform_dir, platform_filename)
  return os.path.join(LayoutTestsDir(filename), platform_filename)

def RelativeTestFilename(filename):
  """Provide the filename of the test relative to the layout data
  directory as a unix style path (a/b/c)."""
  return _WinPathToUnix(filename[len(LayoutTestsDir(filename)) + 1:])

def _WinPathToUnix(path):
  """Convert a windows path to use unix-style path separators (a/b/c)."""
  return path.replace('\\', '/')

#
# Routines that are arguably platform-specific but have been made
# generic for now (they used to be in platform_utils_*)
#
def FilenameToUri(full_path):
  """Convert a test file to a URI."""
  LAYOUTTESTS_DIR = "LayoutTests/"
  LAYOUTTEST_HTTP_DIR = "LayoutTests/http/tests/"
  LAYOUTTEST_WEBSOCKET_DIR = "LayoutTests/websocket/tests/"

  relative_path = _WinPathToUnix(RelativeTestFilename(full_path))
  port = None
  use_ssl = False

  if relative_path.startswith(LAYOUTTEST_HTTP_DIR):
    # LayoutTests/http/tests/ run off port 8000 and ssl/ off 8443
    relative_path = relative_path[len(LAYOUTTEST_HTTP_DIR):]
    port = 8000
  elif relative_path.startswith(LAYOUTTEST_WEBSOCKET_DIR):
    # LayoutTests/websocket/tests/ run off port 8880 and 9323
    # Note: the root is LayoutTests/, not LayoutTests/websocket/tests/
    relative_path = relative_path[len(LAYOUTTESTS_DIR):]
    port = 8880

  # Make LayoutTests/http/tests/local run as local files. This is to mimic the
  # logic in run-webkit-tests.
  # TODO(jianli): Consider extending this to "media/".
  if port and not relative_path.startswith("local/"):
    if relative_path.startswith("ssl/"):
      port += 443
      protocol = "https"
    else:
      protocol = "http"
    return "%s://127.0.0.1:%u/%s" % (protocol, port, relative_path)

  if sys.platform in ('cygwin', 'win32'):
    return "file:///" + GetAbsolutePath(full_path)
  return "file://" + GetAbsolutePath(full_path)

def GetAbsolutePath(path):
  """Returns an absolute UNIX path."""
  return _WinPathToUnix(os.path.abspath(path))

def MaybeMakeDirectory(*path):
  """Creates the specified directory if it doesn't already exist."""
  # This is a reimplementation of google.path_utils.MaybeMakeDirectory().
  try:
    os.makedirs(os.path.join(*path))
  except OSError, e:
    if e.errno != errno.EEXIST:
      raise

def PathFromBase(*comps):
  """Returns an absolute filename from a set of components specified
  relative to the top of the source tree. If the path does not exist,
  the exception PathNotFound is raised."""
  # This is a reimplementation of google.path_utils.PathFromBase().
  global _basedir
  if _basedir == None:
    # We compute the top of the source tree by finding the absolute
    # path of this source file, and then climbing up three directories
    # as given in subpath. If we move this file, subpath needs to be updated.
    path = os.path.abspath(__file__)
    subpath = os.path.join('webkit','tools','layout_tests')
    _basedir = path[:path.index(subpath)]
  path = os.path.join(_basedir, *comps)
  if not os.path.exists(path):
    raise PathNotFound('could not find %s' % (path))
  return path

def RemoveDirectory(*path):
  """Recursively removes a directory, even if it's marked read-only.

  Remove the directory located at *path, if it exists.

  shutil.rmtree() doesn't work on Windows if any of the files or directories
  are read-only, which svn repositories and some .svn files are.  We need to
  be able to force the files to be writable (i.e., deletable) as we traverse
  the tree.

  Even with all this, Windows still sometimes fails to delete a file, citing
  a permission error (maybe something to do with antivirus scans or disk
  indexing).  The best suggestion any of the user forums had was to wait a
  bit and try again, so we do that too.  It's hand-waving, but sometimes it
  works. :/
  """
  file_path = os.path.join(*path)
  if not os.path.exists(file_path):
    return

  win32 = False
  if sys.platform == 'win32':
    win32 = True
    # Some people don't have the APIs installed. In that case we'll do without.
    try:
      win32api = __import__('win32api')
      win32con = __import__('win32con')
    except ImportError:
      win32 = False

    def remove_with_retry(rmfunc, path):
      os.chmod(path, stat.S_IWRITE)
      if win32:
        win32api.SetFileAttributes(path, win32con.FILE_ATTRIBUTE_NORMAL)
      try:
        return rmfunc(path)
      except EnvironmentError, e:
        if e.errno != errno.EACCES:
          raise
        print 'Failed to delete %s: trying again' % repr(path)
        time.sleep(0.1)
        return rmfunc(path)
  else:
    def remove_with_retry(rmfunc, path):
      if os.path.islink(path):
        return os.remove(path)
      else:
        return rmfunc(path)

  for root, dirs, files in os.walk(file_path, topdown=False):
    # For POSIX:  making the directory writable guarantees removability.
    # Windows will ignore the non-read-only bits in the chmod value.
    os.chmod(root, 0770)
    for name in files:
      remove_with_retry(os.remove, os.path.join(root, name))
    for name in dirs:
      remove_with_retry(os.rmdir, os.path.join(root, name))

  remove_with_retry(os.rmdir, file_path)

#
# Wrappers around platform_utils
#

def PlatformName(platform=None):
  """Returns the appropriate chromium platform name for |platform|. If
     |platform| is None, returns the name of the chromium platform on the
     currently running system. If |platform| is of the form 'chromium-*',
     it is returned unchanged, otherwise 'chromium-' is prepended."""
  if platform == None:
    return platform_utils.PlatformName()
  if not platform.startswith('chromium-'):
    platform = "chromium-" + platform
  return platform

def PlatformVersion():
  return platform_utils.PlatformVersion()

def LigHTTPdExecutablePath():
  return platform_utils.LigHTTPdExecutablePath()

def LigHTTPdModulePath():
  return platform_utils.LigHTTPdModulePath()

def LigHTTPdPHPPath():
  return platform_utils.LigHTTPdPHPPath()

def WDiffPath():
  return platform_utils.WDiffPath()

def TestShellPath(target):
  return platform_utils.TestShellPath(target)

def ImageDiffPath(target):
  return platform_utils.ImageDiffPath(target)

def LayoutTestHelperPath(target):
  return platform_utils.LayoutTestHelperPath(target)

def FuzzyMatchPath():
  return platform_utils.FuzzyMatchPath()

def ShutDownHTTPServer(server_pid):
  return platform_utils.ShutDownHTTPServer(server_pid)

def KillAllTestShells():
  platform_utils.KillAllTestShells()
