#!/usr/bin/python2.4
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


"""Selenium tests for the O3D plugin.

Sets up a local Selenium Remote Control server and a static file
server that serves files off the o3d directory.

Launches browsers to test the local build of the o3d plugin
and reports results back to the user.
"""

import os
import sys

script_dir = os.path.normpath(os.path.dirname(os.path.abspath(__file__)))
o3d_dir = os.path.dirname(os.path.dirname(script_dir))
src_dir = os.path.dirname(o3d_dir)
third_party_dir = os.path.join(src_dir, 'third_party')
o3d_third_party_dir = os.path.join(o3d_dir, 'third_party')
gflags_dir = os.path.join(o3d_third_party_dir, 'gflags', 'python')
selenium_dir = os.path.join(third_party_dir, 'selenium_rc', 'files')
selenium_py_dir = os.path.join(selenium_dir, 'selenium-python-client-driver')
sys.path.append(gflags_dir)
sys.path.append(selenium_py_dir)

import re
import SimpleHTTPServer
import socket
import SocketServer
import subprocess
import threading
import time
import unittest
import gflags
import javascript_unit_tests
import test_runner
import selenium
import samples_tests
import selenium_constants
import selenium_utilities
import pdiff_test
import Queue

if sys.platform == 'win32' or sys.platform == 'cygwin':
  default_java_exe = "java.exe"
else:
  default_java_exe = "java"

# Command line flags
FLAGS = gflags.FLAGS
gflags.DEFINE_boolean("verbose", False, "verbosity")
gflags.DEFINE_boolean("screenshots", False, "takes screenshots")
gflags.DEFINE_string(
    "java",
    default_java_exe,
    "specifies the path to the java executable.")
gflags.DEFINE_string(
    "selenium_server",
    os.path.join(selenium_dir, 'selenium-server', 'selenium-server.jar'),
    "specifies the path to the selenium server jar.")
gflags.DEFINE_string(
    "product_dir",
    None,
    "specifies the path to the build output directory.")
gflags.DEFINE_string(
    "screencompare",
    "",
    "specifies the directory in which perceptualdiff resides.\n"
    "compares screenshots with reference images")
gflags.DEFINE_string(
    "screenshotsdir",
    selenium_constants.DEFAULT_SCREENSHOT_PATH,
    "specifies the directory in which screenshots will be stored.")
gflags.DEFINE_string(
    "referencedir",
    selenium_constants.DEFAULT_SCREENSHOT_PATH,
    "Specifies the directory where reference images will be read from.")
gflags.DEFINE_string(
    "testprefix", "Test",
    "specifies the prefix of tests to run")
gflags.DEFINE_string(
    "testsuffixes",
    "small,medium,large",
    "specifies the suffixes, separated by commas of tests to run")
gflags.DEFINE_string(
    "servertimeout",
    "30",
    "Specifies the timeout value, in seconds, for the selenium server.")


# Browsers to choose from (for browser flag).
# use --browser $BROWSER_NAME to run
# tests for that browser
gflags.DEFINE_list(
    "browser",
    "*firefox",
    "\n".join(["comma-separated list of browsers to test",
               "Options:"] +
              selenium_constants.SELENIUM_BROWSER_SET))
gflags.DEFINE_string(
    "browserpath",
    "",
    "specifies the path to the browser executable "
    "(for platforms that don't support MOZ_PLUGIN_PATH)")
gflags.DEFINE_string(
    "samplespath",
    "",
    "specifies the path from the web root to the samples.")

class MyRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
  """Hook to handle HTTP server requests.

  Functions as a handler for logging and other utility functions.
  """

  def log_message(self, format, *args):
    """Logging hook for HTTP server."""

    # For now, just suppress logging.
    pass
    # TODO: might be nice to have a verbose option for debugging.

    


class LocalFileHTTPServer(threading.Thread):
  """Minimal HTTP server that serves local files.

  Members:
    http_alive: event to signal that http server is up and running
    http_port: the TCP port the server is using
  """

  START_PORT = 8100
  END_PORT = 8105

  def __init__(self, local_root=None):
    """Initializes the server.

    Initializes the HTTP server to serve static files from the
    local o3d directory

    Args:
      local_root: all files below this path are served.  If not specified,
        the current directory is the root.
    """
    threading.Thread.__init__(self)
    self._local_root = local_root
    self.http_alive = threading.Event()
    self.http_port = 0

  def run(self):
    """Runs the HTTP server.

    Server is started on an available port in the range of
    START_PORT to END_PORT
    """

    if self._local_root:
      os.chdir(self._local_root)

    for self.http_port in range(self.START_PORT, self.END_PORT):
      # Attempt to start the server
      try:
        httpd = SocketServer.TCPServer(("", self.http_port),
                                       MyRequestHandler)
      except socket.error:
        # Server didn't manage to start up, try another port.
        pass
      else:
        self.http_alive.set()
        httpd.serve_forever()

    if not self.http_alive.isSet():
      print("No available port found for HTTP server in the range %d to %d."
            % (self.START_PORT, self.END_PORT))
      self.http_port = 0

  @staticmethod
  def StartServer(local_root=None):
    """Create and start a LocalFileHTTPServer on a separate thread.

    Args:
      local_root: serve all static files below this directory.  If not
        specified, the current directory is the root.

    Returns:
      http_server: LocalFileHTTPServer() object
    """

    # Start up the Selenium Remote Control server
    http_server = LocalFileHTTPServer(local_root)
    http_server.setDaemon(True)
    http_server.start()

    time_out = 30.0

    # Wait till the Selenium RC Server is up
    print 'Waiting %d seconds for local HTTP server to start.' % (int(time_out))
    http_server.http_alive.wait(time_out)
    if not http_server.http_port:
      print 'Timed out.'
      return None

    print "LocalFileHTTPServer started on port %d" % http_server.http_port

    return http_server


class SeleniumRemoteControl(threading.Thread):
  """A thread that launches the Selenium Remote Control server.

  The Remote Control server allows us to launch a browser and remotely
  control it from a script.

  Members:
    selenium_alive: event to signal that selenium server is up and running
    selenium_port: the TCP port the server is using
    process: the subprocess.Popen instance for the server
  """

  START_PORT = 5430
  END_PORT = 5535

  def __init__(self, verbose, java_path, selenium_server, server_timeout):
    """Initializes the SeleniumRemoteControl class.

    Args:
      verbose: boolean verbose flag
      java_path: path to java used to run selenium.
      selenium_server: path to jar containing selenium server.
      server_timeout: server timeout value, in seconds.
    """
    self.selenium_alive = threading.Event()
    self.selenium_port = 0
    self.verbose = verbose
    self.java_path = java_path
    self.selenium_server = selenium_server
    self.timeout = server_timeout
    threading.Thread.__init__(self)

  def run(self):
    """Starts the selenium server.

    Server is started on an available port in the range of
    START_PORT to END_PORT
    """

    for self.selenium_port in range(self.START_PORT, self.END_PORT):
      # Attempt to start the selenium RC server from java
      self.process = subprocess.Popen(
          [self.java_path, "-jar", self.selenium_server, "-multiWindow",
           "-port", str(self.selenium_port), "-timeout", self.timeout],
          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

      for unused_i in range(1, 20):
        server_msg = self.process.stdout.readline()
        if self.verbose and server_msg is not None:
          # log if verbose flag is on
          print "sel_serv:" + server_msg

        # This status message indicates that the server has done
        # a bind to the port self.selenium_port successfully.
        if server_msg.find("INFO - Started SocketListener") != -1:
          self.selenium_alive.set()
          break

      # Error starting server on this port, try the next port.
      if not self.selenium_alive.isSet():
        continue

      # Loop and read from stdout
      while self.process.poll() is None:
        server_msg = self.process.stdout.readline()
        if self.verbose and server_msg is not None:
          # log if verbose flag is on
          print "sel_serv:" + server_msg

      # Finish.
      break

    if not self.selenium_alive.isSet():
      print("No available port found for Selenium RC Server "
            "in the range %d to %d."
            % (self.START_PORT, self.END_PORT))
      self.selenium_port = 0

  @staticmethod
  def StartServer(verbose, java_path, selenium_server, server_timeout):
    """Create and start the Selenium RC Server on a separate thread.

    Args:
      verbose: boolean verbose flag
      java_path: path to java used to run selenium.
      selenium_server: path to jar containing selenium server.
      server_timeout: server timeout value, in seconds

    Returns:
      selenium_server: SeleniumRemoteControl() object
    """

    # Start up the Selenium Remote Control server
    selenium_server = SeleniumRemoteControl(verbose,
                                            java_path,
                                            selenium_server,
                                            server_timeout)

    selenium_server.setDaemon(True)
    selenium_server.start()

    time_out = 30.0

    # Wait till the Selenium RC Server is up
    print 'Waiting %d seconds for Selenium RC to start.' % (int(time_out))
    selenium_server.selenium_alive.wait(time_out)
    if not selenium_server.selenium_port:
      print 'Timed out.'
      return None

    print("Selenium RC server started on port %d"
          % selenium_server.selenium_port)

    return selenium_server


class SeleniumSessionBuilder:
  def __init__(self, sel_port, sel_timeout, http_port, browserpath):

    self.sel_port = sel_port
    self.sel_timeout = sel_timeout
    self.http_port = http_port
    self.browserpath = browserpath

  def NewSeleniumSession(self, browser):
    server_url = "http://localhost:"
    server_url += str(self.http_port)

    browser_path_with_space = ""
    if self.browserpath:
      browser_path_with_space = " " + self.browserpath


    new_session = selenium.selenium("localhost",
                                    self.sel_port,
                                    browser + browser_path_with_space,
                                    server_url)
    
    new_session.start()
    new_session.set_timeout(self.sel_timeout)
    if browser == "*iexplore":
      # This improves stability on IE, especially IE 6. It at least fixes the
      # StressWindow test. It adds a 10ms delay between selenium commands.
      new_session.set_speed(10)
    
    return new_session


def TestBrowser(session_builder, browser, test_list, verbose):
  """Runs Selenium tests for a specific browser.

  Args:
    session_builder: session_builder for creating new selenium sessions.
    browser: selenium browser name (eg. *iexplore, *firefox).
    test_list: list of tests.
    
  Returns:
    summary_result: result of test runners.
  """
  print "Testing %s..." % browser
  
  summary_result = test_runner.TestResult(test_runner.StringBuffer(), browser,
                                          verbose)
  
  # Fill up the selenium test queue.
  test_queue = Queue.Queue()
  for test in test_list:
    test_queue.put(test)


  pdiff_queue = None
  if FLAGS.screenshots:
    # Need to do screen comparisons.
    # |pdiff_queue| is the queue of perceptual diff tests that need to be done.
    # This queue is added to by individual slenium test runners.
    # |pdiff_result_queue| is the result of the perceptual diff tests.
    pdiff_queue = Queue.Queue()
    pdiff_result_queue = Queue.Queue()
    pdiff_worker = test_runner.PDiffTestRunner(pdiff_queue,
                                               pdiff_result_queue,
                                               browser, verbose)
    pdiff_worker.start()
    
  # Start initial selenium test runner.
  worker = test_runner.SeleniumTestRunner(session_builder, browser,
                                          test_queue, pdiff_queue,
                                          verbose)
  worker.start()

  # Run through all selenium tests.
  while not worker.IsCompletelyDone():
    if worker.IsTesting() and worker.IsPastDeadline():
      # Test has taken more than allotted. Abort and go to next test.
      worker.AbortTest()

    elif worker.DidFinishTest():
      # Do this so that a worker does not grab test off queue till we tell it.     
      result = worker.Continue()
      result.printAll(sys.stdout)
      summary_result.merge(result)
    
    # Sleep here for a brief time.  This thread is polling the worker thread.
    # We cannot wait for a message from the worker thread because the worker
    # may hang on a bad test. We also do not want to sleep till the test's
    # deadline because the test may finish before then.
    time.sleep(.1)

  if FLAGS.screenshots:
    # Finish screenshot comparisons.
    pdiff_worker.EndTesting()
    while not pdiff_worker.IsCompletelyDone():
      time.sleep(1)

    # Be careful here, make sure no one else is editing |pdiff_reult_queue|.
    while not pdiff_result_queue.empty():
      result = pdiff_result_queue.get()
      result.printAll(sys.stdout)
      summary_result.merge(result)
      
  return summary_result



def MatchesSuffix(name, suffixes):
  """Checks if a name ends in one of the suffixes.

  Args:
    name: Name to test.
    suffixes: list of suffixes to test for.
  Returns:
    True if name ends in one of the suffixes or if suffixes is empty.
  """
  if suffixes:
    name_lower = name.lower()
    for suffix in suffixes:
      if name_lower.endswith(suffix):
        return True
    return False
  else:
    return True


def _GetTestsFromFile(filename, prefix, test_prefix_filter, test_suffixes,
                      browser, module, path_to_html):
  """Add tests defined in filename, and associated perceptual diff test, if
  needed.

  Assumes module has a method "GenericTest" that uses self.args to run.

  Args:
    filename: filename of file with list of tests.
    prefix: prefix to add to the beginning of each test.
    test_prefix_filter: Only adds a test if it starts with this.
    test_suffixes: list of suffixes to filter by. An empty list = pass all.
    browser: browser name.
    module: module which will have method GenericTest() called to run each test.   
    path_to_html: Path from server root to html
  """
  # See comments in that file for the expected format.
  # skip lines that are blank or have "#" or ";" as their first non whitespace
  # character.
  test_list_file = open(filename, "r")
  samples = test_list_file.readlines()
  test_list_file.close()

  tests = []

  for sample in samples:
    sample = sample.strip()
    if not sample or sample[0] == ";" or sample[0] == "#":
      continue

    arguments = sample.split()
    test_type = arguments[0].lower()
    test_path = arguments[1]
    options = arguments[2:]

    # TODO: Add filter based on test_type
    test_skipped = False
    if test_path.startswith("Test"):
      name = test_path
    else:
      # Need to make a name.
      name = ("Test" + prefix + re.sub("\W", "_", test_path) +
              test_type.capitalize())
      # Only test suffixes for generic tests. That is how it has always worked.
      if test_suffixes and not MatchesSuffix(name, test_suffixes):
        test_skipped = True
    
    if test_prefix_filter and not name.startswith(test_prefix_filter):
      test_skipped = True
      
    # Only execute this test if the current browser is not in the list
    # of skipped browsers.
    screenshot_count = 0
    for option in options:
      if option.startswith("except"):
        skipped_platforms = selenium_utilities.GetArgument(option)
        if not skipped_platforms is None:
          skipped_platforms = skipped_platforms.split(",")
          if browser in skipped_platforms:
            test_skipped = True
      elif option.startswith("screenshots"):
        screenshot_count += int(selenium_utilities.GetArgument(option))
      elif option.startswith("screenshot"):
        screenshot_count += 1

    if not test_skipped:
      # Add a test method with this name if it doesn't exist.
      if not (hasattr(module, name) and callable(getattr(module, name))):
        setattr(module, name, module.GenericTest)
             
      new_test = module(name, browser, path_to_html, test_type, test_path,
                        options)
      
      if screenshot_count and FLAGS.screenshots:
        pdiff_name = name + 'Screenshots'
        screenshot = selenium_utilities.ScreenshotNameFromTestName(test_path)
        setattr(pdiff_test.PDiffTest, pdiff_name,
                pdiff_test.PDiffTest.PDiffTest)
        new_pdiff = pdiff_test.PDiffTest(pdiff_name,
                                         screenshot_count,
                                         screenshot,
                                         FLAGS.screencompare,
                                         FLAGS.screenshotsdir,
                                         FLAGS.referencedir,
                                         options)
        tests += [(new_test, new_pdiff)]
      else:
        tests += [new_test]
        

  return tests


def GetTestsForBrowser(browser, test_prefix, test_suffixes):
  """Returns list of tests from test files.

  Args:
    browser: browser name
    test_prefix: prefix of tests to run.
    test_suffixes: A comma separated string of suffixes to filter by.
  Returns:
    A list of unittest.TestCase.
  """
  tests = []
  suffixes = test_suffixes.split(",")

  # add sample tests.
  filename = os.path.abspath(os.path.join(script_dir, "sample_list.txt"))
  tests += _GetTestsFromFile(filename, "Sample", test_prefix, suffixes, browser,
                             samples_tests.SampleTests,
                             FLAGS.samplespath.replace("\\","/"))
  
  # add javascript tests.
  filename = os.path.abspath(os.path.join(script_dir,
                                          "javascript_unit_test_list.txt"))
  tests += _GetTestsFromFile(filename, "UnitTest", test_prefix, suffixes,
                             browser, javascript_unit_tests.JavaScriptUnitTests,
                             "")

  return tests


def GetChromePath():
  value = None
  if sys.platform == "win32" or sys.platform == "cygwin":
    import _winreg
    try:
      key = _winreg.OpenKey(_winreg.HKEY_CLASSES_ROOT,
                            "Applications\\chrome.exe\\shell\\open\\command")
      (value, type) = _winreg.QueryValueEx(key, None)
      _winreg.CloseKey(key)
      value = os.path.dirname(value)
      
    except WindowsError:
      value = None
      if '*googlechrome' in FLAGS.browser:
        print "Unable to determine location for Chrome -- is it installed?"
    
  return value


def main(unused_argv):
  # Boolean to record if all tests passed.
  all_tests_passed = True

  selenium_constants.REFERENCE_SCREENSHOT_PATH = os.path.join(
    FLAGS.referencedir,
    "reference",
    "")
  selenium_constants.PLATFORM_SPECIFIC_REFERENCE_SCREENSHOT_PATH = os.path.join(
    FLAGS.referencedir,
    selenium_constants.PLATFORM_SCREENSHOT_DIR,
    "")
  
  # Launch HTTP server.
  http_server = LocalFileHTTPServer.StartServer(FLAGS.product_dir)

  if not http_server:
    print "Could not start a local http server with root." % FLAGS.product_dir
    return 1
  
 
  # Start Selenium Remote Control and Selenium Session Builder.
  sel_server_jar = os.path.abspath(FLAGS.selenium_server)
  sel_server = SeleniumRemoteControl.StartServer(
    FLAGS.verbose, FLAGS.java, sel_server_jar,
    FLAGS.servertimeout)

  if not sel_server:
    print "Could not start selenium server at %s." % sel_server_jar
    return 1

  session_builder = SeleniumSessionBuilder(
    sel_server.selenium_port,
    int(FLAGS.servertimeout) * 1000,
    http_server.http_port,
    FLAGS.browserpath)

  all_tests_passed = True
  # Test browsers.
  for browser in FLAGS.browser:
    if browser in set(selenium_constants.SELENIUM_BROWSER_SET):
      test_list = GetTestsForBrowser(browser, FLAGS.testprefix,
                                     FLAGS.testsuffixes)

      result = TestBrowser(session_builder, browser, test_list, FLAGS.verbose)

      if not result.wasSuccessful():
        all_tests_passed = False

      # Log non-succesful tests, for convenience.
      print ""
      print "Failures for %s:" % browser
      print "[Selenium tests]"
      for entry in test_list:
        if type(entry) == tuple:
          test = entry[0]
        else:
          test = entry

        if test in result.results:
          if result.results[test] != 'PASS':
            print test.name

      print ""
      print "[Perceptual Diff tests]"
      for entry in test_list:
        if type(entry) == tuple:
          pdiff_test = entry[1]
          if pdiff_test in result.results:
            if result.results[pdiff_test] != 'PASS':
              print pdiff_test.name
        

      # Log summary results.
      print ""
      print "Summary for %s:" % browser
      print "  %d tests run." % result.testsRun
      print "  %d errors." % len(result.errors)
      print "  %d failures.\n" % len(result.failures)

    else:
      print "ERROR: Browser %s is invalid." % browser
      print "Run with --help to view list of supported browsers.\n"
      all_tests_passed = False

  # Shut down remote control
  shutdown_session = selenium.selenium("localhost",
      sel_server.selenium_port, "*firefox",
      "http://%s:%d" % (socket.gethostname(), http_server.http_port))  
  shutdown_session.shut_down_selenium_server()

  if all_tests_passed:
    # All tests successful.
    return 0
  else:
    # Return error code 1.
    return 1

if __name__ == "__main__":
  remaining_argv = FLAGS(sys.argv)

  # Setup the environment for Firefox
  os.environ["MOZ_CRASHREPORTER_DISABLE"] = "1"
  os.environ["MOZ_PLUGIN_PATH"] = os.path.normpath(FLAGS.product_dir)

  # Setup the path for chrome.
  chrome_path = GetChromePath()
  if chrome_path:
    if os.environ.get("PATH"):
      os.environ["PATH"] = os.pathsep.join([os.environ["PATH"], chrome_path])
    else:
      os.environ["PATH"] =  chrome_path

  # Setup the LD_LIBRARY_PATH on Linux.
  if sys.platform[:5] == "linux":
    if os.environ.get("LD_LIBRARY_PATH"):
      os.environ["LD_LIBRARY_PATH"] = os.pathsep.join(
        [os.environ["LD_LIBRARY_PATH"], os.path.normpath(FLAGS.product_dir)])
    else:
      os.environ["LD_LIBRARY_PATH"] = os.path.normpath(FLAGS.product_dir)

  sys.exit(main(remaining_argv))
