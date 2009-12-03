# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A helper class for reading in and dealing with tests expectations
for layout tests.
"""

import logging
import os
import re
import sys
import time
import path_utils

sys.path.append(path_utils.PathFromBase('third_party'))
import simplejson

# Test expectation and modifier constants.
(PASS, FAIL, TEXT, IMAGE, IMAGE_PLUS_TEXT, TIMEOUT, CRASH, SKIP, WONTFIX,
 DEFER, SLOW, REBASELINE, MISSING, FLAKY, NOW, NONE) = range(16)

# Test expectation file update action constants
(NO_CHANGE, REMOVE_TEST, REMOVE_PLATFORM, ADD_PLATFORMS_EXCEPT_THIS) = range(4)

class TestExpectations:
  TEST_LIST = "test_expectations.txt"

  def __init__(self, tests, directory, platform, is_debug_mode, is_lint_mode):
    """Reads the test expectations files from the given directory."""
    path = os.path.join(directory, self.TEST_LIST)
    self._expected_failures = TestExpectationsFile(path, tests, platform,
        is_debug_mode, is_lint_mode)

  # TODO(ojan): Allow for removing skipped tests when getting the list of tests
  # to run, but not when getting metrics.
  # TODO(ojan): Replace the Get* calls here with the more sane API exposed
  # by TestExpectationsFile below. Maybe merge the two classes entirely?

  def GetExpectationsJsonForAllPlatforms(self):
    return self._expected_failures.GetExpectationsJsonForAllPlatforms()

  def GetRebaseliningFailures(self):
    return (self._expected_failures.GetTestSet(REBASELINE, FAIL) |
            self._expected_failures.GetTestSet(REBASELINE, IMAGE) |
            self._expected_failures.GetTestSet(REBASELINE, TEXT) |
            self._expected_failures.GetTestSet(REBASELINE, IMAGE_PLUS_TEXT))

  def GetExpectations(self, test):
    return self._expected_failures.GetExpectations(test)

  def GetExpectationsString(self, test):
    """Returns the expectatons for the given test as an uppercase string.
    If there are no expectations for the test, then "PASS" is returned."""
    expectations = self.GetExpectations(test)
    retval = []

    for expectation in expectations:
      for item in TestExpectationsFile.EXPECTATIONS.items():
        if item[1] == expectation:
          retval.append(item[0])
          break

    return " ".join(retval).upper()

  def GetTimelineForTest(self, test):
    return self._expected_failures.GetTimelineForTest(test)

  def GetTestsWithResultType(self, result_type):
    return self._expected_failures.GetTestsWithResultType(result_type)

  def GetTestsWithTimeline(self, timeline):
    return self._expected_failures.GetTestsWithTimeline(timeline)

  def MatchesAnExpectedResult(self, test, result):
    """Returns whether we got one of the expected results for this test."""
    return (result in self._expected_failures.GetExpectations(test) or
            (result in (IMAGE, TEXT, IMAGE_PLUS_TEXT) and
            FAIL in self._expected_failures.GetExpectations(test)) or
            result == MISSING and self.IsRebaselining(test) or
            result == SKIP and self._expected_failures.HasModifier(test, SKIP))

  def IsRebaselining(self, test):
    return self._expected_failures.HasModifier(test, REBASELINE)

  def HasModifier(self, test, modifier):
    return self._expected_failures.HasModifier(test, modifier)

  def RemovePlatformFromFile(self, tests, platform, backup=False):
    return self._expected_failures.RemovePlatformFromFile(tests,
                                                          platform,
                                                          backup)

def StripComments(line):
  """Strips comments from a line and return None if the line is empty
  or else the contents of line with leading and trailing spaces removed
  and all other whitespace collapsed"""

  commentIndex = line.find('//')
  if commentIndex is -1:
    commentIndex = len(line)

  line = re.sub(r'\s+', ' ', line[:commentIndex].strip())
  if line == '': return None
  else: return line

class ModifiersAndExpectations:
  """A holder for modifiers and expectations on a test that serializes to JSON.
  """
  def __init__(self, modifiers, expectations):
    self.modifiers = modifiers
    self.expectations = expectations

class ExpectationsJsonEncoder(simplejson.JSONEncoder):
  """JSON encoder that can handle ModifiersAndExpectations objects.
  """
  def default(self, obj):
    if isinstance(obj, ModifiersAndExpectations):
      return {"modifiers": obj.modifiers, "expectations": obj.expectations}
    else:
      return JSONEncoder.default(self, obj)

class TestExpectationsFile:
  """Test expectation files consist of lines with specifications of what
  to expect from layout test cases. The test cases can be directories
  in which case the expectations apply to all test cases in that
  directory and any subdirectory. The format of the file is along the
  lines of:

    LayoutTests/fast/js/fixme.js = FAIL
    LayoutTests/fast/js/flaky.js = FAIL PASS
    LayoutTests/fast/js/crash.js = CRASH TIMEOUT FAIL PASS
    ...

  To add other options:
    SKIP : LayoutTests/fast/js/no-good.js = TIMEOUT PASS
    DEBUG : LayoutTests/fast/js/no-good.js = TIMEOUT PASS
    DEBUG SKIP : LayoutTests/fast/js/no-good.js = TIMEOUT PASS
    LINUX DEBUG SKIP : LayoutTests/fast/js/no-good.js = TIMEOUT PASS
    DEFER LINUX WIN : LayoutTests/fast/js/no-good.js = TIMEOUT PASS

  SKIP: Doesn't run the test.
  SLOW: The test takes a long time to run, but does not timeout indefinitely.
  WONTFIX: For tests that we never intend to pass on a given platform.
  DEFER: Test does not count in our statistics for the current release.
  DEBUG: Expectations apply only to the debug build.
  RELEASE: Expectations apply only to release build.
  LINUX/WIN/WIN-XP/WIN-VISTA/WIN-7/MAC: Expectations apply only to these
      platforms.

  Notes:
    -A test cannot be both SLOW and TIMEOUT
    -A test cannot be both DEFER and WONTFIX
    -A test should only be one of IMAGE, TEXT, IMAGE+TEXT, or FAIL. FAIL is
     a migratory state that currently means either IMAGE, TEXT, or IMAGE+TEXT.
     Once we have finished migrating the expectations, we will change FAIL
     to have the meaning of IMAGE+TEXT and remove the IMAGE+TEXT identifier.
    -A test can be included twice, but not via the same path.
    -If a test is included twice, then the more precise path wins.
    -CRASH tests cannot be DEFER or WONTFIX
  """

  EXPECTATIONS = { 'pass': PASS,
                   'fail': FAIL,
                   'text': TEXT,
                   'image': IMAGE,
                   'image+text': IMAGE_PLUS_TEXT,
                   'timeout': TIMEOUT,
                   'crash': CRASH,
                   'missing': MISSING }

  EXPECTATION_DESCRIPTIONS = { SKIP : ('Skipped', 'Skipped'),
                               PASS : ('passes', 'passes'),
                               FAIL : ('failure', 'failures'),
                               TEXT : ('text diff mismatch',
                                       'text diff mismatch'),
                               IMAGE : ('image mismatch', 'image mismatch'),
                               IMAGE_PLUS_TEXT : ('image and text mismatch',
                                                  'image and text mismatch'),
                               CRASH : ('test shell crash',
                                        'test shell crashes'),
                               TIMEOUT : ('test timed out', 'tests timed out'),
                               MISSING : ('No expected result found',
                                          'No expected results found') }

  EXPECTATION_ORDER = ( PASS, CRASH, TIMEOUT, MISSING, IMAGE_PLUS_TEXT,
     TEXT, IMAGE, FAIL, SKIP )

  BASE_PLATFORMS = ( 'linux', 'mac', 'win' )
  PLATFORMS = BASE_PLATFORMS + ( 'win-xp', 'win-vista', 'win-7' )

  BUILD_TYPES = ( 'debug', 'release' )

  MODIFIERS = { 'skip': SKIP,
                'wontfix': WONTFIX,
                'defer': DEFER,
                'slow': SLOW,
                'rebaseline': REBASELINE,
                'none': NONE }

  TIMELINES = { 'wontfix': WONTFIX,
                'now': NOW,
                'defer': DEFER }

  RESULT_TYPES = { 'skip': SKIP,
                   'pass': PASS,
                   'fail': FAIL,
                   'flaky': FLAKY }


  def __init__(self, path, full_test_list, platform, is_debug_mode,
      is_lint_mode, expectations_as_str=None, suppress_errors=False):
    """
    path: The path to the expectation file. An error is thrown if a test is
        listed more than once.
    full_test_list: The list of all tests to be run pending processing of the
        expections for those tests.
    platform: Which platform from self.PLATFORMS to filter tests for.
    is_debug_mode: Whether we testing a test_shell built debug mode.
    is_lint_mode: Whether this is just linting test_expecatations.txt.
    expectations_as_str: Contents of the expectations file. Used instead of
        the path. This makes unittesting sane.
    suppress_errors: Whether to suppress lint errors.
    """

    self._path = path
    self._expectations_as_str = expectations_as_str
    self._is_lint_mode = is_lint_mode
    self._full_test_list = full_test_list
    self._suppress_errors = suppress_errors
    self._errors = []
    self._non_fatal_errors = []
    self._platform = self.ToTestPlatformName(platform)
    if self._platform is None:
      raise Exception("Unknown platform '%s'" % (platform))
    self._is_debug_mode = is_debug_mode

    # Maps relative test paths as listed in the expectations file to a list of
    # maps containing modifiers and expectations for each time the test is
    # listed in the expectations file.
    self._all_expectations = {}

    # Maps a test to its list of expectations.
    self._test_to_expectations = {}

    # Maps a test to its list of options (string values)
    self._test_to_options = {}

    # Maps a test to its list of modifiers: the constants associated with
    # the options minus any bug or platform strings
    self._test_to_modifiers = {}

    # Maps a test to the base path that it was listed with in the test list.
    self._test_list_paths = {}

    self._modifier_to_tests = self._DictOfSets(self.MODIFIERS)
    self._expectation_to_tests = self._DictOfSets(self.EXPECTATIONS)
    self._timeline_to_tests = self._DictOfSets(self.TIMELINES)
    self._result_type_to_tests = self._DictOfSets(self.RESULT_TYPES)

    self._Read(self._GetIterableExpectations())

  def _DictOfSets(self, strings_to_constants):
    """Takes a dict of strings->constants and returns a dict mapping
    each constant to an empty set."""
    d = {}
    for c in strings_to_constants.values():
      d[c] = set()
    return d

  def _GetIterableExpectations(self):
    """Returns an object that can be iterated over. Allows for not caring about
    whether we're iterating over a file or a new-line separated string.
    """
    if self._expectations_as_str:
      iterable = [x + "\n" for x in self._expectations_as_str.split("\n")]
      # Strip final entry if it's empty to avoid added in an extra newline.
      if iterable[len(iterable) - 1] == "\n":
        return iterable[:len(iterable) - 1]
      return iterable
    else:
      return open(self._path)

  def ToTestPlatformName(self, name):
    """Returns the test expectation platform that will be used for a
    given platform name, or None if there is no match."""
    chromium_prefix = 'chromium-'
    name = name.lower()
    if name.startswith(chromium_prefix):
      name = name[len(chromium_prefix):]
    if name in self.PLATFORMS:
      return name
    return None

  def GetTestSet(self, modifier, expectation=None, include_skips=True):
    if expectation is None:
      tests = self._modifier_to_tests[modifier]
    else:
      tests = (self._expectation_to_tests[expectation] &
          self._modifier_to_tests[modifier])

    if not include_skips:
      tests = tests - self.GetTestSet(SKIP, expectation)

    return tests

  def GetTestsWithResultType(self, result_type):
    return self._result_type_to_tests[result_type]

  def GetTestsWithTimeline(self, timeline):
    return self._timeline_to_tests[timeline]

  def HasModifier(self, test, modifier):
    return test in self._modifier_to_tests[modifier]

  def GetExpectations(self, test):
    return self._test_to_expectations[test]

  def GetExpectationsJsonForAllPlatforms(self):
    # Specify separators in order to get compact encoding.
    return ExpectationsJsonEncoder(separators=(',', ':')).encode(
        self._all_expectations)

  def Contains(self, test):
    return test in self._test_to_expectations

  def RemovePlatformFromFile(self, tests, platform, backup=False):
    """Remove the platform option from test expectations file.

    If a test is in the test list and has an option that matches the given
    platform, remove the matching platform and save the updated test back
    to the file. If no other platforms remaining after removal, delete the
    test from the file.

    Args:
      tests: list of tests that need to update..
      platform: which platform option to remove.
      backup: if true, the original test expectations file is saved as
              [self.TEST_LIST].orig.YYYYMMDDHHMMSS

    Returns:
      no
    """

    new_file = self._path + '.new'
    logging.debug('Original file: "%s"', self._path)
    logging.debug('New file: "%s"', new_file)
    f_orig = self._GetIterableExpectations()
    f_new = open(new_file, 'w')

    tests_removed = 0
    tests_updated = 0
    for line in f_orig:
      action = self._GetPlatformUpdateAction(line, tests, platform)
      if action == NO_CHANGE:
        # Save the original line back to the file
        logging.debug('No change to test: %s', line)
        f_new.write(line)
      elif action == REMOVE_TEST:
        tests_removed += 1
        logging.info('Test removed: %s', line)
      elif action == REMOVE_PLATFORM:
        parts = line.split(':')
        new_options = parts[0].replace(platform.upper() + ' ', '', 1)
        new_line = ('%s:%s' % (new_options, parts[1]))
        f_new.write(new_line)
        tests_updated += 1
        logging.info('Test updated: ')
        logging.info('  old: %s', line)
        logging.info('  new: %s', new_line)
      elif action == ADD_PLATFORMS_EXCEPT_THIS:
        parts = line.split(':')
        new_options = parts[0]
        for p in self.PLATFORMS:
          if not p == platform:
            new_options += p.upper() + ' '
        new_line = ('%s:%s' % (new_options, parts[1]))
        f_new.write(new_line)
        tests_updated += 1
        logging.info('Test updated: ')
        logging.info('  old: %s', line)
        logging.info('  new: %s', new_line)
      else:
        logging.error('Unknown update action: %d; line: %s', action, line)

    logging.info('Total tests removed: %d', tests_removed)
    logging.info('Total tests updated: %d', tests_updated)

    f_orig.close()
    f_new.close()

    if backup:
      date_suffix = time.strftime('%Y%m%d%H%M%S', time.localtime(time.time()))
      backup_file = ('%s.orig.%s' % (self._path, date_suffix))
      if os.path.exists(backup_file):
        os.remove(backup_file)
      logging.info('Saving original file to "%s"', backup_file)
      os.rename(self._path, backup_file)
    else:
      os.remove(self._path)

    logging.debug('Saving new file to "%s"', self._path)
    os.rename(new_file, self._path)
    return True

  def ParseExpectationsLine(self, line):
    """Parses a line from test_expectations.txt and returns a tuple with the
    test path, options as a list, expectations as a list."""
    line = StripComments(line)
    if not line:
      return (None, None, None)

    options = []
    if line.find(":") is -1:
      test_and_expectation = line.split("=")
    else:
      parts = line.split(":")
      options = self._GetOptionsList(parts[0])
      test_and_expectation = parts[1].split('=')

    test = test_and_expectation[0].strip()
    if (len(test_and_expectation) is not 2):
      self._AddError(lineno, "Missing expectations.", test_and_expectation)
      expectations = None
    else:
      expectations = self._GetOptionsList(test_and_expectation[1])

    return (test, options, expectations)

  def _GetPlatformUpdateAction(self, line, tests, platform):
    """Check the platform option and return the action needs to be taken.

    Args:
      line: current line in test expectations file.
      tests: list of tests that need to update..
      platform: which platform option to remove.

    Returns:
      NO_CHANGE: no change to the line (comments, test not in the list etc)
      REMOVE_TEST: remove the test from file.
      REMOVE_PLATFORM: remove this platform option from the test.
      ADD_PLATFORMS_EXCEPT_THIS: add all the platforms except this one.
    """
    test, options, expectations = self.ParseExpectationsLine(line)
    if not test or test not in tests:
      return NO_CHANGE

    has_any_platform = False
    for option in options:
      if option in self.PLATFORMS:
        has_any_platform = True
        if not option == platform:
          return REMOVE_PLATFORM

    # If there is no platform specified, then it means apply to all platforms.
    # Return the action to add all the platforms except this one.
    if not has_any_platform:
      return ADD_PLATFORMS_EXCEPT_THIS

    return REMOVE_TEST

  def _HasValidModifiersForCurrentPlatform(self, options, lineno,
      test_and_expectations, modifiers):
    """Returns true if the current platform is in the options list or if no
    platforms are listed and if there are no fatal errors in the options list.

    Args:
      options: List of lowercase options.
      lineno: The line in the file where the test is listed.
      test_and_expectations: The path and expectations for the test.
      modifiers: The set to populate with modifiers.
    """
    has_any_platform = False
    has_bug_id = False
    for option in options:
      if option in self.MODIFIERS:
        modifiers.add(option)
      elif option in self.PLATFORMS:
        has_any_platform = True
      elif option.startswith('bug'):
        has_bug_id = True
      elif option not in self.BUILD_TYPES:
        self._AddError(lineno, 'Invalid modifier for test: %s' % option,
            test_and_expectations)

    if has_any_platform and not self._MatchPlatform(options):
      return False

    if not has_bug_id and 'wontfix' not in options:
      # TODO(ojan): Turn this into an AddError call once all the tests have
      # BUG identifiers.
      self._LogNonFatalError(lineno, 'Test lacks BUG modifier.',
          test_and_expectations)

    if 'release' in options or 'debug' in options:
      if self._is_debug_mode and 'debug' not in options:
        return False
      if not self._is_debug_mode and 'release' not in options:
        return False

    if 'wontfix' in options and 'defer' in options:
      self._AddError(lineno, 'Test cannot be both DEFER and WONTFIX.',
          test_and_expectations)

    if self._is_lint_mode and 'rebaseline' in options:
      self._AddError(lineno, 'REBASELINE should only be used for running'
          'rebaseline.py. Cannot be checked in.', test_and_expectations)

    return True

  def _MatchPlatform(self, options):
    """Match the list of options against our specified platform. If any
    of the options prefix-match self._platform, return True. This handles
    the case where a test is marked WIN and the platform is WIN-VISTA.

    Args:
      options: list of options
    """
    for opt in options:
      if self._platform.startswith(opt):
        return True
    return False

  def _AddToAllExpectations(self, test, options, expectations):
    # Make all paths unix-style so the dashboard doesn't need to.
    test = test.replace('\\', '/')
    if not test in self._all_expectations:
      self._all_expectations[test] = []
    self._all_expectations[test].append(
        ModifiersAndExpectations(options, expectations))

  def _Read(self, expectations):
    """For each test in an expectations iterable, generate the expectations for
    it.
    """
    lineno = 0
    for line in expectations:
      lineno += 1

      test_list_path, options, expectations = self.ParseExpectationsLine(line)
      if not expectations:
        continue

      self._AddToAllExpectations(test_list_path, " ".join(options).upper(),
          " ".join(expectations).upper())

      modifiers = set()
      if options and not self._HasValidModifiersForCurrentPlatform(options,
          lineno, test_list_path, modifiers):
        continue

      expectations = self._ParseExpectations(expectations, lineno,
          test_list_path)

      if 'slow' in options and TIMEOUT in expectations:
        self._AddError(lineno, 'A test should not be both slow and timeout. '
            'If it times out indefinitely, then it should be just timeout.',
            test_list_path)

      full_path = os.path.join(path_utils.LayoutTestsDir(test_list_path),
                               test_list_path)
      full_path = os.path.normpath(full_path)
      # WebKit's way of skipping tests is to add a -disabled suffix.
      # So we should consider the path existing if the path or the -disabled
      # version exists.
      if not os.path.exists(full_path) and not \
        os.path.exists(full_path + '-disabled'):
        # Log a non fatal error here since you hit this case any time you
        # update test_expectations.txt without syncing the LayoutTests
        # directory
        self._LogNonFatalError(lineno, 'Path does not exist.', test_list_path)
        continue

      if not self._full_test_list:
        tests = [test_list_path]
      else:
        tests = self._ExpandTests(test_list_path)

      self._AddTests(tests, expectations, test_list_path, lineno,
                     modifiers, options)

    if not self._suppress_errors and (
        len(self._errors) or len(self._non_fatal_errors)):
      if self._is_debug_mode:
        build_type = 'DEBUG'
      else:
        build_type = 'RELEASE'
      print "\nFAILURES FOR PLATFORM: %s, BUILD_TYPE: %s" \
          % (self._platform.upper(), build_type)

      for error in self._non_fatal_errors:
        logging.error(error)
      if len(self._errors):
        raise SyntaxError('\n'.join(map(str, self._errors)))

    # Now add in the tests that weren't present in the expectations file
    expectations = set([PASS])
    options = []
    modifiers = []
    if self._full_test_list:
      for test in self._full_test_list:
        if not test in self._test_list_paths:
          self._AddTest(test, modifiers, expectations, options)

  def _GetOptionsList(self, listString):
    return [part.strip().lower() for part in listString.strip().split(' ')]

  def _ParseExpectations(self, expectations, lineno, test_list_path):
    result = set()
    for part in expectations:
      if not part in self.EXPECTATIONS:
        self._AddError(lineno, 'Unsupported expectation: %s' % part,
            test_list_path)
        continue
      expectation = self.EXPECTATIONS[part]
      result.add(expectation)
    return result

  def _ExpandTests(self, test_list_path):
    # Convert the test specification to an absolute, normalized
    # path and make sure directories end with the OS path separator.
    path = os.path.join(path_utils.LayoutTestsDir(test_list_path),
                        test_list_path)
    path = os.path.normpath(path)
    if os.path.isdir(path): path = os.path.join(path, '')
    # This is kind of slow - O(n*m) - since this is called for all
    # entries in the test lists. It has not been a performance
    # issue so far. Maybe we should re-measure the time spent reading
    # in the test lists?
    result = []
    for test in self._full_test_list:
      if test.startswith(path): result.append(test)
    return result

  def _AddTests(self, tests, expectations, test_list_path, lineno, modifiers,
                options):
    for test in tests:
      if self._AlreadySeenTest(test, test_list_path, lineno):
        continue

      self._ClearExpectationsForTest(test, test_list_path)
      self._AddTest(test, modifiers, expectations, options)

  def _AddTest(self, test, modifiers, expectations, options):
    """Sets the expected state for a given test.

    This routine assumes the test has not been added before. If it has,
    use _ClearExpectationsForTest() to reset the state prior to calling this.

    Args:
      test: test to add
      modifiers: sequence of modifier keywords ('wontfix', 'slow', etc.)
      expectations: sequence of expectations (PASS, IMAGE, etc.)
      options: sequence of keywords and bug identifiers."""
    self._test_to_expectations[test] = expectations
    for expectation in expectations:
      self._expectation_to_tests[expectation].add(test)

    self._test_to_options[test] = options
    self._test_to_modifiers[test] = set()
    for modifier in modifiers:
      mod_value = self.MODIFIERS[modifier]
      self._modifier_to_tests[mod_value].add(test)
      self._test_to_modifiers[test].add(mod_value)

    if 'wontfix' in modifiers:
      self._timeline_to_tests[WONTFIX].add(test)
    elif 'defer' in modifiers:
      self._timeline_to_tests[DEFER].add(test)
    else:
      self._timeline_to_tests[NOW].add(test)

    if 'skip' in modifiers:
      self._result_type_to_tests[SKIP].add(test)
    elif expectations == set([PASS]):
      self._result_type_to_tests[PASS].add(test)
    elif len(expectations) > 1:
      self._result_type_to_tests[FLAKY].add(test)
    else:
      self._result_type_to_tests[FAIL].add(test)


  def _ClearExpectationsForTest(self, test, test_list_path):
    """Remove prexisting expectations for this test.
    This happens if we are seeing a more precise path
    than a previous listing.
    """
    if test in self._test_list_paths:
      self._test_to_expectations.pop(test, '')
      self._RemoveFromSets(test, self._expectation_to_tests)
      self._RemoveFromSets(test, self._modifier_to_tests)
      self._RemoveFromSets(test, self._timeline_to_tests)
      self._RemoveFromSets(test, self._result_type_to_tests)

    self._test_list_paths[test] = os.path.normpath(test_list_path)

  def _RemoveFromSets(self, test, dict):
    """Removes the given test from the sets in the dictionary.

    Args:
      test: test to look for
      dict: dict of sets of files"""
    for set_of_tests in dict.itervalues():
      if test in set_of_tests:
        set_of_tests.remove(test)

  def _AlreadySeenTest(self, test, test_list_path, lineno):
    """Returns true if we've already seen a more precise path for this test
    than the test_list_path.
    """
    if not test in self._test_list_paths:
      return False

    prev_base_path = self._test_list_paths[test]
    if (prev_base_path == os.path.normpath(test_list_path)):
      self._AddError(lineno, 'Duplicate expectations.', test)
      return True

    # Check if we've already seen a more precise path.
    return prev_base_path.startswith(os.path.normpath(test_list_path))

  def _AddError(self, lineno, msg, path):
    """Reports an error that will prevent running the tests. Does not
    immediately raise an exception because we'd like to aggregate all the
    errors so they can all be printed out."""
    self._errors.append('\nLine:%s %s %s' % (lineno, msg, path))

  def _LogNonFatalError(self, lineno, msg, path):
    """Reports an error that will not prevent running the tests. These are
    still errors, but not bad enough to warrant breaking test running."""
    self._non_fatal_errors.append('Line:%s %s %s' % (lineno, msg, path))
