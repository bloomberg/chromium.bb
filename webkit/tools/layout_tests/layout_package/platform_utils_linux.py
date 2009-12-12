# Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This is the Linux implementation of the layout_package.platform_utils
   package. This file should only be imported by that package."""

import os
import signal
import subprocess
import sys
import logging

import path_utils

def PlatformName():
  """Returns the name of the platform we're currently running on."""
  return 'chromium-linux' + PlatformVersion()

def PlatformVersion():
  """Returns the version string for the platform, e.g. '-vista' or
  '-snowleopard'. If the platform does not distinguish between
  minor versions, it returns ''."""
  return ''

def GetNumCores():
  """Returns the number of cores on the machine. For hyperthreaded machines,
  this will be double the number of actual processors."""
  num_cores = os.sysconf("SC_NPROCESSORS_ONLN")
  if isinstance(num_cores, int) and num_cores > 0:
    return num_cores
  return 1

def BaselineSearchPath(all_versions=False):
  """Returns the list of directories to search for baselines/results, in
  order of preference. Paths are relative to the top of the source tree."""
  return [path_utils.ChromiumBaselinePath(PlatformName()),
          path_utils.ChromiumBaselinePath('chromium-win'),
          path_utils.WebKitBaselinePath('win'),
          path_utils.WebKitBaselinePath('mac')]

def ApacheExecutablePath():
  """Returns the executable path to start Apache"""
  path = os.path.join("/usr", "sbin", "apache2")
  if os.path.exists(path):
    return path
  print "Unable to fine Apache executable %s" % path
  _MissingApache()

def ApacheConfigFilePath():
  """Returns the path to Apache config file"""
  return path_utils.PathFromBase("third_party", "WebKit", "LayoutTests", "http",
      "conf", "apache2-debian-httpd.conf")

def LigHTTPdExecutablePath():
  """Returns the executable path to start LigHTTPd"""
  binpath = "/usr/sbin/lighttpd"
  if os.path.exists(binpath):
    return binpath
  print "Unable to find LigHTTPd executable %s" % binpath
  _MissingLigHTTPd()

def LigHTTPdModulePath():
  """Returns the library module path for LigHTTPd"""
  modpath = "/usr/lib/lighttpd"
  if os.path.exists(modpath):
    return modpath
  print "Unable to find LigHTTPd modules %s" % modpath
  _MissingLigHTTPd()

def LigHTTPdPHPPath():
  """Returns the PHP executable path for LigHTTPd"""
  binpath = "/usr/bin/php-cgi"
  if os.path.exists(binpath):
    return binpath
  print "Unable to find PHP CGI executable %s" % binpath
  _MissingLigHTTPd()

def WDiffPath():
  """Path to the WDiff executable, which we assume is already installed and
     in the user's $PATH.
  """
  return 'wdiff'

def ImageDiffPath(target):
  """Path to the image_diff binary.

  Args:
    target: Build target mode (debug or release)"""
  return _PathFromBuildResults(target, 'image_diff')

def LayoutTestHelperPath(target):
  """Path to the layout_test helper binary, if needed, empty otherwise"""
  return ''

def TestShellPath(target):
  """Return the platform-specific binary path for our TestShell.

  Args:
    target: Build target mode (debug or release) """
  if target in ('Debug', 'Release'):
    try:
      debug_path = _PathFromBuildResults('Debug', 'test_shell')
      release_path = _PathFromBuildResults('Release', 'test_shell')

      debug_mtime = os.stat(debug_path).st_mtime
      release_mtime = os.stat(release_path).st_mtime

      if debug_mtime > release_mtime and target == 'Release' or \
         release_mtime > debug_mtime and target == 'Debug':
        logging.info('\x1b[31mWarning: you are not running the most ' + \
                     'recent test_shell binary. You need to pass ' + \
                     '--debug or not to select between Debug and ' + \
                     'Release.\x1b[0m')
    # This will fail if we don't have both a debug and release binary.
    # That's fine because, in this case, we must already be running the
    # most up-to-date one.
    except path_utils.PathNotFound:
      pass

  return _PathFromBuildResults(target, 'test_shell')

def FuzzyMatchPath():
  """Return the path to the fuzzy matcher binary."""
  return path_utils.PathFromBase('third_party', 'fuzzymatch', 'fuzzymatch')

def ShutDownHTTPServer(server_pid):
  """Shut down the lighttpd web server. Blocks until it's fully shut down.

  Args:
    server_pid: The process ID of the running server.
  """
  # server_pid is not set when "http_server.py stop" is run manually.
  if server_pid is None:
    # This isn't ideal, since it could conflict with web server processes not
    # started by http_server.py, but good enough for now.
    null = open("/dev/null");
    subprocess.call(['killall', '-TERM', '-u', os.getenv('USER'), 'lighttpd'],
                    stderr=null)
    subprocess.call(['killall', '-TERM', '-u', os.getenv('USER'), 'apache2'],
                    stderr=null)
    null.close()
  else:
    try:
      os.kill(server_pid, signal.SIGTERM)
      #TODO(mmoss) Maybe throw in a SIGKILL just to be sure?
    except OSError:
      # Sometimes we get a bad PID (e.g. from a stale httpd.pid file), so if
      # kill fails on the given PID, just try to 'killall' web servers.
      ShutDownHTTPServer(None)

def KillProcess(pid):
  """Forcefully kill the process.

  Args:
    pid: The id of the process to be killed.
  """
  os.kill(pid, signal.SIGKILL)

def KillAllTestShells():
   """Kills all instances of the test_shell binary currently running."""
   subprocess.Popen(('killall', '-TERM', 'test_shell'),
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE).wait()

#
# Private helper functions
#

def _MissingLigHTTPd():
  print 'Please install using: "sudo apt-get install lighttpd php5-cgi"'
  print 'For complete Linux build requirements, please see:'
  print 'http://code.google.com/p/chromium/wiki/LinuxBuildInstructions'
  sys.exit(1)

def _MissingApache():
  print ('Please install using: "sudo apt-get install apache2 '
      'libapache2-mod-php5"')
  print 'For complete Linux build requirements, please see:'
  print 'http://code.google.com/p/chromium/wiki/LinuxBuildInstructions'
  sys.exit(1)

def _PathFromBuildResults(*pathies):
  # FIXME(dkegel): use latest or warn if more than one found?
  for dir in ["sconsbuild", "out", "xcodebuild"]:
    try:
      return path_utils.PathFromBase(dir, *pathies)
    except:
      pass
  raise path_utils.PathNotFound("Unable to find %s in build tree" %
      (os.path.join(*pathies)))
