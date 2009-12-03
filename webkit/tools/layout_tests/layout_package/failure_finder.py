# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(gwilson): 1.  Change text differs to use external utils.
#                2.  Change text_expectations parsing to existing
#                    logic in layout_pagckage.test_expectations.

import difflib
import errno
import os
import path_utils
import platform_utils
import re
import shutil
import subprocess
import sys
import urllib2
import zipfile

from failure import Failure

WEBKIT_TRAC_HOSTNAME = "trac.webkit.org"
WEBKIT_LAYOUT_TEST_BASE_URL = "http://svn.webkit.org/repository/webkit/trunk/"
WEBKIT_PLATFORM_BASELINE_URL = (WEBKIT_LAYOUT_TEST_BASE_URL +
                                "LayoutTests/platform/%s/")

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
IMAGE_MISMATCH = "Image mismatch"
TEST_TIMED_OUT = "Test timed out"
TEST_SHELL_CRASHED = "Test shell crashed"

CHROMIUM_WIN = "chromium-win"
CHROMIUM_WIN_XP = "chromium-win-xp"
CHROMIUM_WIN_VISTA = "chromium-win-vista"
CHROMIUM_WIN_7 = "chromium-win-7"
CHROMIUM_MAC = "chromium-mac"
CHROMIUM_LINUX = "chromium-linux"
PLATFORM = "platform"
LAYOUTTESTS = "LayoutTests"

# These platform dirs must be in order of their precedence.
# TODO(gwilson): This is not the same fallback order as test_shell.  This list
# should be reversed, and we need to add detection for the type of OS that
# the given builder is running.
WEBKIT_MAC_PLATFORM_DIRS = ["mac-leopard", "mac-snowleopard", "mac"]
WEBKIT_WIN_PLATFORM_DIRS = ["win", "mac"]
CHROMIUM_MAC_PLATFORM_DIRS = [CHROMIUM_MAC]
CHROMIUM_WIN_PLATFORM_DIRS = [CHROMIUM_WIN_XP, CHROMIUM_WIN_VISTA,
                              CHROMIUM_WIN_7, CHROMIUM_WIN]
CHROMIUM_LINUX_PLATFORM_DIRS = [CHROMIUM_LINUX, CHROMIUM_WIN]

ARCHIVE_URL_REGEX = "last.*change: (\d+)"
BUILD_NAME_REGEX = "build name: ([^\s]*)"
CHROMIUM_FILE_AGE_REGEX = '<br />\s*Modified\s*<em>.*</em> \((.*)\) by'
TEST_PATH_REGEX = "[^\s]+?"
FAILED_REGEX = ("ERROR (" + TEST_PATH_REGEX + ") failed:\s*"
                "(" + TEXT_DIFF_MISMATCH + ")?\s*"
                "(" + IMAGE_MISMATCH + ")?\s*"
                "(" + TEST_TIMED_OUT + ")?\s*"
                "(" + TEST_SHELL_CRASHED + ")?")
FAILED_UNEXPECTED_REGEX = "  [^\s]+(?: = .*?)?\n"
LAST_BUILD_REGEX = ("<h2>Recent Builds:</h2>"
                    "[\s\S]*?<a href=\"../builders/.*?/builds/(\d+)\">")
# Sometimes the lines of hyphens gets interrupted with multiple processes
# outputting to stdio, so don't rely on them being contiguous.
SUMMARY_REGEX = "-{78}(.*?)-{78}" # -{78} --> 78 dashes in a row.
SUMMARY_REGRESSIONS = "Regressions:.*?\n((?:  [^\s]+(?: = .*?)?\n)+)"
TEST_EXPECTATIONS_PLATFORM_REGEX = "((WONTFIX |BUG.* )+.* %s.* : %s = [^\n]*)"
TEST_EXPECTATIONS_NO_PLATFORM_REGEX = ("((WONTFIX |BUG.* )+.*"
                                       "(?!WIN)(?!LINUX)(?!MAC).* :"
                                       " %s = [^\n]*)")

WEBKIT_FILE_AGE_REGEX = ('<a class="file" title="View File" href="%s">.*?</a>.'
                         '*?<td class="age" .*?>\s*'
                         '<a class="timeline" href=".*?" title=".*?">(.*?)</a>')

LOCAL_BASELINE_REGEXES = [
  ".*/third_party/Webkit/LayoutTests/platform/.*?(/.*)",
  ".*/third_party/Webkit/LayoutTests(/.*)",
  ".*/webkit/data/layout_tests/platform/.*?/LayoutTests(/.*)",
  ".*/webkit/data/layout_tests/platform/.*?(/.*)",
  ".*/webkit/data/layout_tests(/.*)",
  "LayoutTests(/.*)"
]

UPSTREAM_IMAGE_FILE_ENDING = "-upstream.png"

TEST_EXPECTATIONS_WONTFIX = "WONTFIX"

TEMP_ZIP_DIR = "temp-zip-dir"

TARGETS = ["Release", "Debug"]

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

def CreateDirectory(dir):
  """
  Method that creates the directory structure given.
  This will create directories recursively until the given dir exists.
  """
  if not os.path.exists(dir):
    os.makedirs(dir, 0777)

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

def GetImageDiffExecutable():
  for target in TARGETS:
    try:
      return path_utils.ImageDiffPath(target)
    except Exception, e:
      continue
      # This build target did not exist, try the next one.
  raise Exception("No image diff executable could be found.  You may need "
                  "to build the image diff project under at least one build "
                  "target to create image diffs.")

def GeneratePNGDiff(file1, file2, output_file):
  _compare_available = False;
  try:
    executable = GetImageDiffExecutable()
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
  return _compare_available

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

class BaselineCandidate(object):
  """Simple data object for holding the URL and local file path of a
  possible baseline.  The local file path is meant to refer to the locally-
  cached version of the file at the URL."""

  def __init__(self, local, url):
    self.local_file = local
    self.baseline_url = url

  def IsValid(self):
    return self.local_file != None and self.baseline_url != None

class FailureFinder(object):

  def __init__(self,
               build,
               builder_name,
               exclude_known_failures,
               test_regex,
               output_dir,
               max_failures,
               verbose,
               builder_output_log_file = None,
               archive_step_log_file = None,
               zip_file = None,
               test_expectations_file = None):
    self.build = build
    # TODO(gwilson): add full url-encoding for the platform.
    self.SetPlatform(builder_name)
    self.exclude_known_failures = exclude_known_failures
    self.exclude_wontfix = True
    self.test_regex = test_regex
    self.output_dir = output_dir
    self.max_failures = max_failures
    self.verbose = verbose
    self.fyi_builder = False
    self._flaky_test_cache = {}
    self._test_expectations_cache = None
    # If true, scraping will still happen but no files will be downloaded.
    self.dont_download = False
    # Local caches of log files.  If set, the finder will use these files
    # rather than scraping them from the buildbot.
    self.builder_output_log_file = builder_output_log_file
    self.archive_step_log_file = archive_step_log_file
    self.zip_file = zip_file
    self.test_expectations_file = test_expectations_file
    self.delete_zip_file = True
    # Determines if the script should scrape the baselines from webkit.org
    # and chromium.org, or if it should use local baselines in the current
    # checkout.
    self.use_local_baselines = False

  def SetPlatform(self, platform):
    self.platform = platform.replace(" ", "%20")

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

    if self.use_local_baselines:
      self._BuildBaselineIndexes()
    self.failures = self._GetFailuresFromBuilder()
    if (self.failures and
        (self._DownloadResultResources() or self.dont_download)):
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
    if not content:
      return None
    matches = self._FindMatchesInBuilderOutput(content)

    if self.verbose:
      print "%s failures found." % len(matches)

    failures = []
    matches.sort()
    for match in matches:
      if (len(failures) < self.max_failures and
          (not self.test_regex or match[0].find(self.test_regex) > -1)):
        failure = self._CreateFailureFromMatch(match)
        if self.verbose:
          print failure.test_path
        failures.append(failure)

    return failures

  def _ScrapeBuilderOutput(self):
    # If the build log file is specified, use that instead of scraping.
    if self.builder_output_log_file:
      log = open(self.builder_output_log_file, 'r')
      return "".join(log.readlines())

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
      print "I could not find that builder, or build did not compile."
      print "Check that the builder name matches exactly (case sensitive),"
      print "and wrap quotes around builder names that have spaces."
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
    failure.image_mismatch = match[2] != ''
    failure.crashed = match[4] != ''
    failure.timeout = match[3] != ''
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

    target_zip = "%s/layout-test-results-%s.zip" % (self.output_dir,
                                                    self.build)
    if self.zip_file:
      filename = self.zip_file
      self.delete_zip_file = False
    else:
      revision, build_name = self._GetRevisionAndBuildFromArchiveStep()
      zip_url = GetZipFileURL(revision, build_name)
      if self.verbose:
        print "Downloading zip file from %s to %s" % (zip_url, target_zip)
      filename = self._DownloadFile(zip_url, target_zip, "b")
      if not filename:
        if self.verbose:
          print "Could not download zip file from %s.  Does it exist?" % zip_url
        return False

    if zipfile.is_zipfile(filename):
      zip = zipfile.ZipFile(filename)
      if self.verbose:
        print 'Extracting files...'
      directory = "%s/layout-test-results-%s" % (self.output_dir, self.build)
      CreateDirectory(directory)
      self._UnzipZipfile(zip, TEMP_ZIP_DIR)

      for failure in self.failures:
        failure.test_expectations_line = (
          self._GetTestExpectationsLine(failure.test_path))
        if self.exclude_wontfix and failure.IsWontFix():
          self.failures.remove(failure)
          continue
        if failure.text_diff_mismatch:
          self._PopulateTextFailure(failure, directory, zip)
        if failure.image_mismatch:
          self._PopulateImageFailure(failure, directory, zip)
        if not self.use_local_baselines:
          failure.test_age = self._GetFileAge(failure.GetTestHome())
        failure.flakiness = self._GetFlakiness(failure.test_path, self.platform)
      zip.close()
      if self.verbose:
        print "Files extracted."
      if self.delete_zip_file:
        if self.verbose:
          print "Cleaning up zip file..."
        path_utils.RemoveDirectory(TEMP_ZIP_DIR)
        os.remove(filename)
      return True
    else:
      if self.verbose:
        print "Downloaded file '%s' doesn't look like a zip file." % filename
      return False

  def _UnzipZipfile(self, zip, base_dir):
    for i, name in enumerate(zip.namelist()):
      if not name.endswith('/'):
        extracted_file_path = os.path.join(base_dir, name)
        try:
          (path, filename) = os.path.split(extracted_file_path)
          os.makedirs(path, 0777)
        except:
          pass
        outfile = open(extracted_file_path, 'wb')
        outfile.write(zip.read(name))
        outfile.flush()
        outfile.close()
        os.chmod(extracted_file_path, 0777)

  def _GetRevisionAndBuildFromArchiveStep(self):
    if self.archive_step_log_file:
      log = open(self.archive_step_log_file, 'r')
      content = "".join(log.readlines())
    else:
      content = ScrapeURL(GetArchiveURL(self.build,
                                        self.platform,
                                        self.fyi_builder))
    revision = ExtractFirstValue(content, ARCHIVE_URL_REGEX)
    build_name = ExtractFirstValue(content, BUILD_NAME_REGEX)
    return (revision, build_name)

  def _PopulateTextFailure(self, failure, directory, zip):
    baseline = self._GetBaseline(failure.GetExpectedTextFilename(), directory)
    failure.text_baseline_local = baseline.local_file
    failure.text_baseline_url = baseline.baseline_url
    failure.text_baseline_age = (
      self._GetFileAge(failure.GetTextBaselineTracHome()))
    failure.text_actual_local = "%s/%s" % (directory,
                                           failure.GetActualTextFilename())
    if (baseline and baseline.IsValid() and not self.dont_download):
      self._CopyFileFromZipDir(failure.GetTextResultLocationInZipFile(),
                               failure.text_actual_local)
      GenerateTextDiff(failure.text_baseline_local,
                       failure.text_actual_local,
                       directory + "/" + failure.GetTextDiffFilename())

  def _PopulateImageFailure(self, failure, directory, zip):
    baseline = self._GetBaseline(failure.GetExpectedImageFilename(), directory)
    failure.image_baseline_local = baseline.local_file
    failure.image_baseline_url = baseline.baseline_url
    if baseline and baseline.IsValid():
      failure.image_baseline_age = (
        self._GetFileAge(failure.GetImageBaselineTracHome()))
      failure.image_actual_local = "%s/%s" % (directory,
                                   failure.GetActualImageFilename())
      self._CopyFileFromZipDir(failure.GetImageResultLocationInZipFile(),
                               failure.image_actual_local)
      if (not GeneratePNGDiff(failure.image_baseline_local,
                              failure.image_actual_local,
                             "%s/%s" %
                             (directory, failure.GetImageDiffFilename()))
          and self.verbose):
        print "Could not generate PNG diff for %s" % failure.test_path
      if failure.IsImageBaselineInChromium() or self.use_local_baselines:
        upstream_baseline = (
          self._GetUpstreamBaseline(failure.GetExpectedImageFilename(),
                                    directory))
        failure.image_baseline_upstream_local = upstream_baseline.local_file
        failure.image_baseline_upstream_url = upstream_baseline.baseline_url

  def _GetBaseline(self, filename, directory, upstream_only = False):
    """ Search and download the baseline for the given test (put it in the
    directory given.)"""

    local_filename = os.path.join(directory, filename)
    local_directory = local_filename[:local_filename.rfind("/")]
    if upstream_only:
      last_index = local_filename.rfind(".")
      if last_index > -1:
        local_filename = (local_filename[:last_index] +
                          UPSTREAM_IMAGE_FILE_ENDING)

    download_file_modifiers = ""
    if local_filename.endswith(".png"):
      download_file_modifiers = "b"  # binary file

    if not self.dont_download:
      CreateDirectory(local_directory)

    local_baseline = None
    url_of_baseline = None

    if self.use_local_baselines:
      test_path_key = self._NormalizeBaselineIdentifier(filename)
      dict = self.baseline_dict
      if upstream_only:
        dict = self.webkit_baseline_dict
      if test_path_key in dict:
        local_baseline = dict[test_path_key]
        url_of_baseline = local_baseline
        shutil.copy(local_baseline, local_directory)
      elif self.verbose:
        print "Baseline %s does not exist in the index." % test_path_key
    else:
      index = 0
      possible_files = self._GetPossibleFileList(filename, upstream_only)
      # Download the baselines from the webkit.org site.
      while local_baseline == None and index < len(possible_files):
        local_baseline = self._DownloadFile(possible_files[index],
                                            local_filename,
                                            download_file_modifiers,
                                            True)
        if local_baseline:
          url_of_baseline = possible_files[index]
        index += 1

    if not local_baseline:
      if self.verbose:
        print "Could not find any baseline for %s" % filename
    else:
      local_baseline = os.path.normpath(local_baseline)
    if local_baseline and self.verbose:
      print "Found baseline: %s" % url_of_baseline

    return BaselineCandidate(local_baseline, url_of_baseline)

  def _AddBaselinePaths(self, list, base_path, directories):
    for dir in directories:
      list.append(os.path.join(base_path, dir))

  # TODO(gwilson): Refactor this method to use
  # platform_utils_*.BaselineSearchPath instead of custom logic.
  def _BuildBaselineIndexes(self):
    """ Builds an index of all the known local baselines in both chromium and
    webkit.  Two baselines are created, a webkit-specific (no chromium baseline)
    dictionary and an overall (both) dictionary.  Each one has a structure like:
    "/fast/dom/one-expected.txt" -> "C:\\path\\to\\fast\\dom\\one-expected.txt"
    """
    if self.verbose:
      print "Building index of all local baselines..."

    self.baseline_dict = {}
    self.webkit_baseline_dict = {}

    base = os.path.abspath(os.path.curdir)
    webkit_base = path_utils.PathFromBase('third_party', 'Webkit',
                                          'LayoutTests')
    chromium_base = path_utils.PathFromBase('webkit', 'data', 'layout_tests')
    chromium_base_platform = os.path.join(chromium_base, PLATFORM)
    webkit_base_platform = os.path.join(webkit_base, PLATFORM)

    possible_chromium_files = []
    possible_webkit_files = []

    if IsMacPlatform(self.platform):
      self._AddBaselinePaths(possible_chromium_files, chromium_base_platform,
                             CHROMIUM_MAC_PLATFORM_DIRS)
      self._AddBaselinePaths(possible_chromium_files, webkit_base_platform,
                             WEBKIT_MAC_PLATFORM_DIRS)
      self._AddBaselinePaths(possible_webkit_files, webkit_base_platform,
                             WEBKIT_MAC_PLATFORM_DIRS)
    elif IsLinuxPlatform(self.platform):
      self._AddBaselinePaths(possible_chromium_files, chromium_base_platform,
                             CHROMIUM_LINUX_PLATFORM_DIRS)
    else:
      self._AddBaselinePaths(possible_chromium_files, chromium_base_platform,
                             CHROMIUM_WIN_PLATFORM_DIRS)

    if not IsMacPlatform(self.platform):
      self._AddBaselinePaths(possible_webkit_files, webkit_base_platform,
                             WEBKIT_WIN_PLATFORM_DIRS)

    possible_webkit_files.append(webkit_base)

    self._PopulateBaselineDict(possible_webkit_files, self.webkit_baseline_dict)
    self._PopulateBaselineDict(possible_chromium_files, self.baseline_dict)
    for key in self.webkit_baseline_dict.keys():
      if not key in self.baseline_dict:
        self.baseline_dict[key] = self.webkit_baseline_dict[key]

    return True

  def _PopulateBaselineDict(self, directories, dictionary):
    for dir in directories:
      os.path.walk(dir, self._VisitBaselineDir, dictionary)

  def _VisitBaselineDir(self, dict, dirname, names):
    """ Method intended to be called by os.path.walk to build up an index
    of where all the test baselines exist. """
    # Exclude .svn from the walk, since we don't care what is in these dirs.
    if '.svn' in names:
      names.remove('.svn')
    for name in names:
      if name.find("-expected.") > -1:
        test_path_key = os.path.join(dirname, name)
        # Fix path separators to match the separators used on the buildbots.
        test_path_key = test_path_key.replace("\\", "/")
        test_path_key = self._NormalizeBaselineIdentifier(test_path_key)
        if not test_path_key in dict:
          dict[test_path_key] = os.path.join(dirname, name)

  # TODO(gwilson): Simplify identifier creation to not rely so heavily on
  # directory and path names.
  def _NormalizeBaselineIdentifier(self, test_path):
    """ Given either a baseline path (i.e. /LayoutTests/platform/mac/...) or a
    test path (i.e. /LayoutTests/fast/dom/....) will normalize to a unique
    identifier. This is basically a hashing function for layout test paths."""

    for regex in LOCAL_BASELINE_REGEXES:
      value = ExtractFirstValue(test_path, regex)
      if value:
        return value
    return test_path

  def _AddBaselineURLs(self, list, base_url, platforms):
    # If the base URL doesn't contain any platform in its path, only add the
    # base URL to the list.  This happens with the chrome/ and pending/ dirs.
    if base_url.find("%s") == -1:
      list.append(base_url)
      return
    for platform in platforms:
      list.append(base_url % platform)

  # TODO(gwilson): Refactor this method to use
  # platform_utils_*.BaselineSearchPath instead of custom logic.  This may
  # require some kind of wrapper since this method looks for URLs instead of
  # local paths.
  def _GetPossibleFileList(self, filename, only_webkit):
    """ Returns a list of possible filename locations for the given file.
    Uses the platform of the class to determine the order.
    """

    possible_chromium_files = []
    possible_webkit_files = []

    reduced_filename = filename.replace("LayoutTests/", "")
    chromium_platform_url = LAYOUT_TEST_REPO_BASE_URL
    if not filename.startswith("chrome") and not filename.startswith("pending"):
      chromium_platform_url += "platform/%s/"
    chromium_platform_url += filename

    webkit_platform_url = WEBKIT_PLATFORM_BASELINE_URL + reduced_filename

    if IsMacPlatform(self.platform):
      self._AddBaselineURLs(possible_chromium_files, chromium_platform_url,
                            CHROMIUM_MAC_PLATFORM_DIRS)
      self._AddBaselineURLs(possible_webkit_files, webkit_platform_url,
                            WEBKIT_MAC_PLATFORM_DIRS)
    elif IsLinuxPlatform(self.platform):
      self._AddBaselineURLs(possible_chromium_files, chromium_platform_url,
                            CHROMIUM_LINUX_PLATFORM_DIRS)
    else:
      self._AddBaselineURLs(possible_chromium_files, chromium_platform_url,
                            CHROMIUM_WIN_PLATFORM_DIRS)

    if not IsMacPlatform(self.platform):
      self._AddBaselineURLs(possible_webkit_files, webkit_platform_url,
                            WEBKIT_WIN_PLATFORM_DIRS)
    possible_webkit_files.append(WEBKIT_LAYOUT_TEST_BASE_URL + filename)

    if only_webkit:
      return possible_webkit_files
    return possible_chromium_files + possible_webkit_files

  # Like _GetBaseline, but only retrieves the baseline from upstream (skip
  # looking in chromium).
  def _GetUpstreamBaseline(self, filename, directory):
    return self._GetBaseline(filename, directory, upstream_only = True)

  def _GetFileAge(self, url):
    # Check if the given URL is really a local file path.
    if not url or not url.startswith("http"):
      return None
    try:
      if url.find(WEBKIT_TRAC_HOSTNAME) > -1:
        return ExtractSingleRegexAtURL(url[:url.rfind("/")],
                                       WEBKIT_FILE_AGE_REGEX %
                                       url[url.find("/browser"):])
      else:
        return ExtractSingleRegexAtURL(url + "?view=log",
                                       CHROMIUM_FILE_AGE_REGEX)
    except:
      if self.verbose:
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

  def _GetTestExpectations(self):
    if not self._test_expectations_cache:
      try:
        if self.test_expectations_file:
          log = open(self.test_expectations_file, 'r')
          self._test_expectations_cache = "\n".join(log.readlines())
        else:
          self._test_expectations_cache = ScrapeURL(TEST_EXPECTATIONS_URL)
      except HTTPError:
        print ("Could not find test_expectations.txt at %s" %
               TEST_EXPECTATIONS_URL)

    return self._test_expectations_cache

  def _GetTestExpectationsLine(self, test_path):
    content = self._GetTestExpectations()

    if not content:
      return None

    for match in content.splitlines():
      line = re.search(".*? : (.*?) = .*", match)
      if line and test_path.find(line.group(1)) > -1:
        return match

    return None

  def _CopyFileFromZipDir(self, file_in_zip, file_to_create):
    modifiers = ""
    if file_to_create.endswith(".png"):
      modifiers = "b"
    dir = os.path.join(os.path.split(file_to_create)[0:-1])[0]
    CreateDirectory(dir)
    file = os.path.normpath(os.path.join(TEMP_ZIP_DIR, file_in_zip))
    shutil.copy(file, dir)

  def _ExtractFileFromZip(self, zip, file_in_zip, file_to_create):
    modifiers = ""
    if file_to_create.endswith(".png"):
      modifiers = "b"
    try:
      CreateDirectory(file_to_create[0:file_to_create.rfind("/")])
      localFile = open(file_to_create, "w%s" % modifiers)
      localFile.write(zip.read(file_in_zip))
      localFile.close()
      os.chmod(file_to_create, 0777)
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
        if self.verbose:
          print "File at %s already exists." % local_filename
        return local_filename
      if self.dont_download:
        return local_filename
      webFile = urllib2.urlopen(url)
      localFile = open(local_filename, ("w%s" % modifiers))
      localFile.write(webFile.read())
      webFile.close()
      localFile.close()
      os.chmod(local_filename, 0777)
    except urllib2.HTTPError:
      return None
    except urllib2.URLError:
      print "The url %s is malformed." % url
      return None
    return localFile.name
