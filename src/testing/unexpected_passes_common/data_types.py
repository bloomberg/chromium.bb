# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Various custom data types for use throughout the unexpected pass finder."""

from __future__ import print_function

import copy
import fnmatch
import logging

import six

FULL_PASS = 1
NEVER_PASS = 2
PARTIAL_PASS = 3


class Expectation(object):
  """Container for a test expectation.

  Similar to typ's expectations_parser.Expectation class, but with unnecessary
  data stripped out and made hashable.

  The data contained in an Expectation is equivalent to a single line in an
  expectation file.
  """

  def __init__(self, test, tags, expected_results, bug=None):
    self.test = test
    self.tags = frozenset(tags)
    self.bug = bug or ''
    if isinstance(expected_results, str):
      self.expected_results = frozenset([expected_results])
    else:
      self.expected_results = frozenset(expected_results)

    # We're going to be making a lot of comparisons, and fnmatch is *much*
    # slower (~40x from rough testing) than a straight comparison, so only use
    # it if necessary.
    if '*' in test:
      self._comp = lambda r: fnmatch.fnmatch(r, self.test)
    else:
      self._comp = lambda r: r == self.test

  def __eq__(self, other):
    return (isinstance(other, Expectation) and self.test == other.test
            and self.tags == other.tags
            and self.expected_results == other.expected_results
            and self.bug == other.bug)

  def __ne__(self, other):
    return not self.__eq__(other)

  def __hash__(self):
    return hash((self.test, self.tags, self.expected_results, self.bug))

  def AppliesToResult(self, result):
    """Checks whether this expectation should have applied to |result|.

    An expectation applies to a result if the test names match (including
    wildcard expansion) and the expectation's tags are a subset of the result's
    tags.

    Args:
      result: A Result instance to check against.

    Returns:
      True if |self| applies to |result|, otherwise False.
    """
    assert isinstance(result, Result)
    return (self._comp(result.test) and self.tags <= result.tags)


class Result(object):
  """Container for a test result.

  Contains the minimal amount of data necessary to describe/identify a result
  from ResultDB for the purposes of the unexpected pass finder.
  """

  def __init__(self, test, tags, actual_result, step, build_id):
    """
    Args:
      test: A string containing the name of the test. Cannot have wildcards.
      tags: An iterable containing the typ tags for the result.
      actual_result: The actual result of the test as a string.
      step: A string containing the name of the step on the builder.
      build_id: A string containing the Buildbucket ID for the build this result
          came from.
    """
    # Results should not have any globs.
    assert '*' not in test
    self.test = test
    self.tags = frozenset(tags)
    self.actual_result = actual_result
    self.step = step
    self.build_id = build_id

  def __eq__(self, other):
    return (isinstance(other, Result) and self.test == other.test
            and self.tags == other.tags
            and self.actual_result == other.actual_result
            and self.step == other.step and self.build_id == other.build_id)

  def __ne__(self, other):
    return not self.__eq__(other)

  def __hash__(self):
    return hash(
        (self.test, self.tags, self.actual_result, self.step, self.build_id))


class BuildStats(object):
  """Container for keeping track of a builder's pass/fail stats."""

  def __init__(self):
    self.passed_builds = 0
    self.total_builds = 0
    self.failure_links = frozenset()

  @property
  def failed_builds(self):
    return self.total_builds - self.passed_builds

  @property
  def did_fully_pass(self):
    return self.passed_builds == self.total_builds

  @property
  def did_never_pass(self):
    return self.failed_builds == self.total_builds

  def AddPassedBuild(self):
    self.passed_builds += 1
    self.total_builds += 1

  def AddFailedBuild(self, build_id):
    self.total_builds += 1
    build_link = BuildLinkFromBuildId(build_id)
    self.failure_links = frozenset([build_link]) | self.failure_links

  def __eq__(self, other):
    return (isinstance(other, BuildStats)
            and self.passed_builds == other.passed_builds
            and self.total_builds == other.total_builds
            and self.failure_links == other.failure_links)

  def __ne__(self, other):
    return not self.__eq__(other)


def BuildLinkFromBuildId(build_id):
  return 'http://ci.chromium.org/b/%s' % build_id


# These explicit overrides could likely be replaced by using regular dicts with
# type hinting in Python 3. Based on https://stackoverflow.com/a/2588648, this
# should cover all cases where the dict can be modified.
class BaseTypedMap(dict):
  """A base class for typed dictionaries.

  Any child classes that override __setitem__ will have any modifications to the
  dictionary go through the type checking in __setitem__.
  """

  def __init__(self, *args, **kwargs):  # pylint:disable=super-init-not-called
    self.update(*args, **kwargs)

  def update(self, *args, **kwargs):
    if args:
      assert len(args) == 1
      other = dict(args[0])
      for k, v in other.items():
        self[k] = v
    for k, v in kwargs.items():
      self[k] = v

  def setdefault(self, key, value=None):
    if key not in self:
      self[key] = value
    return self[key]

  def _value_type(self):
    raise NotImplementedError()

  def IterToValueType(self, value_type):
    """Recursively iterates over contents until |value_type| is found.

    Used to get rid of nested loops, instead using a single loop that
    automatically iterates through all the contents at a certain depth.

    Args:
      value_type: The type to recurse to and then iterate over. For example,
          "BuilderStepMap" would result in iterating over the BuilderStepMap
          values, meaning that the returned generator would create tuples in the
          form (test_name, expectation, builder_map).

    Returns:
      A generator that yields tuples. The length and content of the tuples will
      vary depending on |value_type|. For example, using "BuilderStepMap" would
      result in tuples of the form (test_name, expectation, builder_map), while
      "BuildStats" would result in (test_name, expectation, builder_name,
      step_name, build_stats).
    """
    if self._value_type() == value_type:
      for k, v in self.items():
        yield k, v
    else:
      for k, v in self.items():
        for nested_value in v.IterToValueType(value_type):
          yield (k, ) + nested_value

  def Merge(self, other_map, reference_map=None):
    """Merges |other_map| into self.

    Args:
      other_map: A BaseTypedMap whose contents will be merged into self.
      reference_map: A dict containing the information that was originally in
          self. Used for ensuring that a single expectation/builder/step
          combination is only ever updated once. If None, a copy of self will be
          used.
    """
    assert isinstance(other_map, self.__class__)
    # We should only ever encounter a single updated BuildStats for an
    # expectation/builder/step combination. Use the reference map to determine
    # if a particular BuildStats has already been updated or not.
    reference_map = reference_map or copy.deepcopy(self)
    for key, value in other_map.items():
      if key not in self:
        self[key] = value
      else:
        if isinstance(value, dict):
          self[key].Merge(value, reference_map.get(key, {}))
        else:
          assert isinstance(value, BuildStats)
          # Ensure we haven't updated this BuildStats already. If the reference
          # map doesn't have a corresponding BuildStats, then base_map shouldn't
          # have initially either, and thus it would have been added before
          # reaching this point. Otherwise, the two values must match, meaning
          # that base_map's BuildStats hasn't been updated yet.
          reference_stats = reference_map.get(key, None)
          assert reference_stats is not None
          assert reference_stats == self[key]
          self[key] = value


class TestExpectationMap(BaseTypedMap):
  """Typed map for string types -> ExpectationBuilderMap.

  This results in a dict in the following format:
  {
    test_name1 (str): {
      expectation1 (data_types.Expectation): {
        builder_name1 (str): {
          step_name1 (str): stats1 (data_types.BuildStats),
          step_name2 (str): stats2 (data_types.BuildStats),
          ...
        },
        builder_name2 (str): { ... },
      },
      expectation2 (data_types.Expectation): { ... },
      ...
    },
    test_name2 (str): { ... },
    ...
  }
  """

  def __setitem__(self, key, value):
    assert IsStringType(key)
    assert isinstance(value, ExpectationBuilderMap)
    super(TestExpectationMap, self).__setitem__(key, value)

  def _value_type(self):
    return ExpectationBuilderMap

  def IterBuilderStepMaps(self):
    """Iterates over all BuilderStepMaps contained in the map.

    Returns:
      A generator yielding tuples in the form (test_name (str), expectation
      (Expectation), builder_map (BuilderStepMap))
    """
    return self.IterToValueType(BuilderStepMap)

  def AddResultList(self, builder, results):
    """Adds |results| to |self|.

    Args:
      builder: A string containing the builder |results| came from. Should be
          prefixed with something to distinguish between identically named CI
          and try builders.
      results: A list of data_types.Result objects corresponding to the ResultDB
          data queried for |builder|.

    Returns:
      A list of data_types.Result objects who did not have a matching
      expectation in |self|.
    """
    failure_results = set()
    pass_results = set()
    unmatched_results = []
    for r in results:
      if r.actual_result == 'Pass':
        pass_results.add(r)
      else:
        failure_results.add(r)

    # Remove any cases of failure -> pass from the passing set. If a test is
    # flaky, we get both pass and failure results for it, so we need to remove
    # the any cases of a pass result having a corresponding, earlier failure
    # result.
    modified_failing_retry_results = set()
    for r in failure_results:
      modified_failing_retry_results.add(
          Result(r.test, r.tags, 'Pass', r.step, r.build_id))
    pass_results -= modified_failing_retry_results

    for r in pass_results | failure_results:
      found_matching = self._AddResult(r, builder)
      if not found_matching:
        unmatched_results.append(r)

    return unmatched_results

  def _AddResult(self, result, builder):
    """Adds a single |result| to |self|.

    Args:
      result: A data_types.Result object to add.
      builder: A string containing the name of the builder |result| came from.

    Returns:
      True if an expectation in |self| applied to |result|, otherwise False.
    """
    found_matching_expectation = False
    # We need to use fnmatch since wildcards are supported, so there's no point
    # in checking the test name key right now. The AppliesToResult check already
    # does an fnmatch check.
    for _, expectation, builder_map in self.IterBuilderStepMaps():
      if expectation.AppliesToResult(result):
        found_matching_expectation = True
        step_map = builder_map.setdefault(builder, StepBuildStatsMap())
        stats = step_map.setdefault(result.step, BuildStats())
        if result.actual_result == 'Pass':
          stats.AddPassedBuild()
        else:
          stats.AddFailedBuild(result.build_id)
    return found_matching_expectation

  def SplitByStaleness(self):
    """Separates stored data based on expectation staleness.

    Returns:
      Three TestExpectationMaps (stale_dict, semi_stale_dict, active_dict). All
      three combined contain the information of |self|. |stale_dict| contains
      entries for expectations that are no longer being helpful,
      |semi_stale_dict| contains entries for expectations that might be
      removable or modifiable, but have at least one failed test run.
      |active_dict| contains entries for expectations that are preventing
      failures on all builders they're active on, and thus shouldn't be removed.
    """
    stale_dict = TestExpectationMap()
    semi_stale_dict = TestExpectationMap()
    active_dict = TestExpectationMap()

    # This initially looks like a good target for using
    # TestExpectationMap's iterators since there are many nested loops.
    # However, we need to reset state in different loops, and the alternative of
    # keeping all the state outside the loop and resetting under certain
    # conditions ends up being less readable than just using nested loops.
    for test_name, expectation_map in self.items():
      for expectation, builder_map in expectation_map.items():
        # A temporary map to hold data so we can later determine whether an
        # expectation is stale, semi-stale, or active.
        tmp_map = {
            FULL_PASS: BuilderStepMap(),
            NEVER_PASS: BuilderStepMap(),
            PARTIAL_PASS: BuilderStepMap(),
        }

        split_stats_map = builder_map.SplitBuildStatsByPass()
        for builder_name, (fully_passed, never_passed,
                           partially_passed) in split_stats_map.items():
          if fully_passed:
            tmp_map[FULL_PASS][builder_name] = fully_passed
          if never_passed:
            tmp_map[NEVER_PASS][builder_name] = never_passed
          if partially_passed:
            tmp_map[PARTIAL_PASS][builder_name] = partially_passed

        def _CopyPassesIntoBuilderMap(builder_map, pass_types):
          for pt in pass_types:
            for builder, steps in tmp_map[pt].items():
              builder_map.setdefault(builder, StepBuildStatsMap()).update(steps)

        # Handle the case of a stale expectation.
        if not (tmp_map[NEVER_PASS] or tmp_map[PARTIAL_PASS]):
          builder_map = stale_dict.setdefault(
              test_name,
              ExpectationBuilderMap()).setdefault(expectation, BuilderStepMap())
          _CopyPassesIntoBuilderMap(builder_map, [FULL_PASS])
        # Handle the case of an active expectation.
        elif not tmp_map[FULL_PASS]:
          builder_map = active_dict.setdefault(
              test_name,
              ExpectationBuilderMap()).setdefault(expectation, BuilderStepMap())
          _CopyPassesIntoBuilderMap(builder_map, [NEVER_PASS, PARTIAL_PASS])
        # Handle the case of a semi-stale expectation.
        else:
          # TODO(crbug.com/998329): Sort by pass percentage so it's easier to
          # find problematic builders without highlighting.
          builder_map = semi_stale_dict.setdefault(
              test_name,
              ExpectationBuilderMap()).setdefault(expectation, BuilderStepMap())
          _CopyPassesIntoBuilderMap(builder_map,
                                    [FULL_PASS, PARTIAL_PASS, NEVER_PASS])
    return stale_dict, semi_stale_dict, active_dict

  def FilterOutUnusedExpectations(self):
    """Filters out any unused Expectations from stored data.

    An Expectation is considered unused if its corresponding dictionary is
    empty. If removing Expectations results in a top-level test key having an
    empty dictionary, that test entry will also be removed.

    Returns:
      A list containing any Expectations that were removed.
    """
    logging.info('Filtering out unused expectations')
    unused_expectations = []
    for _, expectation, builder_map in self.IterBuilderStepMaps():
      if not builder_map:
        unused_expectations.append(expectation)
    for unused in unused_expectations:
      for _, expectation_map in self.items():
        if unused in expectation_map:
          del expectation_map[unused]
    logging.debug('Found %d unused expectations', len(unused_expectations))

    empty_tests = []
    for test_name, expectation_map in self.items():
      if not expectation_map:
        empty_tests.append(test_name)
    for empty in empty_tests:
      del self[empty]
    logging.debug('Found %d empty tests: %s', len(empty_tests), empty_tests)

    return unused_expectations


class ExpectationBuilderMap(BaseTypedMap):
  """Typed map for Expectation -> BuilderStepMap."""

  def __setitem__(self, key, value):
    assert isinstance(key, Expectation)
    assert isinstance(value, self._value_type())
    super(ExpectationBuilderMap, self).__setitem__(key, value)

  def _value_type(self):
    return BuilderStepMap


class BuilderStepMap(BaseTypedMap):
  """Typed map for string types -> StepBuildStatsMap."""

  def __setitem__(self, key, value):
    assert IsStringType(key)
    assert isinstance(value, self._value_type())
    super(BuilderStepMap, self).__setitem__(key, value)

  def _value_type(self):
    return StepBuildStatsMap

  def SplitBuildStatsByPass(self):
    """Splits the underlying BuildStats data by passing-ness.

    Returns:
      A dict mapping builder name to a tuple (fully_passed, never_passed,
      partially_passed). Each *_passed is a StepBuildStatsMap containing data
      for the steps that either fully passed on all builds, never passed on any
      builds, or passed some of the time.
    """
    retval = {}
    for builder_name, step_map in self.items():
      fully_passed = StepBuildStatsMap()
      never_passed = StepBuildStatsMap()
      partially_passed = StepBuildStatsMap()

      for step_name, stats in step_map.items():
        if stats.did_fully_pass:
          assert step_name not in fully_passed
          fully_passed[step_name] = stats
        elif stats.did_never_pass:
          assert step_name not in never_passed
          never_passed[step_name] = stats
        else:
          assert step_name not in partially_passed
          partially_passed[step_name] = stats
      retval[builder_name] = (fully_passed, never_passed, partially_passed)
    return retval

  def IterBuildStats(self):
    """Iterates over all BuildStats contained in the map.

    Returns:
      A generator yielding tuples in the form (builder_name (str), step_name
      (str), build_stats (BuildStats))"""
    return self.IterToValueType(BuildStats)


class StepBuildStatsMap(BaseTypedMap):
  """Typed map for string types -> BuildStats"""

  def __setitem__(self, key, value):
    assert IsStringType(key)
    assert isinstance(value, self._value_type())
    super(StepBuildStatsMap, self).__setitem__(key, value)

  def _value_type(self):
    return BuildStats


def IsStringType(s):
  return isinstance(s, six.string_types)
