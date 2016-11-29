# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import shlex
import sys
import time
import traceback

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir,
  os.pardir, 'third_party', 'webdriver', 'pylib'))
from selenium import webdriver
from selenium.webdriver.chrome.options import Options

# TODO(robertogden) add logging


def ParseFlags():
  """
  Parses the given command line arguments and returns an object with the flags
  as properties.
  """
  parser = argparse.ArgumentParser()
  parser.add_argument('--browser_args', nargs=1, type=str, help='Override '
    'browser flags in code with these flags')
  parser.add_argument('--via_header_matches', metavar='via_header', nargs=1,
    default='1.1 Chrome-Compression-Proxy', help='What the via should match to '
    'be considered valid')
  parser.add_argument('--chrome_exec', nargs=1, type=str, help='The path to '
    'the Chrome or Chromium executable')
  parser.add_argument('chrome_driver', nargs=1, type=str, help='The path to '
    'the ChromeDriver executable')
  # TODO(robertogden) make this a logging statement
  print 'DEBUG: Args=', json.dumps(vars(parser.parse_args(sys.argv[1:])))
  return parser.parse_args(sys.argv[1:])

def HandleException(test_name=None):
  """
  Writes the exception being handled and a stack trace to stderr.
  """
  sys.stderr.write("**************************************\n")
  sys.stderr.write("**************************************\n")
  sys.stderr.write("**                                  **\n")
  sys.stderr.write("**       UNCAUGHT EXCEPTION         **\n")
  sys.stderr.write("**                                  **\n")
  sys.stderr.write("**************************************\n")
  sys.stderr.write("**************************************\n")
  if test_name:
    sys.stderr.write("Failed test: %s" % test_name)
  traceback.print_exception(*sys.exc_info())
  sys.exit(1)

class TestDriver:
  """
  This class is the tool that is used by every integration test to interact with
  the Chromium browser and validate proper functionality. This class sits on top
  of the Selenium Chrome Webdriver with added utility and helper functions for
  Chrome-Proxy. This class should be used with Python's 'with' operator.
  """

  def __init__(self):
    self._flags = ParseFlags()
    self._driver = None
    self.chrome_args = {}
    self._url = ''

  def __enter__(self):
    return self

  def __exit__(self, exc_type, exc_value, tb):
    if self._driver:
      self._StopDriver()

  def _OverrideChromeArgs(self):
    """
    Overrides any given flags in the code with those given on the command line.
    """
    if self._flags.browser_args and len(self._flags.browser_args) > 0:
      for a in shlex.split(self._flags.browser_args):
        self.chrome_args[a] = True

  def _StartDriver(self):
    """
    Parses the flags to pass to Chromium, then starts the ChromeDriver.
    """
    opts = Options()
    for a in self.chrome_args:
      opts.add_argument(a)
    caps = {'loggingPrefs': {'performance': 'INFO'}}
    if self._flags.chrome_exec:
      caps['chrome.binary'] = self._flags.chrome_exec
    driver = webdriver.Chrome(executable_path=self._flags.chrome_driver[0],
      chrome_options=opts, desired_capabilities=caps)
    driver.command_executor._commands.update({
      'getAvailableLogTypes': ('GET', '/session/$sessionId/log/types'),
      'getLog': ('POST', '/session/$sessionId/log')})
    self._driver = driver

  def _StopDriver(self):
    """
    Nicely stops the ChromeDriver.
    """
    self._driver.quit()
    del self._driver

  def AddChromeArgs(self, args):
    """
    Adds multiple arguments that will be passed to Chromium at start.
    """
    if not self.chrome_args:
      self.chrome_args = {}
    for a in args:
      self.chrome_args[a] = True

  def AddChromeArg(self, arg):
    """
    Adds a single argument that will be passed to Chromium at start.
    """
    if not self.chrome_args:
      self.chrome_args = {}
    self.chrome_args[arg] = True

  def RemoveChromeArgs(self, args):
    """
    Removes multiple arguments that will no longer be passed to Chromium at
    start.
    """
    if not self.chrome_args:
      self.chrome_args = {}
      return
    for a in args:
      del self.chrome_args[a]

  def RemoveChromeArg(self, arg):
    """
    Removes a single argument that will no longer be passed to Chromium at
    start.
    """
    if not self.chrome_args:
      self.chrome_args = {}
      return
    del self.chrome_args[arg]

  def ClearChromeArgs(self):
    """
    Removes all arguments from Chromium at start.
    """
    self.chrome_args = {}

  def ClearCache(self):
    """
    Clears the browser cache. Important note: ChromeDriver automatically starts
    a clean copy of Chrome on every instantiation.
    """
    self.ExecuteJavascript('if(window.chrome && chrome.benchmarking && '
      'chrome.benchmarking.clearCache){chrome.benchmarking.clearCache(); '
      'chrome.benchmarking.clearPredictorCache();chrome.benchmarking.'
      'clearHostResolverCache();}')

  # TODO(robertogden) use a smart page instead
  def SetURL(self, url):
    """
    Sets the URL that the browser will navigate to during the test.
    """
    self._url = url

  # TODO(robertogden) add timeout
  def LoadPage(self):
    """
    Starts Chromium with any arguments previously given and navigates to the
    previously given URL.
    """
    if not self._driver:
      self._StartDriver()
    self._driver.get(self._url)

  # TODO(robertogden) add timeout
  def ExecuteJavascript(self, script):
    """
    Executes the given javascript in the browser's current page as if it were on
    the console. Returns a string of whatever the evaluation was.
    """
    if not self._driver:
      self._StartDriver()
    return self._driver.execute_script("return " + script)


class IntegrationTest:
  """
  A parent class for all integration tests with utility and assertion methods.
  All methods starting with the word 'test' (ignoring case) will be called with
  the RunAllTests() method which can be used in place of a main method.
  """
  def RunAllTests(self):
    """
    Runs all methods starting with the word 'test' (ignoring case) in the class.
    Can be used in place of a main method to run all tests in a class.
    """
    methodList = [method for method in dir(self) if callable(getattr(self,
      method)) and method.lower().startswith('test')]
    for method in methodList:
      try:
        getattr(self, method)()
      except Exception as e:
        HandleException(method)

  # TODO(robertogden) add some nice assertion functions

  def Fail(self, msg):
    sys.stderr.write("**************************************\n")
    sys.stderr.write("**************************************\n")
    sys.stderr.write("**                                  **\n")
    sys.stderr.write("**          TEST FAILURE            **\n")
    sys.stderr.write("**                                  **\n")
    sys.stderr.write("**************************************\n")
    sys.stderr.write("**************************************\n")
    sys.stderr.write(msg, '\n')
    sys.exit(1)
