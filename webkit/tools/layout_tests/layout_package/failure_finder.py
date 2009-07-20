# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(gwilson): 1.  Change text differs to use external utils.
#                2.  Change text_expectations parsing to existing
#                    logic in layout_pagckage.test_expectations.
import google.path_utils
import difflib
import errno
import os
import platform_utils
import re
import subprocess
import sys
import urllib2
import zipfile

from failure import Failure

WEBKIT_TRAC_HOSTNAME = "trac.webkit.org"
WEBKIT_LAYOUT_TEST_BASE_URL = "http://svn.webkit.org/repository/webkit/trunk/"
WEBKIT_IMAGE_BASELINE_BASE_URL_WIN = (WEBKIT_LAYOUT_TEST_BASE_URL +
                                      "LayoutTests/platform/win/")
WEBKIT_IMAGE_BASELINE_BASE_URL_MAC = (WEBKIT_LAYOUT_TEST_BASE_URL +
                                      "LayoutTests/platform/mac/")

BUILDBOT_BASE = "http://build.chromium.org/buildbot/"
WEBKIT_BUILDER_BASE = BUILDBOT_BASE + "waterfall/builders/%s"
FYI_BUILDER_BASE = BUILDBOT_BASE + "waterfall.fyi/builders/%s"
RESULTS_URL_BASE = "/builds/%s/steps/webkit_tests/logs/stdio"
ARCHIVE_URL_BASE = "/builds/%s/steps/archive_webkit_tests_results/logs/stdio"
ZIP_FILE_URL_BASE = (BUILDBOT_BASE +
                     "layout_test_results/%s/%s/layout-test-results.zip")
CHROMIUM_SRC_HOME = "http://src.chromium.org/viewvc/chrome/trunk/src/webkit/"
LAYOUT_TEST_REPO_BASE_URL = CHROMIUM_SRC_HOME + "data/layout_tests/"

# TODO(gwilson): Put flaky test dashboard URL here when ready.
FLAKY_TEST_URL = ""
FLAKY_TEST_REGEX = "%s</a></td><td align=right>(\d+)</td>"

TEST_EXPECTATIONS_URL = (CHROMIUM_SRC_HOME +
                         "tools/layout_tests/test_expectations.txt")

# Failure types as found in builder stdio.
TEXT_DIFF_MISMATCH = "Text diff mismatch"
SIMPLIFIED_TEXT_DIFF_MISMATCH = "Simplified text diff mismatch"
IMAGE_MISMATCH = "Image mismatch"
TEST_TIMED_OUT = "Test timed out"
TEST_SHELL_CRASHED = "Test shell crashed"

CHROMIUM_WIN = "chromium-win"
CHROMIUM_MAC = "chromium-mac"
CHROMIUM_LINUX = "chromium-linux"

ARCHIVE_URL_REGEX = "last.*change: (\d+)"
BUILD_NAME_REGEX = "build name: ([^\s]*)"
CHROMIUM_FILE_AGE_REGEX = '<br />\s*Modified\s*<em>.*</em> \((.*)\) by'
TEST_PATH_REGEX = "[^\s]+?"
FAILED_REGEX = ("ERROR (" + TEST_PATH_REGEX + ") failed:\s*"
                "(" + TEXT_DIFF_MISMATCH + ")?\s*"
                "(" + SIMPLIFIED_TEXT_DIFF_MISMATCH +")?\s*"
                "(" + IMAGE_MISMATCH + ")?\s*"
                "(" + TEST_TIMED_OUT + ")?\s*"
                "(" + TEST_SHELL_CRASHED + ")?")
FAILED_UNEXPECTED_REGEX = "  [^\s]+(?: = .*?)?\n"
LAST_BUILD_REGEX = ("<h2>Recent Builds:</h2>"
                    "[\s\S]*?<a href=\"../builders/.*?/builds/(\d+)\">")
SUMMARY_REGEX = "-{78}(.*?)-{78}" # -{78} --> 78 dashes in a row.
SUMMARY_REGRESSIONS = "Regressions:.*?\n((?:  [^\s]+(?: = .*?)?\n)+)"
TEST_EXPECTATIONS_PLATFORM_REGEX = "((WONTFIX|BUG.*).* %s.* : %s = [^\n]*)"
TEST_EXPECTATIONS_NO_PLATFORM_REGEX = ("((WONTFIX|BUG.*).*"
                                       "(?!WIN)(?!LINUX)(?!MAC).* :"
                                       " %s = [^\n]*)")

WEBKIT_FILE_AGE_REGEX = ('<a class="file" title="View File" href="%s">.*?</a>.'
                         '*?<td class="age" .*?>\s*'
                         '<a class="timeline" href=".*?" title=".*?">(.*?)</a>')

UPSTREAM_IMAGE_FILE_ENDING = "-upstream.png"

def GetURLBase(use_fyi):
  if use_fyi:
    return FYI_BUILDER_BASE
  return WEBKIT_BUILDER_BASE

def GetResultsURL(build, platform, use_fyi = False):
  return (GetURLBase(use_fyi) + RESULTS_URL_BASE) % (platform, build)

def GetArchiveURL(build, platform, use_fyi = False):
  return (GetURLBase(use_fyi) + ARCHIVE_URL_BASE) % (platform, build)

def GetZipFileURL(build, platform):
  return ZIP_FILE_URL_BASE % (platform, build)

def GetBuilderURL(platform, use_fyi = False):
  return GetURLBase(use_fyi) % platform

# TODO(gwilson): Once the new flakiness dashboard is done, connect it here.
def GetFlakyTestURL(platform):
  return ""

# TODO(gwilson): can we refactor these into the resourcegatherer?
def IsLinuxPlatform(platform):
  return (platform and platform.find("Linux") > -1)

def IsMacPlatform(platform):
  return (platform and platform.find("Mac") > -1)

def CreateDirectory(dirname):
  """
  Method that creates the directory structure given.
  This will create directories recursively until the given dir exists.
  """
  dir = "./" + dirname
  if not os.path.isdir(dir):
    if not os.path.isdir(dir[0:dir.rfind("/")]):
      CreateDirectory(dir[0:dir.rfind("/")])
    os.mkdir("./" + dirname + "/")

def ExtractFirstValue(string, regex):
  m = re.search(regex, string)
  if m and m.group(1):
    return m.group(1)
  return None

def ExtractSingleRegexAtURL(url, regex):
  content = ScrapeURL(url)
  m = re.search(regex, content, re.DOTALL)
  if m and m.group(1):
    return m.group(1)
  return None

def ScrapeURL(url):
  return urllib2.urlopen(urllib2.Request(url)).read()

def GeneratePNGDiff(file1, file2, output_file):
  platform_util = platform_utils.PlatformUtility('')
  _compare_available = False;
  try:
    executable = platform_util.ImageCompareExecutablePath("Debug")
    cmd = [executable, '--diff', file1, file2, output_file]
    _compare_available = True;
  except Exception, e:
    print "No command line to compare %s and %s : %s" % (file1, file2, e)

  result = 1
  if _compare_available:
    try:
      result = subprocess.call(cmd);
    except OSError, e:
      if e.errno == errno.ENOENT or e.errno == errno.EACCES:
        _compare_available = False
        print "No possible comparison between %s and %s." % (file1, file2)
      else:
        raise e
  if not result:
    print "The given PNG images were the same!"
  return result and result != 1 and _compare_available

# TODO(gwilson): Change this to use the pretty print differs.
def GenerateTextDiff(file1, file2, output_file):
  # Open up expected and actual text files and use difflib to compare them.
  dataA = open(file1, 'r').read()
  dataB = open(file2, 'r').read()
  d = difflib.Differ()
  diffs = list(d.compare(dataA.split("\n"), dataB.split("\n")))
  output = open(output_file, 'w')
  output.write("\n".join(diffs))
  output.close()

class FailureFinder(object):

  def __init__(self,
               build,
               builder_name,
               exclude_known_failures,
               test_regex,
               output_dir,
               max_failures,
               verbose):
    self.build = build
    # TODO(gwilson): add full url-encoding for the platform.
    self.platform = builder_name.replace(" ", "%20")
    self.exclude_known_failures = exclude_known_failures
    self.test_regex = test_regex
    self.output_dir = output_dir
    self.max_failures = max_failures
    self.verbose = verbose
    self.fyi_builder = False
    self._flaky_test_cache = {}
    self._test_expectations_cache = None

  # TODO(gwilson): Change this to get the last build that finished
  # successfully.
  def GetLastBuild(self):
    """
    Returns the last build number for this platform.
    If use_fyi is true, this only looks at the fyi builder.
    """
    try:
      return ExtractSingleRegexAtURL(GetBuilderURL(self.platform,
                                                   self.fyi_builder),
                                     LAST_BUILD_REGEX)
    except urllib2.HTTPError:
      if not self.fyi_builder:
        self.fyi_builder = True
        return self.GetLastBuild()

  def GetFailures(self):
    if not self.build:
      self.build = self.GetLastBuild()
    if self.verbose:
      print "Using build number %s" % self.build

    self.failures = self._GetFailuresFromBuilder()
    if self.failures and self._DownloadResultResources():
      return self.failures
    return None

  def _GetFailuresFromBuilder(self):
    """
    Returns a list of failures for the given build and platform by scraping
    the buildbots and parsing their results.
    The list returned contains Failure class objects.
    """
    if self.verbose:
      print "Fetching failures from buildbot..."

    content = self._ScrapeBuilderOutput()
    matches = self._FindMatchesInBuilderOutput(content)

    if self.verbose:
      print "%s failures found." % len(matches)

    failures = []

    for match in matches:
      if (len(failures) < self.max_failures and
          (not self.test_regex or match[0].find(self.test_regex) > -1)):
        failure = self._CreateFailureFromMatch(match)
        if self.verbose:
          print failure.test_path
        failures.append(failure)

    return failures

  def _ScrapeBuilderOutput(self):
    # Scrape the failures from the buildbot for this revision.
    try:
      return ScrapeURL(GetResultsURL(self.build,
                                     self.platform,
                                     self.fyi_builder))
    except:
      # If we hit a problem, and we're not on the FYI builder, try it again
      # on the FYI builder.
      if not self.fyi_builder:
        if self.verbose:
          print "Could not find builder on waterfall, trying fyi waterfall..."
        self.fyi_builder = True
        return self._ScrapeBuilderOutput()
      print "Couldn't find that builder, or build did not compile."
      return None

  def _FindMatchesInBuilderOutput(self, output):
    matches = []
    matches = re.findall(FAILED_REGEX, output, re.MULTILINE)
    if self.exclude_known_failures:
      summary = re.search(SUMMARY_REGEX, output, re.DOTALL)
      regressions = []
      if summary:
        regressions = self._FindRegressionsInSummary(summary.group(1))
      matches = self._MatchRegressionsToFailures(regressions, matches)
    return matches

  def _CreateFailureFromMatch(self, match):
    failure = Failure()
    failure.text_diff_mismatch = match[1] != ''
    failure.simplified_text_diff_mismatch = match[2] != ''
    failure.image_mismatch = match[3] != ''
    failure.crashed = match[5] != ''
    failure.timeout = match[4] != ''
    failure.test_path = match[0]
    failure.platform = self.platform
    return failure

  def _FindRegressionsInSummary(self, summary):
    regressions = []
    if not summary or not len(summary):
      return regressions
    matches = re.findall(SUMMARY_REGRESSIONS, summary, re.DOTALL)
    for match in matches:
      lines = re.findall(FAILED_UNEXPECTED_REGEX, match, re.DOTALL)
      for line in lines:
        clipped = line.strip()
        if clipped.find("=") > -1:
          clipped = clipped[:clipped.find("=") - 1]
        regressions.append(clipped)
    return regressions

  def _MatchRegressionsToFailures(self, regressions, failures):
    matches = []
    for regression in regressions:
      for failure in failures:
        if failure[0].find(regression) > -1:
          matches.append(failure)
    return matches

  # TODO(gwilson): add support for multiple conflicting build numbers by
  # renaming the zip file and naming the directory appropriately.
  def _DownloadResultResources(self):
    """
    Finds and downloads/extracts all of the test results (pixel/text output)
    for all of the given failures.
    """
    content = ScrapeURL(GetArchiveURL(self.build,
                                      self.platform,
                                      self.fyi_builder))
    revision = ExtractFirstValue(content, ARCHIVE_URL_REGEX)
    build_name = ExtractFirstValue(content, BUILD_NAME_REGEX)

    target_zip = "%s/layout-test-results-%s.zip" % (self.output_dir,
                                                    self.build)
    zip_url = GetZipFileURL(revision, build_name)
    if self.verbose:
      print "Downloading zip file from %s to %s" % (zip_url, target_zip)
    filename = self._DownloadFile(zip_url, target_zip, "b")
    if not filename:
      print "Could not download zip file from %s.  Does it exist?" % zip_url
      return False

    if zipfile.is_zipfile(filename):
      zip = zipfile.ZipFile(filename)
      if self.verbose:
        print 'Extracting files...'
      directory = "%s/layout-test-results-%s" % (self.output_dir, self.build)
      CreateDirectory(directory)
      for failure in self.failures:
        if failure.text_diff_mismatch or failure.simplified_text_diff_mismatch:
          self._PopulateTextFailure(failure, directory, zip)
        if failure.image_mismatch:
          self._PopulateImageFailure(failure, directory, zip)
        failure.test_age = self._GetFileAge(failure.GetTestHome())
        failure.flakiness = self._GetFlakiness(failure.test_path, self.platform)
        failure.test_expectations_line = (
          self._GetTestExpectationsLine(failure.test_path, self.platform))
      zip.close()
      if self.verbose:
        print "Files extracted."
        print "Deleting zip file..."
      os.remove(filename)
      return True
    else:
      print "Downloaded file '%s' doesn't look like a zip file." % filename
      return False

  def _PopulateTextFailure(self, failure, directory, zip):
    baselines = self._GetBaseline(failure.GetExpectedTextFilename(),
                                  directory)
    failure.text_baseline_local = baselines[0]
    failure.text_baseline_url = baselines[1]
    failure.text_baseline_age = (
      self._GetFileAge(failure.GetTextBaselineTracHome()))
    failure.text_actual_local = "%s/%s" % (directory,
                                           failure.GetActualTextFilename())
    if self._ExtractFileFromZip(zip,
                                failure.GetTextResultLocationInZipFile(),
                                failure.text_actual_local):
      GenerateTextDiff(failure.text_baseline_local,
                       failure.text_actual_local,
                       directory + "/" + failure.GetTextDiffFilename())

  def _PopulateImageFailure(self, failure, directory, zip):
    baselines = self._GetBaseline(failure.GetExpectedImageFilename(),
                                   directory)
    failure.image_baseline_local = baselines[0]
    failure.image_baseline_url = baselines[1]
    if baselines[0] and baselines[1]:
      failure.image_baseline_age = (
        self._GetFileAge(failure.GetImageBaselineTracHome()))
      failure.image_actual_local = "%s/%s" % (directory,
                                   failure.GetActualImageFilename())
      self._ExtractFileFromZip(zip,
                               failure.GetImageResultLocationInZipFile(),
                               failure.image_actual_local)
      if not GeneratePNGDiff("./" + failure.image_baseline_local,
                             "./" + failure.image_actual_local,
                             "./%s/%s" %
                             (directory, failure.GetImageDiffFilename())):
        print "Could not generate PNG diff for %s" % failure.test_path
      if failure.IsImageBaselineInChromium():
        upstream_baselines = (
          self._GetUpstreamBaseline(failure.GetExpectedImageFilename(),
                                    directory))
        failure.image_baseline_upstream_local = upstream_baselines[0]
        failure.image_baseline_upstream_url = upstream_baselines[1]

  def _GetBaseline(self, filename, directory, upstream_only = False):
    """
    Search and download the baseline for the given test on the given platform.
    First, look in the chromium repo, in that platform, for the baseline.
    If it's not there, look in the win platform for the baseline.
    If it's not there, look in webkit dir.
    If it's not there, look in the webkit platform dir.
    If it's not there, look in the webkit mac dir.
    If it's not there, I don't know where it came from.
    """
    # TODO(gwilson): Add support for Webkit's OSX-specific dirs (tiger, etc.)
    # TODO(gwilson): Refactor this to use the same logic as other scripts.

    local_filename = "%s/%s" % (directory, filename)
    if upstream_only:
      last_index = local_filename.rfind(".")
      if last_index > -1:
        local_filename = (local_filename[0:last_index] +
                          UPSTREAM_IMAGE_FILE_ENDING)

    download_file_modifiers = ""
    if local_filename.endswith(".png"):
      download_file_modifiers = "b"  # binary file

    CreateDirectory(local_filename[0:local_filename.rfind("/")])

    webkit_mac_location = (
      self._MangleWebkitPixelTestLocation(WEBKIT_IMAGE_BASELINE_BASE_URL_MAC,
                                           filename))
    webkit_win_location = (
      self._MangleWebkitPixelTestLocation(WEBKIT_IMAGE_BASELINE_BASE_URL_WIN,
                                           filename))

    possible_files = []

    # TODO(gwilson): Move this into the Failure class.
    if not upstream_only:
      if IsMacPlatform(self.platform):
        possible_files.append(self._GetBaselineURL(filename, CHROMIUM_MAC))
      if IsLinuxPlatform(self.platform):
        possible_files.append(self._GetBaselineURL(filename, CHROMIUM_LINUX))
      possible_files.append(self._GetBaselineURL(filename, CHROMIUM_WIN))
    possible_files.append(WEBKIT_LAYOUT_TEST_BASE_URL + filename)
    if IsMacPlatform(self.platform):
      possible_files.append(webkit_mac_location)
      possible_files.append(webkit_win_location)
    else:
      possible_files.append(webkit_win_location)
      possible_files.append(webkit_mac_location)

    local_baseline = None
    url_of_baseline = None
    index = 0
    while local_baseline == None and index < len(possible_files):
      local_baseline = self._DownloadFile(possible_files[index],
                                           local_filename,
                                           download_file_modifiers,
                                           True)
      if local_baseline:
        url_of_baseline = possible_files[index]
      index += 1

    if not local_baseline:
      print "Could not find any baseline for %s" % filename
    if local_baseline and self.verbose:
      print "Found baseline: %s" % url_of_baseline

    return (local_baseline, url_of_baseline)

  # Like _GetBaseline, but only retrieves the baseline from upstream (skip
  # looking in chromium).
  def _GetUpstreamBaseline(self, filename, directory):
    return self._GetBaseline(filename, directory, upstream_only = True)

  def _MangleWebkitPixelTestLocation(self, webkit_base_url, filename):
    # Clip the /LayoutTests/ off the front of the filename.
    # TODO(gwilson): find a more elegant way of doing this.
    return webkit_base_url + filename[12:]

  # TODO(gwilson): move this into Failure class.
  def _GetBaselineURL(self, filename, target_platform):
    if filename.startswith("chrome") or filename.startswith("pending"):
      # No need to append the platform.  These are platform-agnostic.
      return LAYOUT_TEST_REPO_BASE_URL + filename
    # Prepend the platform to the url.
    return "%splatform/%s/%s" % (LAYOUT_TEST_REPO_BASE_URL,
                                 target_platform, filename)

  def _GetFileAge(self, url):
    try:
      if url.find(WEBKIT_TRAC_HOSTNAME) > -1:
        return ExtractSingleRegexAtURL(url[:url.rfind("/")],
                                       WEBKIT_FILE_AGE_REGEX %
                                       url[url.find("/browser"):])
      else:
        return ExtractSingleRegexAtURL(url + "?view=log",
                                       CHROMIUM_FILE_AGE_REGEX)
    except:
      print "Could not find age for %s. Does the file exist?" % url
      return None

  # Returns a flakiness on a scale of 1-50.
  # TODO(gwilson): modify this to also return which of the last 10 builds failed
  # for this test.
  def _GetFlakiness(self, test_path, target_platform):
    url = GetFlakyTestURL(target_platform)
    if url == "":
      return None

    if url in self._flaky_test_cache:
      content = self._flaky_test_cache[url]
    else :
      content = urllib2.urlopen(urllib2.Request(url)).read()
      self._flaky_test_cache[url] = content

    flakiness = ExtractFirstValue(content, FLAKY_TEST_REGEX % test_path)
    return flakiness

  def _GetTestExpectationsLine(self, test_path, target_platform):
    if not self._test_expectations_cache:
      try:
        self._test_expectations_cache = ScrapeURL(TEST_EXPECTATIONS_URL)
      except HTTPError:
        print ("Could not find test_expectations.txt at %s" %
               TEST_EXPECTATIONS_URL)

    if not self._test_expectations_cache:
      return None

    translated_platform = "WIN"
    if IsMacPlatform(target_platform):
      translated_platform = "MAC"
    if IsLinuxPlatform(target_platform):
      translated_platform = "LINUX"

    test_parent_dir = test_path[:test_path.rfind("/")]
    # TODO(gwilson): change this to look in (recursively?) higher paths.
    possible_lines = [
      TEST_EXPECTATIONS_PLATFORM_REGEX % (translated_platform, test_path),
      TEST_EXPECTATIONS_NO_PLATFORM_REGEX % (test_path),
      TEST_EXPECTATIONS_PLATFORM_REGEX % (translated_platform, test_parent_dir),
      TEST_EXPECTATIONS_NO_PLATFORM_REGEX % (test_parent_dir)
    ]

    line = None
    for regex in possible_lines:
      if not line:
        line = ExtractFirstValue(self._test_expectations_cache, regex)
    return line

  def _ExtractFileFromZip(self, zip, file_in_zip, file_to_create):
    modifiers = ""
    if file_to_create.endswith(".png"):
      modifiers = "b"
    try:
      CreateDirectory(file_to_create[0:file_to_create.rfind("/")])
      localFile = open(file_to_create, "w%s" % modifiers)
      localFile.write(zip.read(file_in_zip))
      localFile.close()
      return True
    except KeyError:
      print "File %s does not exist in zip file." % (file_in_zip)
    except AttributeError:
      print "File %s does not exist in zip file." % (file_in_zip)
      print "Is this zip file assembled correctly?"
    return False


  def _DownloadFile(self, url, local_filename = None, modifiers = "",
                    force = False):
    """
    Copy the contents of a file from a given URL
    to a local file.
    """
    try:
      if local_filename == None:
        local_filename = url.split('/')[-1]
      if os.path.isfile(local_filename) and not force:
        print "File at %s already exists." % local_filename
        return local_filename
      webFile = urllib2.urlopen(url)
      localFile = open(local_filename, ("w%s" % modifiers))
      localFile.write(webFile.read())
      webFile.close()
      localFile.close()
    except urllib2.HTTPError:
      return None
    except urllib2.URLError:
      print "The url %s is malformed." % url
      return None
    return localFile.name
