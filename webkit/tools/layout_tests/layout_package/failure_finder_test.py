#!/bin/env/python
# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from failure_finder import FailureFinder

TEST_BUILDER_OUTPUT = """090723 10:38:22 test_shell_thread.py:289
            ERROR chrome/fast/forms/textarea-metrics.html failed:
              Text diff mismatch
              Simplified text diff mismatch
            090723 10:38:21 test_shell_thread.py:289
            ERROR chrome/fast/dom/xss-DENIED-javascript-variations.html failed:
              Text diff mismatch
              Simplified text diff mismatch
            090723 10:37:58 test_shell_thread.py:289
            ERROR LayoutTests/plugins/bindings-test.html failed:
              Text diff mismatch
              Simplified text diff mismatch
------------------------------------------------------------------------------
Expected to crash, but passed (1):
  chrome/fast/forms/textarea-metrics.html

Regressions: Unexpected failures (2):
  chrome/fast/dom/xss-DENIED-javascript-variations.html = FAIL
  LayoutTests/plugins/bindings-test.html = FAIL
------------------------------------------------------------------------------
"""

WEBKIT_BUILDER_NUMBER = "9800"
WEBKIT_FAILURES = (
  ["LayoutTests/fast/backgrounds/animated-svg-as-mask.html",
   "LayoutTests/fast/backgrounds/background-clip-text.html",
   "LayoutTests/fast/backgrounds/mask-composite.html",
   "LayoutTests/fast/backgrounds/repeat/mask-negative-offset-repeat.html",
   "LayoutTests/fast/backgrounds/svg-as-background-3.html",
   "LayoutTests/fast/backgrounds/svg-as-background-6.html",
   "LayoutTests/fast/backgrounds/svg-as-mask.html",
   "LayoutTests/fast/block/float/013.html",
   "LayoutTests/fast/block/float/nested-clearance.html",
   "LayoutTests/fast/block/positioning/047.html"])

CHROMIUM_BASELINE = "chrome/fast/forms/basic-buttons.html"
EXPECTED_CHROMIUM_LOCAL_BASELINE = "./chrome/fast/forms/basic-buttons.html"
EXPECTED_CHROMIUM_URL_BASELINE = ("http://src.chromium.org/viewvc/chrome/"
                                  "trunk/src/webkit/data/layout_tests/chrome/"
                                  "fast/forms/basic-buttons.html")

WEBKIT_BASELINE = "LayoutTests/fast/forms/11423.html"
EXPECTED_WEBKIT_LOCAL_BASELINE = "./LayoutTests/fast/forms/11423.html"
EXPECTED_WEBKIT_URL_BASELINE = ("http://svn.webkit.org/repository/webkit/trunk/"
                                "LayoutTests/fast/forms/11423.html")

TEST_ZIP_FILE = ("http://build.chromium.org/buildbot/layout_test_results/"
                 "webkit-rel/21432/layout-test-results.zip")

EXPECTED_REVISION = "20861"
EXPECTED_BUILD_NAME = "webkit-rel"

class FailureFinderTest(object):

  def runTests(self):
    all_tests_passed = True

    tests = ["testWhitespaceInBuilderName",
             "testGetLastBuild",
             "testFindMatchesInBuilderOutput",
             "testScrapeBuilderOutput",
             "testGetChromiumBaseline",
             "testGetWebkitBaseline",
             "testZipDownload",
             "testTranslateBuildToZip",
             "testFull"]

    for test in tests:
      print test
      result = eval(test + "()")
      if result:
        print "PASS"
      else:
        all_tests_passed = False
        print "FAIL"
    return all_tests_passed

def _getBasicFailureFinder():
  return FailureFinder(None, "Webkit", False, "", ".", 10, False)

def _testLastBuild(failure_finder):
  try:
    last_build = failure_finder.GetLastBuild()
    # Verify that last_build is not empty and is a number.
    build = int(last_build)
    return (build > 0)
  except:
    return False

def testGetLastBuild():
  test = _getBasicFailureFinder()
  return _testLastBuild(test)

def testWhitespaceInBuilderName():
  test = _getBasicFailureFinder()
  test.SetPlatform("Webkit (webkit.org)")
  return _testLastBuild(test)

def testScrapeBuilderOutput():

  # Try on the default builder.
  test = _getBasicFailureFinder()
  test.build = "9800"
  output = test._ScrapeBuilderOutput()
  if not output:
    return False

  # Try on a crazy builder on the FYI waterfall.
  test = _getBasicFailureFinder()
  test.build = "1766"
  test.SetPlatform("Webkit Linux (webkit.org)")
  output = test._ScrapeBuilderOutput()
  if not output:
    return False

  return True

def testFindMatchesInBuilderOutput():
  test =  _getBasicFailureFinder()
  test.exclude_known_failures = True
  matches = test._FindMatchesInBuilderOutput(TEST_BUILDER_OUTPUT)
  # Verify that we found x matches.
  if len(matches) != 2:
    print "Did not find all unexpected failures."
    return False

  test.exclude_known_failures = False
  matches = test._FindMatchesInBuilderOutput(TEST_BUILDER_OUTPUT)
  if len(matches) != 3:
    print "Did not find all failures."
    return False
  return True

def _testBaseline(test_name, expected_local, expected_url):
  test = _getBasicFailureFinder()
  # Test baseline that is obviously in Chromium's tree.
  local, url = test._GetBaseline(test_name, ".", False)
  try:
    os.remove(local)
    if (local != expected_local or url != expected_url):
      return False
  except:
    return False
  return True

def testGetChromiumBaseline():
  return _testBaseline(CHROMIUM_BASELINE, EXPECTED_CHROMIUM_LOCAL_BASELINE,
                       EXPECTED_CHROMIUM_URL_BASELINE)

def testGetWebkitBaseline():
  return _testBaseline(WEBKIT_BASELINE, EXPECTED_WEBKIT_LOCAL_BASELINE,
                       EXPECTED_WEBKIT_URL_BASELINE)

# TODO(gwilson): implement support for using local log files instead of
# scraping the buildbots.
def testUseLocalOutput():
  return True

def testZipDownload():
  test = _getBasicFailureFinder()
  try:
    test._DownloadFile(TEST_ZIP_FILE, "test.zip", "b") # "b" -> binary
    os.remove("test.zip")
    return True
  except:
    return False

def testTranslateBuildToZip():
  test = _getBasicFailureFinder()
  test.build = WEBKIT_BUILDER_NUMBER
  revision, build_name = test._GetRevisionAndBuildFromArchiveStep()
  if revision != EXPECTED_REVISION or build_name != EXPECTED_BUILD_NAME:
    return False
  return True

def testFull():
  """ Verifies that the entire system works end-to-end. """
  test = _getBasicFailureFinder()
  test.build = WEBKIT_BUILDER_NUMBER
  test.dont_download = True  # Dry run only, no downloading needed.
  failures = test.GetFailures()
  # Verify that the max failures parameter works.
  if not failures or len(failures) > 10:
    "Got no failures or too many failures."
    return False

  # Verify the failures match the list of expected failures.
  for failure in failures:
    if not (failure.test_path in WEBKIT_FAILURES):
      print "Found a failure I did not expect to see."
      return False

  return True

if __name__ == "__main__":
  fft = FailureFinderTest()
  result = fft.runTests()
  if result:
    print "All tests passed."
  else:
    print "Not all tests passed."
