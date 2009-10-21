#!/usr/bin/python2.6.2
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


"""Prepares to run the selenium lab tests and finally invokes the selenium
runner main.py.                      

Usage:
  run_lab_test.py [test_config_path]

Args:
  browser_config_path: path to test config file (optional)
"""

import copy
import logging
import optparse
import os
import shutil
import subprocess
import sys
import threading
import time

import configure_ie
import runner_constants as const
import runner_util as run_util
import util

# Resolution to configure video card to before running tests.
SCREEN_WIDTH = 1280
SCREEN_HEIGHT = 1024
SCREEN_BPP = 32

join = os.path.join

if util.IsWindows():
  IMAGE_DIFF_PATH = join(const.O3D_PATH, 'third_party', 'pdiff', 'files', 
                         'bin', 'win', 'perceptualdiff.exe')
elif util.IsMac():
  IMAGE_DIFF_PATH = join(const.O3D_PATH, 'third_party', 'pdiff', 'files',
                         'bin', 'mac', 'perceptualdiff')
else:
  IMAGE_DIFF_PATH = join(const.O3D_PATH, 'third_party', 'pdiff', 'files',
                         'bin', 'linux', 'perceptualdiff')

SELENIUM_TEST_RUNNER_PATH = join(const.O3D_PATH, 'o3d', 'tests', 'selenium',
                                 'main.py')

SELENIUM_JAR_PATH = join(const.O3D_PATH, 'third_party', 'selenium_rc', 'files',
                         'selenium-server', 'selenium-server.jar')

O3D_REFERENCE_IMAGES_PATH = join(const.O3D_PATH, 'o3d', 'o3d_assets', 'tests',
                                 'screenshots')

SCREENSHOTS_PATH = join(const.RESULTS_PATH,'screenshots')

# Set total test timeout to 90 minutes.
TEST_TIMEOUT_SECS = 60 * 90.0

SELENIUM_BROWSER_PREFIXES = {
  'ie': '*iexplore',
  'ff': '*firefox',
  'chr': '*googlechrome',
  'saf': '*safari',
}


class TestRunningThread(threading.Thread):
  """Runs test in separate thread.  Allows timeout if test blocks."""

  def __init__(self, command):
    threading.Thread.__init__(self)
    # Make the test running thread a daemon thread.  It blocks waiting for
    # output from the test.  By being a daemon this program can exit even
    # if the runner thread is deadlocked waiting for output from the test.
    self.setDaemon(True)
    self.command = command
    self.return_code = None
    self.has_started_event = threading.Event()
    self.finished_event = threading.Event()
    
  def run(self):
    logging.info('Running tests:')
    logging.info(' '.join(self.command))
    self.test_process = subprocess.Popen(args=self.command,
                                         stdout=subprocess.PIPE,
                                         stderr=subprocess.STDOUT,
                                         universal_newlines=True)

    self.has_started_event.set()
    logging.info('Output from running test follows:')

    while True:
      line = self.test_process.stdout.readline()
      if line:
        print line.strip('\n')
      else:
        break
      
    self.finished_event.set()
        
  
  def HasFinished(self):
    # The main thread must poll the test thread because it is possible
    # for test_process.readline() to hang. This can happen when a debug
    # window opens from a crashing program. This way, we can know when
    # our process has exited, and then try to kill all the windows.
    if not self.has_started_event.isSet():
      # The tests cannot have finished if they have not even started.
      return False
    
    self.test_process.poll()
    
    code = self.test_process.returncode
    
    if code is not None and util.IsWindows():
      # Wait for the test runner to exit safely, if it is able.
      time.sleep(5)
      
      if not self.finished_event.isSet():
        # Something is preventing proper exiting of test runner.
        # Try to kill all the debug windows.
        logging.info('Trying to clean up after the test. Ignore errors here.')
        os.system('TASKKILL /F /IM dwwin.exe')
        os.system('TASKKILL /F /IM iedw.exe')
  
        # Windows Error Reporting (on Vista)
        os.system('TASKKILL /F /IM WerFault.exe')
        
        # Browsers.
        os.system('TASKKILL /F /IM chrome.exe')
        os.system('TASKKILL /F /IM iexplore.exe')
        os.system('TASKKILL /F /IM firefox.exe')
        
        # Wait and see if the test is allowed to finish now.
        time.sleep(5)
      
      if not self.finished_event.isSet():
        logging.error('Could not kill all the extra processes created by the' +
                      'test run.')
      else:
        logging.info('Test process exited succesfully.')
        
    return code is not None
  
  
  def GetReturnCode(self):
    """Returns the exit code from the test runner, or special code 128 if 
    test runner did not exit."""
    
    if not self.has_started_event.isSet():
      code = 128
    else:
      code = self.test_process.returncode
      if code is None:
        code = 128
    return code
    


def RunTest(browser):
  """Runs tests on |browser|.
  Args:
    browser: the browser to test.
  Returns:
    True on success.
  """
  # Run selenium test.
  os.chdir(const.AUTO_PATH)

  if util.IsWindows(): 
    if not run_util.EnsureWindowsScreenResolution(SCREEN_WIDTH, SCREEN_HEIGHT, 
                                                  SCREEN_BPP):
      logging.error('Failed to configure screen resolution.')
      return 1

  
    
  # Clear all screenshots.
  logging.info('** Deleting previous screenshots.')
  if os.path.exists(SCREENSHOTS_PATH):
    shutil.rmtree(SCREENSHOTS_PATH)
    
  os.makedirs(SCREENSHOTS_PATH)  
  
  logging.info('** Running selenium tests...')

  # -u for unbuffered output.
  # Use Python2.4 for two reasons.  First, this is more or less the standard.
  # Second, if we use Python2.6 or later, we must manually shutdown the
  # httpserver, or the next run will overlap ports, which causes
  # some strange problems/exceptions.
  args = [const.PYTHON, '-u', SELENIUM_TEST_RUNNER_PATH]

  browser_parts = browser.split(' ', 1)
  args.append('--browser=' + browser_parts[0])
  if len(browser_parts) > 1:
    args.append('--browserpath=' + browser_parts[1])
  
  args.append('--servertimeout=80')
  args.append('--product_dir=' + const.PRODUCT_DIR_PATH)
  args.append('--verbose=0')
  args.append('--screenshots')
  args.append('--screencompare=' + IMAGE_DIFF_PATH)
  args.append('--screenshotsdir=' + SCREENSHOTS_PATH)
  args.append('--referencedir=' + O3D_REFERENCE_IMAGES_PATH)
  args.append('--selenium_server=' + SELENIUM_JAR_PATH)
  args.append('--testprefix=Test')
  args.append('--testsuffixes=small,medium,large')

  runner = TestRunningThread(args)
  runner.start()

  timeout_time = time.time() + TEST_TIMEOUT_SECS
  while not runner.HasFinished():
    if time.time() > timeout_time:
      break
    time.sleep(5)
  
  return runner.GetReturnCode()
    

def main(argv):
  logfile = os.path.join(os.path.dirname(os.path.abspath(__file__)), 
                         'log_run_lab_test.txt')
  util.ConfigureLogging(logfile)
  logging.info('Running on machine: ' + util.IpAddress())
  
  if len(argv) > 2:
    logging.error('Usage: run_lab_test.py [test_config_file]')
    return 1
  
  if len(argv) == 2:
    # Use given config file.
    config_path = argv[1]
  else:
    # Use default config file.
    config_path = os.path.join(const.HOME_PATH, 'test_config.txt')

  # Uninstall/Install plugin.
  if not run_util.UninstallO3DPlugin():
    logging.error('Could not successfully uninstall O3D. Tests will not be run.')
    return 1
  if not run_util.InstallO3DPlugin():
    logging.error('Unable to install O3D plugin. Tests will not be run.')
    return 1
    
  # Grab test configuration info from config file.
  if not os.path.exists(config_path):
    logging.error('Browser config file "%s" could not be found.' % config_path)
    return 1
  else:
    config_file = open(config_path, 'rU')
    test_browsers = []
    while True:
      browser = config_file.readline().strip()
      if len(browser) == 0:
        # No more lines in the file, go ahead and break.
        break
      test_browsers += [browser]

    config_file.close()

  if len(test_browsers) == 0:
    logging.warn('No browsers found in config file. No tests will run.')

  # Test browsers.
  all_test_passed = True
  for browser in test_browsers:
    # Get the Selenium name of the browser. The config file can contain
    # a particular browser name like "ie6", a selenium name like "*firefox",
    # or a selenium name like "*googlechrome C:/Chrome/chrome.exe".
    sel_name = browser
    for prefix in SELENIUM_BROWSER_PREFIXES.keys():
      if browser.startswith(prefix):
        sel_name = SELENIUM_BROWSER_PREFIXES[prefix]

    # Configure IE.
    if sel_name.startswith('*iexplore'):
      if not configure_ie.ConfigureIE():
        logging.error('Failed to configure IE.')
        all_test_passed = False
        continue

    # Run selenium tests.
    if RunTest(sel_name) != 0:
      all_test_passed = False
    
  if all_test_passed:
    logging.info('All tests passed.')
    return 0
  else:
    logging.info('Tests failed.')
    return 1

if __name__ == '__main__':
  code = main(sys.argv)
  sys.exit(code)    
