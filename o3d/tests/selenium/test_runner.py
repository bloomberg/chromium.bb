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


"""Test runners and associated classes.

Each test runner has its own thread, which attempts to perform a given test.
If a test hangs, the test runner can be aborted or exited.

"""

import os
import sys

import socket
import subprocess
import threading
import time
import unittest
import gflags
import selenium
import selenium_constants
import Queue
import thread
import copy

class StringBuffer:
  """Primitive string buffer.

  Members:
    data: the contents of the buffer
  """
  def __init__(self):
    self.data = ""
  def write(self, data):
    self.data += str(data)
  def writeln(self, data=None):
    if data is not None:
      self.write(data)
    self.write("\n")
  def get(self):
    get_data = self.data
    self.data = ""
    return get_data

class TestResult(unittest.TestResult):
  """A specialized class that prints formatted text results to a stream.

  """
  separator1 = "=" * 30
  separator2 = "-" * 30

  def __init__(self, stream, browser, verbose):
    unittest.TestResult.__init__(self)
    self.stream = stream
    # Dictionary of start times
    self.start_times = {}
    # Dictionary of results
    self.results = {}
    self.browser = browser
    self.verbose = verbose

  def getDescription(self, test):
    """Gets description of test."""
    return test.shortDescription() or str(test)

  def startTest(self, test):
    """Starts test."""
    # Records the start time
    self.start_times[test] = time.time()
    # Default testresult if success not called
    self.results[test] = "FAIL"
    unittest.TestResult.startTest(self, test)
    if self.verbose:
      self.stream.writeln()
      self.stream.writeln(self.separator2)
      self.stream.write(self.getDescription(test))
      self.stream.writeln(" ... ")

  def stopTest(self, test):
    """Called when test is ended."""
    time_taken = time.time() - self.start_times[test]
    result = self.results[test]
    self.stream.writeln("SELENIUMRESULT %s <%s> [%.3fs]: %s"
                        % (test, self.browser, time_taken, result))
    self.printErrors()

  def addSuccess(self, test):
    """Adds success result to TestResult."""
    unittest.TestResult.addSuccess(self, test)
    self.results[test] = "PASS"

  def addError(self, test, err):
    """Adds error result to TestResult."""
    unittest.TestResult.addError(self, test, err)
    self.results[test] = "FAIL"

  def addFailure(self, test, err):
    """Adds failure result to TestResult."""
    unittest.TestResult.addFailure(self, test, err)
    self.results[test] = "FAIL"

  def noResponse(self, test):
    """Configures the result for a test that did not respond."""
    self.results[test] = "FAIL"
    self.testsRun += 1
    self.errors.append("No response from test")

    if self.verbose:
      self.stream.writeln()
      self.stream.writeln(self.separator2)
      self.stream.write(self.getDescription(test))
      self.stream.writeln(" ... ")
    self.stream.writeln("SELENIUMRESULT %s <%s> [0s]: FAIL"
                        % (test, self.browser))
    self.stream.writeln("Test was aborted due to timeout")

  def printErrors(self):
    """Prints all errors and failures."""
    if self.errors:
      self.printErrorList("ERROR", self.errors)
    if self.failures:
      self.printErrorList("FAIL", self.failures)

  def printErrorList(self, flavour, errors):
    """Prints a given list of errors."""
    for test, err in errors:
      self.stream.writeln("%s:" % flavour)
      self.stream.writeln("%s" % err)

  def printAll(self, stream):
    """Prints the entire stream to the given stream."""
    stream.write(self.stream.data)

  def merge(self, result):
    """Merges the given result into this resultl."""
    self.testsRun += result.testsRun
    for key, entry in result.results.iteritems():
      self.results[key] = entry
    for error in result.errors:
      self.errors.append(error)
    for failure in result.failures:
      self.failures.append(failure)
    self.stream.write(result.stream)


class TestRunnerThread(threading.Thread):
  """Abstract test runner class.  Launches its own thread for running tests.
  Formats test results.

  Members:
    completely_done_event: event that occurs just before thread exits.
    test: the currently running test.
    browser: selenium_name of browser that will be tested.
  """
  def __init__(self, verbose):
    threading.Thread.__init__(self)
    # This thread is a daemon so that the program can exit even if the
    # thread has not finished.
    self.setDaemon(True)
    self.completely_done_event = threading.Event()
    self.test = None
    self.browser = "default_browser"
    self.verbose = verbose

  def IsCompletelyDone(self):
    """Returns true if this test runner is completely done."""
    return self.completely_done_event.isSet()

  def run(self):
    pass
    
  def SetBrowser(self, browser):
    """Sets the browser name."""
    self.browser = browser

  def GetNoResponseResult(self):
    """Returns a generic no response result for last test."""
    result = TestResult(StringBuffer(), self.browser, self.verbose)
    result.noResponse(self.test)
    return result

  def RunTest(self, test):
    "Run the given test case or test suite."
    self.test = test

    stream = StringBuffer()
    result = TestResult(stream, self.browser, self.verbose)
    startTime = time.time()
    test(result)
    stopTime = time.time()
    timeTaken = stopTime - startTime
    if self.verbose:
      result.printErrors()
    run = result.testsRun
    return result
  

class PDiffTestRunner(TestRunnerThread):
  """Test runner for Perceptual Diff tests. Polls a test queue and launches
  given tests. Adds result to given queue.

  Members:
    pdiff_queue: list of tests to run, when they arrive.
    result_queue: queue of our tests results.
    browser: selenium name of browser to be tested.
    end_testing_event: event that occurs when we are guaranteed no more tests
      will be added to the queue.
  """
  def __init__(self, pdiff_queue, result_queue, browser, verbose):
    TestRunnerThread.__init__(self, verbose)
    self.pdiff_queue = pdiff_queue
    self.result_queue = result_queue
    self.browser = browser
    
    self.end_testing_event = threading.Event()

  def EndTesting(self):
    """Called to notify thread that no more tests will be added to the test
    queue."""
    self.end_testing_event.set()

  def run(self):
    while True:
      try:
        test = self.pdiff_queue.get_nowait()

        result = self.RunTest(test)
        
        self.result_queue.put(result)
        
      except Queue.Empty:
        if self.end_testing_event.isSet() and self.pdiff_queue.empty():
          break
        else:
          time.sleep(1)

    self.completely_done_event.set()
    

class SeleniumTestRunner(TestRunnerThread):
  """Test runner for Selenium tests. Takes a test from a test queue and launches
  it. Tries to handle hung/crashed tests gracefully.

  Members:
    testing_event: event that occurs when the runner is testing.
    finished_event: event that occurs when thread has finished testing and
      before it starts its next test.
    can_continue_lock: lock for |can_continue|.
    can_continue: is True when main thread permits the test runner to continue.
    sel_builder: builder that constructs new selenium sessions, as needed.
    browser: selenium name of browser to be tested.
    session: current selenium session being used in tests, can be None.
    test_queue: queue of tests to run.
    pdiff_queue: queue of perceptual diff tests to run. We add a perceptual
      diff test to the queue when the related selenium test passes.
    deadline: absolute time of when the test should be done.
  """
  def __init__(self, sel_builder, browser, test_queue, pdiff_queue, verbose):
    TestRunnerThread.__init__(self, verbose)

    # Synchronization.
    self.testing_event = threading.Event()
    self.finished_event = threading.Event()
    self.can_continue_lock = threading.Lock()
    self.can_continue = False
    
    # Selenium variables.
    self.sel_builder = sel_builder
    self.browser = browser

    # Test variables.
    self.test_queue = test_queue
    self.pdiff_queue = pdiff_queue
    
    self.deadline = 0
    
  def IsPastDeadline(self):
    if time.time() > self.deadline:
      return True
    return False

  def IsTesting(self):
    return self.testing_event.isSet()

  def DidFinishTest(self):
    return self.finished_event.isSet()
  
  def Continue(self):
    """Signals to thread to continue testing.

    Returns:
      result: the result for the recently finished test.
    """

    self.finished_event.clear()
    
    self.can_continue_lock.acquire()
    self.can_continue = True
    result = self.result
    self.can_continue_lock.release()
    
    return result
    
  def AbortTest(self):
    self._StopSession()
    self._StartSession()
    
  def _StartSession(self):
    self.session = self.sel_builder.NewSeleniumSession(self.browser)
    # Copy the session so we can shut down safely on a different thread.
    self.shutdown_session = copy.deepcopy(self.session)

  def _StopSession(self):
    if self.session is not None:
      self.session = None
      try:
        # This can cause an exception on some browsers.
        # Silenly disregard the exception.
        self.shutdown_session.stop()
      except:
        pass
  
  def run(self):
    self._StartSession()
    
    while not self.test_queue.empty():
      try:
        # Grab test from queue.
        test_obj = self.test_queue.get_nowait()
        if type(test_obj) == tuple:
          test = test_obj[0]
          pdiff_test = test_obj[1]
        else:
          test = test_obj
          pdiff_test = None

        self.can_continue = False
        
        # Check the current selenium session. Particularly, we are
        # interested if the previous test ran to completion, but the
        # browser window is closed.
        try:
          # This will generate an exception if the window is closed.
          self.session.window_focus()
        except Exception:
          self._StopSession()
          self._StartSession()

        # Deadline is the time to load page plus test run time.
        self.deadline = time.time() + (test.GetTestTimeout() / 1000.0)
        # Supply test with necessary selenium session.
        test.SetSession(self.session)

        # Run test.
        self.testing_event.set()
        self.result = self.RunTest(test)

        if time.time() > self.deadline:
          self.result = self.GetNoResponseResult()
        
        self.testing_event.clear()
        self.finished_event.set()

        # Wait for instruction from the main thread.
        while True:
          self.can_continue_lock.acquire()
          can_continue = self.can_continue
          self.can_continue_lock.release()
          if can_continue:
            break
          time.sleep(.5)        

        if self.pdiff_queue is not None and pdiff_test is not None:
          if self.result.wasSuccessful():
            # Add the dependent perceptual diff test.
            self.pdiff_queue.put(pdiff_test)       
        
      except Queue.Empty:
        break

    self._StopSession()      
    self.completely_done_event.set()
 

