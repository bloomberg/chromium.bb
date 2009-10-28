# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A helper class for comparing the failures and crashes from a test run
against what we expected to happen (as specified in test_expectations.txt)."""

import errno
import os

import path_utils
import test_failures
import test_expectations


def PrintFilesFromSet(filenames, header_text, output, opt_expectations=None,
    opt_relativizeFilenames=True):
  """A helper method to print a list of files to output.

  Args:
  filenames: a list of absolute filenames
  header_text: a string to display before the list of filenames
  output: file descriptor to write the results to.
  opt_expectations: expectations that failed for this test
  """
  if not len(filenames):
    return

  filenames = list(filenames)
  filenames.sort()
  output.write("\n")
  output.write("%s (%d):\n" % (header_text, len(filenames)))
  output_string = "  %s"
  if opt_expectations:
    output_string += " = %s" % opt_expectations
  output_string += "\n"
  for filename in filenames:
    if opt_relativizeFilenames:
      filename = path_utils.RelativeTestFilename(filename)
    output.write(output_string % filename)

class CompareFailures:
  # A list of which TestFailure classes count as a failure vs a crash.
  FAILURE_TYPES = (test_failures.FailureTextMismatch,
                   test_failures.FailureImageHashMismatch)
  TEXT_FAILURE_TYPES = (test_failures.FailureTextMismatch,)
  IMAGE_FAILURE_TYPES = (test_failures.FailureImageHashMismatch,)
  CRASH_TYPES = (test_failures.FailureCrash,)
  HANG_TYPES = (test_failures.FailureTimeout,)
  MISSING_TYPES = (test_failures.FailureMissingResult,
                   test_failures.FailureMissingImageHash)


  def __init__(self, test_files, test_failures, expectations):
    """Calculate the regressions in this test run.

    Args:
      test_files is a set of the filenames of all the test cases we ran
      test_failures is a dictionary mapping the test filename to a list of
          TestFailure objects if the test failed
      expectations is a TestExpectations object representing the
          current test status
    """
    self._test_files = test_files
    self._test_failures = test_failures
    self._expectations = expectations
    self._CalculateRegressions()


  def PrintRegressions(self, output):
    """Write the regressions computed by _CalculateRegressions() to output. """

    # Print unexpected passes by category.
    passes = self._regressed_passes
    PrintFilesFromSet(passes & self._expectations.GetFixableFailures(),
                      "Expected to fail, but passed",
                      output)
    PrintFilesFromSet(passes & self._expectations.GetFixableTimeouts(),
                      "Expected to timeout, but passed",
                      output)
    PrintFilesFromSet(passes & self._expectations.GetFixableCrashes(),
                      "Expected to crash, but passed",
                      output)

    PrintFilesFromSet(passes & self._expectations.GetWontFixFailures(),
                      "Expected to fail (ignored), but passed",
                      output)
    PrintFilesFromSet(passes & self._expectations.GetWontFixTimeouts(),
                      "Expected to timeout (ignored), but passed",
                      output)
    # Crashes should never be deferred.
    PrintFilesFromSet(passes & self._expectations.GetDeferredFailures(),
                      "Expected to fail (deferred), but passed",
                      output)
    PrintFilesFromSet(passes & self._expectations.GetDeferredTimeouts(),
                      "Expected to timeout (deferred), but passed",
                      output)
    # Print real regressions.
    PrintFilesFromSet(self._regressed_text_failures,
                      "Regressions: Unexpected text failures",
                      output,
                      'TEXT')
    PrintFilesFromSet(self._regressed_image_failures,
                      "Regressions: Unexpected image failures",
                      output,
                      'IMAGE')
    PrintFilesFromSet(self._regressed_image_plus_text_failures,
                      "Regressions: Unexpected image + text failures",
                      output,
                      'IMAGE+TEXT')
    PrintFilesFromSet(self._regressed_hangs,
                      "Regressions: Unexpected timeouts",
                      output,
                      'TIMEOUT')
    PrintFilesFromSet(self._regressed_crashes,
                      "Regressions: Unexpected crashes",
                      output,
                      'CRASH')
    PrintFilesFromSet(self._missing,
                      "Missing expected results",
                      output)

  def _CalculateRegressions(self):
    """Calculate regressions from this run through the layout tests."""
    worklist = self._test_files.copy()

    passes = set()
    crashes = set()
    hangs = set()
    missing = set()
    image_failures = set()
    text_failures  = set()
    image_plus_text_failures = set()

    for test, failure_type_instances in self._test_failures.iteritems():
      # Although each test can have multiple test_failures, we only put them
      # into one list (either the crash list or the failure list).  We give
      # priority to a crash/timeout over others, and to missing results over
      # a text mismatch.
      failure_types = [type(f) for f in failure_type_instances]
      is_crash = [True for f in failure_types if f in self.CRASH_TYPES]
      is_hang = [True for f in failure_types if f in self.HANG_TYPES]
      is_missing = [True for f in failure_types if f in self.MISSING_TYPES]
      is_failure = [True for f in failure_types if f in self.FAILURE_TYPES]
      is_text_failure = [True for f in failure_types if f in
         self.TEXT_FAILURE_TYPES]
      is_image_failure = [True for f in failure_types if f in
         self.IMAGE_FAILURE_TYPES]

      expectations = self._expectations.GetExpectations(test)
      if is_crash:
        if not test_expectations.CRASH in expectations: crashes.add(test)
      elif is_hang:
        if not test_expectations.TIMEOUT in expectations: hangs.add(test)
      # Do not add to the missing list if a test is rebaselining and missing
      # expected files.
      elif (is_missing and
            not self._expectations.IsRebaselining(test) and
            not self._expectations.IsIgnored(test)):
        missing.add(test)
      elif is_image_failure and is_text_failure:
        if (not test_expectations.FAIL in expectations and
            not test_expectations.IMAGE_PLUS_TEXT in expectations):
            image_plus_text_failures.add(test)
      elif is_image_failure:
        if (not test_expectations.FAIL in expectations and
            not test_expectations.IMAGE in expectations):
            image_failures.add(test)
      elif is_text_failure:
        if (not test_expectations.FAIL in expectations and
            not test_expectations.TEXT in expectations):
            text_failures.add(test)
      elif is_failure:
        raise ValueError('unexpected failure type:' + f)
      worklist.remove(test)

    for test in worklist:
      # Check that all passing tests are expected to pass.
      expectations = self._expectations.GetExpectations(test)
      if not test_expectations.PASS in expectations: passes.add(test)

    self._regressed_passes = passes
    self._regressed_crashes = crashes
    self._regressed_hangs = hangs
    self._missing = missing
    self._regressed_image_failures = image_failures
    self._regressed_text_failures = text_failures
    self._regressed_image_plus_text_failures = image_plus_text_failures

  def GetRegressions(self):
    """Returns a set of regressions from the test expectations. This is
    used to determine which tests to list in results.html and the
    right script exit code for the build bots. The list does not
    include the unexpected passes."""
    return (self._regressed_text_failures | self._regressed_image_failures |
            self._regressed_image_plus_text_failures | self._regressed_hangs |
            self._regressed_crashes | self._missing)

