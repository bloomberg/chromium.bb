# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Classes for failures that occur during tests."""

import os
import test_expectations

def DetermineResultType(failure_list):
  """Takes a set of test_failures and returns which result type best fits
  the list of failures. "Best fits" means we use the worst type of failure.

  Returns:
    one of the test_expectations result types - PASS, TEXT, CRASH, etc."""

  if not failure_list or len(failure_list) == 0:
    return test_expectations.PASS

  failure_types = [type(f) for f in failure_list]
  if FailureCrash in failure_types:
    return test_expectations.CRASH
  elif FailureTimeout in failure_types:
    return test_expectations.TIMEOUT
  elif (FailureMissingResult in failure_types or
        FailureMissingImage in failure_types or
        FailureMissingImageHash in failure_types):
    return test_expectations.MISSING
  else:
    is_text_failure = FailureTextMismatch in failure_types
    is_image_failure = (FailureImageHashIncorrect in failure_types or
                        FailureImageHashMismatch in failure_types)
    if is_text_failure and is_image_failure:
      return test_expectations.IMAGE_PLUS_TEXT
    elif is_text_failure:
      return test_expectations.TEXT
    elif is_image_failure:
      return test_expectations.IMAGE
    else:
      raise ValueError("unclassifiable set of failures: " + str(failure_types))


class TestFailure(object):
  """Abstract base class that defines the failure interface."""
  @staticmethod
  def Message():
    """Returns a string describing the failure in more detail."""
    raise NotImplemented

  def ResultHtmlOutput(self, filename):
    """Returns an HTML string to be included on the results.html page."""
    raise NotImplemented

  def ShouldKillTestShell(self):
    """Returns True if we should kill the test shell before the next test."""
    return False

  def RelativeOutputFilename(self, filename, modifier):
    """Returns a relative filename inside the output dir that contains
    modifier.

    For example, if filename is fast\dom\foo.html and modifier is
    "-expected.txt", the return value is fast\dom\foo-expected.txt

    Args:
      filename: relative filename to test file
      modifier: a string to replace the extension of filename with

    Return:
      The relative windows path to the output filename
    """
    return os.path.splitext(filename)[0] + modifier

class FailureWithType(TestFailure):
  """Base class that produces standard HTML output based on the test type.

  Subclasses may commonly choose to override the ResultHtmlOutput, but still
  use the standard OutputLinks.
  """
  def __init__(self, test_type):
    TestFailure.__init__(self)
    # TODO(ojan): This class no longer needs to know the test_type.
    self._test_type = test_type

  # Filename suffixes used by ResultHtmlOutput.
  OUT_FILENAMES = []

  def OutputLinks(self, filename, out_names):
    """Returns a string holding all applicable output file links.

    Args:
      filename: the test filename, used to construct the result file names
      out_names: list of filename suffixes for the files. If three or more
          suffixes are in the list, they should be [actual, expected, diff,
          wdiff].
          Two suffixes should be [actual, expected], and a single item is the
          [actual] filename suffix.  If out_names is empty, returns the empty
          string.
    """
    links = ['']
    uris = [self.RelativeOutputFilename(filename, fn) for fn in out_names]
    if len(uris) > 1:
      links.append("<a href='%s'>expected</a>" % uris[1])
    if len(uris) > 0:
      links.append("<a href='%s'>actual</a>" % uris[0])
    if len(uris) > 2:
      links.append("<a href='%s'>diff</a>" % uris[2])
    if len(uris) > 3:
      links.append("<a href='%s'>wdiff</a>" % uris[3])
    return ' '.join(links)

  def ResultHtmlOutput(self, filename):
    return self.Message() + self.OutputLinks(filename, self.OUT_FILENAMES)


class FailureTimeout(TestFailure):
  """Test timed out.  We also want to restart the test shell if this
  happens."""
  @staticmethod
  def Message():
    return "Test timed out"

  def ResultHtmlOutput(self, filename):
    return "<strong>%s</strong>" % self.Message()

  def ShouldKillTestShell(self):
    return True


class FailureCrash(TestFailure):
  """Test shell crashed."""
  @staticmethod
  def Message():
    return "Test shell crashed"

  def ResultHtmlOutput(self, filename):
    # TODO(tc): create a link to the minidump file
    stack = self.RelativeOutputFilename(filename, "-stack.txt")
    return "<strong>%s</strong> <a href=%s>stack</a>" % (self.Message(), stack)

  def ShouldKillTestShell(self):
    return True


class FailureMissingResult(FailureWithType):
  """Expected result was missing."""
  OUT_FILENAMES = ["-actual.txt"]

  @staticmethod
  def Message():
    return "No expected results found"

  def ResultHtmlOutput(self, filename):
    return ("<strong>%s</strong>" % self.Message() +
            self.OutputLinks(filename, self.OUT_FILENAMES))


class FailureTextMismatch(FailureWithType):
  """Text diff output failed."""
  # Filename suffixes used by ResultHtmlOutput.
  OUT_FILENAMES = ["-actual.txt", "-expected.txt", "-diff.txt"]
  OUT_FILENAMES_WDIFF = ["-actual.txt", "-expected.txt", "-diff.txt",
                         "-wdiff.html"]

  def __init__(self, test_type, has_wdiff):
    FailureWithType.__init__(self, test_type)
    if has_wdiff:
      self.OUT_FILENAMES = self.OUT_FILENAMES_WDIFF

  @staticmethod
  def Message():
    return "Text diff mismatch"


class FailureMissingImageHash(FailureWithType):
  """Actual result hash was missing."""
  # Chrome doesn't know to display a .checksum file as text, so don't bother
  # putting in a link to the actual result.
  OUT_FILENAMES = []

  @staticmethod
  def Message():
    return "No expected image hash found"

  def ResultHtmlOutput(self, filename):
    return "<strong>%s</strong>" % self.Message()


class FailureMissingImage(FailureWithType):
  """Actual result image was missing."""
  OUT_FILENAMES = ["-actual.png"]

  @staticmethod
  def Message():
    return "No expected image found"

  def ResultHtmlOutput(self, filename):
    return ("<strong>%s</strong>" % self.Message() +
            self.OutputLinks(filename, self.OUT_FILENAMES))


class FailureImageHashMismatch(FailureWithType):
  """Image hashes didn't match."""
  OUT_FILENAMES = ["-actual.png", "-expected.png", "-diff.png"]

  @staticmethod
  def Message():
    # We call this a simple image mismatch to avoid confusion, since we link
    # to the PNGs rather than the checksums.
    return "Image mismatch"

class FailureFuzzyFailure(FailureWithType):
  """Image hashes didn't match."""
  OUT_FILENAMES = ["-actual.png", "-expected.png"]

  @staticmethod
  def Message():
    return "Fuzzy image match also failed"

class FailureImageHashIncorrect(FailureWithType):
  """Actual result hash is incorrect."""
  # Chrome doesn't know to display a .checksum file as text, so don't bother
  # putting in a link to the actual result.
  OUT_FILENAMES = []

  @staticmethod
  def Message():
    return "Images match, expected image hash incorrect. "

  def ResultHtmlOutput(self, filename):
    return "<strong>%s</strong>" % self.Message()
