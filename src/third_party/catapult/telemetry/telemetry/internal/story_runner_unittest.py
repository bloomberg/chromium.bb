# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools
import json
import math
import os
import shutil
import StringIO
import sys
import tempfile
import unittest
import logging

import mock

from py_utils import cloud_storage

from telemetry import benchmark
from telemetry.core import exceptions
from telemetry.core import util
from telemetry import decorators
from telemetry.internal.actions import page_action
from telemetry.internal.results import page_test_results
from telemetry.internal.results import results_options
from telemetry.internal import story_runner
from telemetry.internal.util import exception_formatter as ex_formatter_module
from telemetry.page import page as page_module
from telemetry.page import legacy_page_test
from telemetry import story as story_module
from telemetry.testing import fakes
from telemetry.testing import options_for_unittests
from telemetry.testing import system_stub
from telemetry.util import wpr_modes
from telemetry.value import improvement_direction
from telemetry.value import list_of_scalar_values
from telemetry.value import scalar
from telemetry.value import summary as summary_module
from telemetry.web_perf import story_test
from telemetry.web_perf import timeline_based_measurement
from telemetry.wpr import archive_info

from tracing.value import histogram as histogram_module
from tracing.value import histogram_set
from tracing.value.diagnostics import generic_set
from tracing.value.diagnostics import reserved_infos
from tracing.value.diagnostics import tag_map

# This linter complains if we define classes nested inside functions.
# pylint: disable=bad-super-call

# pylint: disable=too-many-lines

class FakePlatform(object):
  def CanMonitorThermalThrottling(self):
    return False

  def WaitForBatteryTemperature(self, _):
    pass

  def GetDeviceTypeName(self):
    return 'GetDeviceTypeName'

  def GetArchName(self):
    return 'amd64'

  def GetOSName(self):
    return 'win'

  def GetOSVersionName(self):
    return 'win10'

  def GetSystemTotalPhysicalMemory(self):
    return 8 * (1024 ** 3)

  def GetDeviceId(self):
    return None


class TestSharedState(story_module.SharedState):

  _platform = FakePlatform()

  @classmethod
  def SetTestPlatform(cls, platform):
    cls._platform = platform

  def __init__(self, test, options, story_set):
    super(TestSharedState, self).__init__(
        test, options, story_set)
    self._test = test
    self._current_story = None

  @property
  def platform(self):
    return self._platform

  def WillRunStory(self, story):
    self._current_story = story

  def CanRunStory(self, story):
    return True

  def RunStory(self, results):
    self._test.ValidateAndMeasurePage(self._current_story, None, results)

  def DidRunStory(self, results):
    pass

  def TearDownState(self):
    pass

  def DumpStateUponFailure(self, story, results):
    pass


class TestSharedPageState(TestSharedState):
  def RunStory(self, results):
    self._test.RunPage(self._current_story, None, results)


class FooStoryState(TestSharedPageState):
  pass


class DummyTest(legacy_page_test.LegacyPageTest):
  def RunPage(self, *_):
    pass

  def ValidateAndMeasurePage(self, page, tab, results):
    pass


class EmptyMetadataForTest(benchmark.BenchmarkMetadata):
  def __init__(self):
    super(EmptyMetadataForTest, self).__init__('')


class DummyLocalStory(story_module.Story):
  def __init__(self, shared_state_class, name='', tags=None):
    if name == '':
      name = 'dummy local story'
    super(DummyLocalStory, self).__init__(
        shared_state_class, name=name, tags=tags)

  def Run(self, shared_state):
    pass

  @property
  def is_local(self):
    return True

  @property
  def url(self):
    return 'data:,'


class DummyPage(page_module.Page):
  def __init__(self, page_set, name):
    super(DummyPage, self).__init__(
        url='file://dummy_pages/dummy_page.html',
        name=name,
        page_set=page_set)


class _DisableBenchmarkExpectations(
    story_module.expectations.StoryExpectations):
  def SetExpectations(self):
    self.DisableBenchmark([story_module.expectations.ALL], 'crbug.com/123')

class _DisableStoryExpectations(story_module.expectations.StoryExpectations):
  def SetExpectations(self):
    self.DisableStory('one', [story_module.expectations.ALL], 'crbug.com/123')


class FakeBenchmark(benchmark.Benchmark):
  def __init__(self):
    super(FakeBenchmark, self).__init__()
    self._disabled = False
    self._story_disabled = False

  @classmethod
  def Name(cls):
    return 'fake'

  test = DummyTest

  def page_set(self):
    return story_module.StorySet()

  @property
  def disabled(self):
    return self._disabled

  @disabled.setter
  def disabled(self, b):
    assert isinstance(b, bool)
    self._disabled = b

  @property
  def story_disabled(self):
    return self._story_disabled

  @story_disabled.setter
  def story_disabled(self, b):
    assert isinstance(b, bool)
    self._story_disabled = b

  @property
  def expectations(self):
    if self.story_disabled:
      return _DisableStoryExpectations()
    if self.disabled:
      return _DisableBenchmarkExpectations()
    return story_module.expectations.StoryExpectations()


def _GetOptionForUnittest():
  options = options_for_unittests.GetCopy()
  options.output_formats = ['none']
  options.output_dir = tempfile.mkdtemp(prefix='story_runner_test')
  options.suppress_gtest_report = False
  parser = options.CreateParser()
  story_runner.AddCommandLineArgs(parser)
  options.MergeDefaultValues(parser.get_default_values())
  story_runner.ProcessCommandLineArgs(parser, options)
  return options


class FakeExceptionFormatterModule(object):
  @staticmethod
  def PrintFormattedException(
      exception_class=None, exception=None, tb=None, msg=None):
    pass


def GetNumberOfSuccessfulPageRuns(results):
  return len([run for run in results.all_page_runs if run.ok or run.skipped])


def GetNumberOfSkippedPageRuns(results):
  return len([run for run in results.all_page_runs if run.skipped])


class TestOnlyException(Exception):
  pass


class _Measurement(legacy_page_test.LegacyPageTest):
  i = 0
  def RunPage(self, page, _, results):
    self.i += 1
    results.AddValue(scalar.ScalarValue(
        page, 'metric', 'unit', self.i,
        improvement_direction=improvement_direction.UP))

  def ValidateAndMeasurePage(self, page, tab, results):
    self.i += 1
    results.AddValue(scalar.ScalarValue(
        page, 'metric', 'unit', self.i,
        improvement_direction=improvement_direction.UP))

def _GenerateBaseBrowserFinderOptions(options_callback=None):
  options = fakes.CreateBrowserFinderOptions()
  options.upload_results = None
  options.suppress_gtest_report = False
  options.results_label = None
  options.reset_results = False
  options.use_live_sites = False
  options.max_failures = 100
  options.pause = None
  options.pageset_repeat = 1
  options.smoke_test_mode = False
  options.output_formats = ['chartjson']
  options.run_disabled_tests = False

  if options_callback:
    options_callback(options)

  parser = options.CreateParser()
  story_runner.AddCommandLineArgs(parser)
  options.MergeDefaultValues(parser.get_default_values())
  story_runner.ProcessCommandLineArgs(parser, options)
  return options



class StoryRunnerTest(unittest.TestCase):
  def setUp(self):
    self.fake_stdout = StringIO.StringIO()
    self.actual_stdout = sys.stdout
    sys.stdout = self.fake_stdout
    self.options = _GetOptionForUnittest()
    self.results = results_options.CreateResults(
        EmptyMetadataForTest(), self.options)
    self._story_runner_logging_stub = None

  def StubOutExceptionFormatting(self):
    """Fake out exception formatter to avoid spamming the unittest stdout."""
    story_runner.exception_formatter = FakeExceptionFormatterModule
    self._story_runner_logging_stub = system_stub.Override(
        story_runner, ['logging'])
    self._story_runner_logging_stub.logging.stdout_stream = sys.stdout

  def RestoreExceptionFormatter(self):
    story_runner.exception_formatter = ex_formatter_module
    if self._story_runner_logging_stub:
      self._story_runner_logging_stub.Restore()
      self._story_runner_logging_stub = None

  def tearDown(self):
    sys.stdout = self.actual_stdout
    results_file_path = os.path.join(os.path.dirname(__file__), '..',
                                     'testing', 'results.html')
    if os.path.isfile(results_file_path):
      os.remove(results_file_path)
    self.RestoreExceptionFormatter()

    shutil.rmtree(self.options.output_dir)

  def testRunStorySet(self):
    number_stories = 3
    story_set = story_module.StorySet()
    for i in xrange(number_stories):
      story_set.AddStory(DummyLocalStory(FooStoryState, name='story_%d' % i))
    test = DummyTest()
    story_runner.Run(test, story_set, self.options, self.results)
    self.assertFalse(self.results.had_failures)
    self.assertEquals(number_stories,
                      GetNumberOfSuccessfulPageRuns(self.results))
    self.assertEquals(story_set.stories[0].wpr_mode, wpr_modes.WPR_REPLAY)

  def testRunStoryWithMissingArchiveFile(self):
    story_set = story_module.StorySet(archive_data_file='data/hi.json')
    story_set.AddStory(page_module.Page(
        'http://www.testurl.com', story_set, story_set.base_dir,
        name='http://www.testurl.com'))
    test = DummyTest()
    self.assertRaises(story_runner.ArchiveError, story_runner.Run, test,
                      story_set, self.options, self.results)

  def testRunStoryWithLongName(self):
    story_set = story_module.StorySet()
    story_set.AddStory(DummyLocalStory(FooStoryState, name='l' * 182))
    test = DummyTest()
    self.assertRaises(ValueError, story_runner.Run, test, story_set,
                      self.options, self.results)

  def testRunStoryWithLongURLPage(self):
    story_set = story_module.StorySet()
    story_set.AddStory(page_module.Page('file://long' + 'g' * 180,
                                        story_set, name='test'))
    test = DummyTest()
    story_runner.Run(test, story_set, self.options, self.results)

  def testSuccessfulTimelineBasedMeasurementTest(self):
    """Check that PageTest is not required for story_runner.Run.

    Any PageTest related calls or attributes need to only be called
    for PageTest tests.
    """
    class TestSharedTbmState(TestSharedState):
      def RunStory(self, results):
        pass

    TEST_WILL_RUN_STORY = 'test.WillRunStory'
    TEST_MEASURE = 'test.Measure'
    TEST_DID_RUN_STORY = 'test.DidRunStory'

    EXPECTED_CALLS_IN_ORDER = [TEST_WILL_RUN_STORY,
                               TEST_MEASURE,
                               TEST_DID_RUN_STORY]

    test = timeline_based_measurement.TimelineBasedMeasurement(
        timeline_based_measurement.Options())

    manager = mock.MagicMock()
    test.WillRunStory = mock.MagicMock()
    test.Measure = mock.MagicMock()
    test.DidRunStory = mock.MagicMock()
    manager.attach_mock(test.WillRunStory, TEST_WILL_RUN_STORY)
    manager.attach_mock(test.Measure, TEST_MEASURE)
    manager.attach_mock(test.DidRunStory, TEST_DID_RUN_STORY)

    story_set = story_module.StorySet()
    story_set.AddStory(DummyLocalStory(TestSharedTbmState, name='foo'))
    story_set.AddStory(DummyLocalStory(TestSharedTbmState, name='bar'))
    story_set.AddStory(DummyLocalStory(TestSharedTbmState, name='baz'))
    story_runner.Run(test, story_set, self.options, self.results)
    self.assertFalse(self.results.had_failures)
    self.assertEquals(3, GetNumberOfSuccessfulPageRuns(self.results))

    self.assertEquals(3*EXPECTED_CALLS_IN_ORDER,
                      [call[0] for call in manager.mock_calls])

  def testCallOrderBetweenStoryTestAndSharedState(self):
    """Check that the call order between StoryTest and SharedState is correct.
    """
    TEST_WILL_RUN_STORY = 'test.WillRunStory'
    TEST_MEASURE = 'test.Measure'
    TEST_DID_RUN_STORY = 'test.DidRunStory'
    STATE_WILL_RUN_STORY = 'state.WillRunStory'
    STATE_RUN_STORY = 'state.RunStory'
    STATE_DID_RUN_STORY = 'state.DidRunStory'

    EXPECTED_CALLS_IN_ORDER = [TEST_WILL_RUN_STORY,
                               STATE_WILL_RUN_STORY,
                               STATE_RUN_STORY,
                               TEST_MEASURE,
                               TEST_DID_RUN_STORY,
                               STATE_DID_RUN_STORY]

    class TestStoryTest(story_test.StoryTest):
      def WillRunStory(self, platform):
        pass

      def Measure(self, platform, results):
        pass

      def DidRunStory(self, platform, results):
        pass

    class TestSharedStateForStoryTest(TestSharedState):
      def RunStory(self, results):
        pass

    @mock.patch.object(TestStoryTest, 'WillRunStory')
    @mock.patch.object(TestStoryTest, 'Measure')
    @mock.patch.object(TestStoryTest, 'DidRunStory')
    @mock.patch.object(TestSharedStateForStoryTest, 'WillRunStory')
    @mock.patch.object(TestSharedStateForStoryTest, 'RunStory')
    @mock.patch.object(TestSharedStateForStoryTest, 'DidRunStory')
    def GetCallsInOrder(state_DidRunStory, state_RunStory, state_WillRunStory,
                        test_DidRunStory, test_Measure, test_WillRunStory):
      manager = mock.MagicMock()
      manager.attach_mock(test_WillRunStory, TEST_WILL_RUN_STORY)
      manager.attach_mock(test_Measure, TEST_MEASURE)
      manager.attach_mock(test_DidRunStory, TEST_DID_RUN_STORY)
      manager.attach_mock(state_WillRunStory, STATE_WILL_RUN_STORY)
      manager.attach_mock(state_RunStory, STATE_RUN_STORY)
      manager.attach_mock(state_DidRunStory, STATE_DID_RUN_STORY)

      test = TestStoryTest()
      story_set = story_module.StorySet()
      story_set.AddStory(DummyLocalStory(TestSharedStateForStoryTest))
      story_runner.Run(test, story_set, self.options, self.results)
      return [call[0] for call in manager.mock_calls]

    calls_in_order = GetCallsInOrder() # pylint: disable=no-value-for-parameter
    self.assertEquals(EXPECTED_CALLS_IN_ORDER, calls_in_order)

  def testAppCrashExceptionCausesFailure(self):
    self.StubOutExceptionFormatting()
    story_set = story_module.StorySet()
    class SharedStoryThatCausesAppCrash(TestSharedPageState):
      def WillRunStory(self, story):
        raise exceptions.AppCrashException(msg='App Foo crashes')

    story_set.AddStory(DummyLocalStory(
        SharedStoryThatCausesAppCrash))
    story_runner.Run(DummyTest(), story_set, self.options, self.results)
    self.assertTrue(self.results.had_failures)
    self.assertEquals(0, GetNumberOfSuccessfulPageRuns(self.results))
    self.assertIn('App Foo crashes', self.fake_stdout.getvalue())

  def testExceptionRaisedInSharedStateTearDown(self):
    self.StubOutExceptionFormatting()
    story_set = story_module.StorySet()
    class SharedStoryThatCausesAppCrash(TestSharedPageState):
      def TearDownState(self):
        raise TestOnlyException()

    story_set.AddStory(DummyLocalStory(
        SharedStoryThatCausesAppCrash))
    with self.assertRaises(TestOnlyException):
      story_runner.Run(DummyTest(), story_set, self.options, self.results)

  def testUnknownExceptionIsNotFatal(self):
    self.StubOutExceptionFormatting()
    story_set = story_module.StorySet()

    class UnknownException(Exception):
      pass

    # This erroneous test is set up to raise exception for the 1st story
    # run.
    class Test(legacy_page_test.LegacyPageTest):
      def __init__(self, *args):
        super(Test, self).__init__(*args)
        self.run_count = 0

      def RunPage(self, *_):
        old_run_count = self.run_count
        self.run_count += 1
        if old_run_count == 0:
          raise UnknownException('FooBarzException')

      def ValidateAndMeasurePage(self, page, tab, results):
        pass

    s1 = DummyLocalStory(TestSharedPageState, name='foo')
    s2 = DummyLocalStory(TestSharedPageState, name='bar')
    story_set.AddStory(s1)
    story_set.AddStory(s2)
    test = Test()
    story_runner.Run(test, story_set, self.options, self.results)
    self.assertEqual(set([s1]), self.results.pages_that_failed)
    self.assertEqual(set([s2]), self.results.pages_that_succeeded)
    self.assertIn('FooBarzException', self.fake_stdout.getvalue())

  def testRaiseBrowserGoneExceptionFromRunPage(self):
    self.StubOutExceptionFormatting()
    story_set = story_module.StorySet()

    class Test(legacy_page_test.LegacyPageTest):
      def __init__(self, *args):
        super(Test, self).__init__(*args)
        self.run_count = 0

      def RunPage(self, *_):
        old_run_count = self.run_count
        self.run_count += 1
        if old_run_count == 0:
          raise exceptions.BrowserGoneException(
              None, 'i am a browser crash message')

      def ValidateAndMeasurePage(self, page, tab, results):
        pass

    story_set.AddStory(DummyLocalStory(TestSharedPageState, name='foo'))
    story_set.AddStory(DummyLocalStory(TestSharedPageState, name='bar'))
    test = Test()
    story_runner.Run(test, story_set, self.options, self.results)
    self.assertEquals(2, test.run_count)
    self.assertTrue(self.results.had_failures)
    self.assertEquals(1, GetNumberOfSuccessfulPageRuns(self.results))

  def testAppCrashThenRaiseInTearDownFatal(self):
    self.StubOutExceptionFormatting()
    story_set = story_module.StorySet()

    unit_test_events = []  # track what was called when
    class DidRunTestError(Exception):
      pass

    class TestTearDownSharedState(TestSharedPageState):
      def TearDownState(self):
        unit_test_events.append('tear-down-state')
        raise DidRunTestError

      def DumpStateUponFailure(self, story, results):
        unit_test_events.append('dump-state')


    class Test(legacy_page_test.LegacyPageTest):
      def __init__(self, *args):
        super(Test, self).__init__(*args)
        self.run_count = 0

      def RunPage(self, *_):
        old_run_count = self.run_count
        self.run_count += 1
        if old_run_count == 0:
          unit_test_events.append('app-crash')
          raise exceptions.AppCrashException

      def ValidateAndMeasurePage(self, page, tab, results):
        pass

    story_set.AddStory(DummyLocalStory(TestTearDownSharedState, name='foo'))
    story_set.AddStory(DummyLocalStory(TestTearDownSharedState, name='bar'))
    test = Test()

    with self.assertRaises(DidRunTestError):
      story_runner.Run(test, story_set, self.options, self.results)
    self.assertEqual(['app-crash', 'dump-state', 'tear-down-state'],
                     unit_test_events)
    # The AppCrashException gets added as a failure.
    self.assertTrue(self.results.had_failures)

  def testPagesetRepeat(self):
    story_set = story_module.StorySet()

    # TODO(eakuefner): Factor this out after flattening page ref in Value
    blank_story = DummyLocalStory(TestSharedPageState, name='blank')
    green_story = DummyLocalStory(TestSharedPageState, name='green')
    story_set.AddStory(blank_story)
    story_set.AddStory(green_story)

    self.options.pageset_repeat = 2
    self.options.output_formats = []
    results = results_options.CreateResults(
        EmptyMetadataForTest(), self.options)
    story_runner.Run(_Measurement(), story_set, self.options, results)
    summary = summary_module.Summary(results)
    values = summary.interleaved_computed_per_page_values_and_summaries

    blank_value = list_of_scalar_values.ListOfScalarValues(
        blank_story, 'metric', 'unit', [1, 3],
        improvement_direction=improvement_direction.UP)
    green_value = list_of_scalar_values.ListOfScalarValues(
        green_story, 'metric', 'unit', [2, 4],
        improvement_direction=improvement_direction.UP)
    merged_value = list_of_scalar_values.ListOfScalarValues(
        None, 'metric', 'unit',
        [1, 3, 2, 4], std=math.sqrt(2),  # Pooled standard deviation.
        improvement_direction=improvement_direction.UP)

    self.assertEquals(4, GetNumberOfSuccessfulPageRuns(results))
    self.assertFalse(results.had_failures)
    self.assertEquals(3, len(values))
    self.assertIn(blank_value, values)
    self.assertIn(green_value, values)
    self.assertIn(merged_value, values)

  def testSmokeTestMode(self):
    story_set = story_module.StorySet()

    blank_story = DummyLocalStory(TestSharedPageState, name='blank')
    green_story = DummyLocalStory(TestSharedPageState, name='green')
    story_set.AddStory(blank_story)
    story_set.AddStory(green_story)

    self.options.pageset_repeat = 2
    self.options.smoke_test_mode = True
    results = results_options.CreateResults(
        EmptyMetadataForTest(), self.options)
    story_runner.Run(_Measurement(), story_set, self.options, results)
    summary = summary_module.Summary(results)
    values = summary.interleaved_computed_per_page_values_and_summaries


    self.assertEquals(2, GetNumberOfSuccessfulPageRuns(results))
    self.assertFalse(results.had_failures)
    self.assertEquals(3, len(values))

  def testRunStoryDisabledStory(self):
    story_set = story_module.StorySet()
    story_one = DummyLocalStory(TestSharedPageState, name='one')
    story_set.AddStory(story_one)
    results = results_options.CreateResults(
        EmptyMetadataForTest(), self.options)

    story_runner.Run(_Measurement(), story_set, self.options, results,
                     expectations=_DisableStoryExpectations())
    summary = summary_module.Summary(results)
    values = summary.interleaved_computed_per_page_values_and_summaries

    self.assertEquals(1, GetNumberOfSuccessfulPageRuns(results))
    self.assertEquals(1, GetNumberOfSkippedPageRuns(results))
    self.assertFalse(results.had_failures)
    self.assertEquals(0, len(values))

  def testRunStoryOneDisabledOneNot(self):
    story_set = story_module.StorySet()
    story_one = DummyLocalStory(TestSharedPageState, name='one')
    story_two = DummyLocalStory(TestSharedPageState, name='two')
    story_set.AddStory(story_one)
    story_set.AddStory(story_two)
    results = results_options.CreateResults(
        EmptyMetadataForTest(), self.options)

    story_runner.Run(_Measurement(), story_set, self.options, results,
                     expectations=_DisableStoryExpectations())
    summary = summary_module.Summary(results)
    values = summary.interleaved_computed_per_page_values_and_summaries

    self.assertEquals(2, GetNumberOfSuccessfulPageRuns(results))
    self.assertEquals(1, GetNumberOfSkippedPageRuns(results))
    self.assertFalse(results.had_failures)
    self.assertEquals(2, len(values))

  def testRunStoryDisabledOverriddenByFlag(self):
    story_set = story_module.StorySet()
    story_one = DummyLocalStory(TestSharedPageState, name='one')
    story_set.AddStory(story_one)
    self.options.run_disabled_tests = True
    results = results_options.CreateResults(
        EmptyMetadataForTest(), self.options)

    story_runner.Run(_Measurement(), story_set, self.options, results,
                     expectations=_DisableStoryExpectations())
    summary = summary_module.Summary(results)
    values = summary.interleaved_computed_per_page_values_and_summaries

    self.assertEquals(1, GetNumberOfSuccessfulPageRuns(results))
    self.assertEquals(0, GetNumberOfSkippedPageRuns(results))
    self.assertFalse(results.had_failures)
    self.assertEquals(2, len(values))

  def testRunStoryPopulatesHistograms(self):
    self.StubOutExceptionFormatting()
    story_set = story_module.StorySet()

    class Test(legacy_page_test.LegacyPageTest):
      def __init__(self, *args):
        super(Test, self).__init__(*args)

      # pylint: disable=unused-argument
      def RunPage(self, _, _2, results):
        results.AddHistogram(
            histogram_module.Histogram('hist', 'count'))

      def ValidateAndMeasurePage(self, page, tab, results):
        pass

    s1 = DummyLocalStory(TestSharedPageState, name='foo')
    story_set.AddStory(s1)
    test = Test()
    story_runner.Run(test, story_set, self.options, self.results)

    dicts = self.results.AsHistogramDicts()
    hs = histogram_set.HistogramSet()
    hs.ImportDicts(dicts)

    self.assertEqual(1, len(hs))
    self.assertEqual('hist', hs.GetFirstHistogram().name)

  def testRunStoryAddsDeviceInfo(self):
    story_set = story_module.StorySet()
    story_set.AddStory(DummyLocalStory(FooStoryState, 'foo', ['bar']))
    story_runner.Run(DummyTest(), story_set, self.options, self.results)

    hs = histogram_set.HistogramSet()
    hs.ImportDicts(self.results.AsHistogramDicts())

    generic_diagnostics = hs.GetSharedDiagnosticsOfType(
        generic_set.GenericSet)

    generic_diagnostics_values = [
        list(diagnostic) for diagnostic in generic_diagnostics]

    self.assertGreater(len(generic_diagnostics), 2)
    self.assertIn(['win10'], generic_diagnostics_values)
    self.assertIn(['win'], generic_diagnostics_values)
    self.assertIn(['amd64'], generic_diagnostics_values)

  def testRunStoryAddsDeviceInfo_EvenInErrors(self):
    class ErrorRaisingDummyLocalStory(DummyLocalStory):
      def __init__(self, shared_state_class, name='', tags=None):
        if name == '':
          name = 'dummy local story'
        super(ErrorRaisingDummyLocalStory, self).__init__(
            shared_state_class, name=name, tags=tags)

      def Run(self, shared_state):
        raise BaseException('foo')

      @property
      def is_local(self):
        return True

      @property
      def url(self):
        return 'data:,'

    story_set = story_module.StorySet()
    story_set.AddStory(ErrorRaisingDummyLocalStory(
        FooStoryState, 'foo', ['bar']))
    story_runner.Run(DummyTest(), story_set, self.options, self.results)

    hs = histogram_set.HistogramSet()
    hs.ImportDicts(self.results.AsHistogramDicts())

    generic_diagnostics = hs.GetSharedDiagnosticsOfType(
        generic_set.GenericSet)

    generic_diagnostics_values = [
        list(diagnostic) for diagnostic in generic_diagnostics]

    self.assertGreater(len(generic_diagnostics), 2)
    self.assertIn(['win10'], generic_diagnostics_values)
    self.assertIn(['win'], generic_diagnostics_values)
    self.assertIn(['amd64'], generic_diagnostics_values)

  def testRunStoryAddsDeviceInfo_OnePerStorySet(self):
    class Test(legacy_page_test.LegacyPageTest):
      def __init__(self, *args):
        super(Test, self).__init__(*args)

      # pylint: disable=unused-argument
      def RunPage(self, _, _2, results):
        results.AddHistogram(
            histogram_module.Histogram('hist', 'count'))

      def ValidateAndMeasurePage(self, page, tab, results):
        pass

    story_set = story_module.StorySet()
    story_set.AddStory(DummyLocalStory(FooStoryState, 'foo', ['bar']))
    story_set.AddStory(DummyLocalStory(FooStoryState, 'abc', ['def']))
    story_runner.Run(Test(), story_set, self.options, self.results)

    hs = histogram_set.HistogramSet()
    hs.ImportDicts(self.results.AsHistogramDicts())

    generic_diagnostics = hs.GetSharedDiagnosticsOfType(
        generic_set.GenericSet)

    generic_diagnostics_values = [
        list(diagnostic) for diagnostic in generic_diagnostics]

    self.assertGreater(len(generic_diagnostics), 2)
    self.assertIn(['win10'], generic_diagnostics_values)
    self.assertIn(['win'], generic_diagnostics_values)
    self.assertIn(['amd64'], generic_diagnostics_values)

    self.assertEqual(1, len(
        [value for value in generic_diagnostics_values if value == ['win']]))

    first_histogram_diags = hs.GetFirstHistogram().diagnostics
    self.assertIn(reserved_infos.ARCHITECTURES.name, first_histogram_diags)
    self.assertIn(reserved_infos.OS_NAMES.name, first_histogram_diags)
    self.assertIn(reserved_infos.OS_VERSIONS.name, first_histogram_diags)

  def testRunStoryAddsTagMap(self):
    story_set = story_module.StorySet()
    story_set.AddStory(DummyLocalStory(FooStoryState, 'foo', ['bar']))
    story_runner.Run(DummyTest(), story_set, self.options, self.results)

    hs = histogram_set.HistogramSet()
    hs.ImportDicts(self.results.AsHistogramDicts())
    tagmap = None
    for diagnostic in hs.shared_diagnostics:
      if isinstance(diagnostic, tag_map.TagMap):
        tagmap = diagnostic
        break

    self.assertIsNotNone(tagmap)
    self.assertListEqual(['bar'], tagmap.tags_to_story_names.keys())
    self.assertSetEqual(set(['foo']), tagmap.tags_to_story_names['bar'])

  def testRunStoryAddsTagMapEvenInFatalException(self):
    self.StubOutExceptionFormatting()
    story_set = story_module.StorySet()

    class Test(legacy_page_test.LegacyPageTest):
      def __init__(self, *args):
        super(Test, self).__init__(*args)

      def RunPage(self, *_):
        raise MemoryError('Fatal exception')

      def ValidateAndMeasurePage(self, page, tab, results):
        pass

    s1 = DummyLocalStory(TestSharedPageState, name='foo', tags=['footag'])
    s2 = DummyLocalStory(TestSharedPageState, name='bar', tags=['bartag'])
    story_set.AddStory(s1)
    story_set.AddStory(s2)
    test = Test()
    with self.assertRaises(MemoryError):
      story_runner.Run(test, story_set, self.options, self.results)
    self.assertIn('Fatal exception', self.fake_stdout.getvalue())

    hs = histogram_set.HistogramSet()
    hs.ImportDicts(self.results.AsHistogramDicts())
    tagmap = None
    for diagnostic in hs.shared_diagnostics:
      if isinstance(diagnostic, tag_map.TagMap):
        tagmap = diagnostic
        break

    self.assertIsNotNone(tagmap)
    self.assertSetEqual(
        set(['footag', 'bartag']), set(tagmap.tags_to_story_names.keys()))
    self.assertSetEqual(set(['foo']), tagmap.tags_to_story_names['footag'])
    self.assertSetEqual(set(['bar']), tagmap.tags_to_story_names['bartag'])

  @decorators.Disabled('chromeos')  # crbug.com/483212
  def testUpdateAndCheckArchives(self):
    usr_stub = system_stub.Override(story_runner, ['cloud_storage'])
    wpr_stub = system_stub.Override(archive_info, ['cloud_storage'])
    archive_data_dir = os.path.join(
        util.GetTelemetryDir(),
        'telemetry', 'internal', 'testing', 'archive_files')
    try:
      story_set = story_module.StorySet()
      story_set.AddStory(page_module.Page(
          'http://www.testurl.com', story_set, story_set.base_dir,
          name='http://www.testurl.com'))
      # Page set missing archive_data_file.
      self.assertRaises(
          story_runner.ArchiveError,
          story_runner._UpdateAndCheckArchives,
          story_set.archive_data_file,
          story_set.wpr_archive_info,
          story_set.stories)

      story_set = story_module.StorySet(
          archive_data_file='missing_archive_data_file.json')
      story_set.AddStory(page_module.Page(
          'http://www.testurl.com', story_set, story_set.base_dir,
          name='http://www.testurl.com'))
      # Page set missing json file specified in archive_data_file.
      self.assertRaises(
          story_runner.ArchiveError,
          story_runner._UpdateAndCheckArchives,
          story_set.archive_data_file,
          story_set.wpr_archive_info,
          story_set.stories)

      story_set = story_module.StorySet(
          archive_data_file=os.path.join(archive_data_dir, 'test.json'),
          cloud_storage_bucket=cloud_storage.PUBLIC_BUCKET)
      story_set.AddStory(page_module.Page(
          'http://www.testurl.com', story_set, story_set.base_dir,
          name='http://www.testurl.com'))
      # Page set with valid archive_data_file.
      self.assertTrue(
          story_runner._UpdateAndCheckArchives(
              story_set.archive_data_file, story_set.wpr_archive_info,
              story_set.stories))
      story_set.AddStory(page_module.Page(
          'http://www.google.com', story_set, story_set.base_dir,
          name='http://www.google.com'))
      # Page set with an archive_data_file which exists but is missing a page.
      self.assertRaises(
          story_runner.ArchiveError,
          story_runner._UpdateAndCheckArchives,
          story_set.archive_data_file,
          story_set.wpr_archive_info,
          story_set.stories)

      story_set = story_module.StorySet(
          archive_data_file=os.path.join(
              archive_data_dir, 'test_missing_wpr_file.json'),
          cloud_storage_bucket=cloud_storage.PUBLIC_BUCKET)
      story_set.AddStory(page_module.Page(
          'http://www.testurl.com', story_set, story_set.base_dir,
          name='http://www.testurl.com'))
      story_set.AddStory(page_module.Page(
          'http://www.google.com', story_set, story_set.base_dir,
          name='http://www.google.com'))
      # Page set with an archive_data_file which exists and contains all pages
      # but fails to find a wpr file.
      self.assertRaises(
          story_runner.ArchiveError,
          story_runner._UpdateAndCheckArchives,
          story_set.archive_data_file,
          story_set.wpr_archive_info,
          story_set.stories)
    finally:
      usr_stub.Restore()
      wpr_stub.Restore()


  def _testMaxFailuresOptionIsRespectedAndOverridable(
      self, num_failing_stories, runner_max_failures, options_max_failures,
      expected_num_failures):
    class SimpleSharedState(story_module.SharedState):
      _fake_platform = FakePlatform()
      _current_story = None

      @property
      def platform(self):
        return self._fake_platform

      def WillRunStory(self, story):
        self._current_story = story

      def RunStory(self, results):
        self._current_story.Run(self)

      def DidRunStory(self, results):
        pass

      def CanRunStory(self, story):
        return True

      def TearDownState(self):
        pass

      def DumpStateUponFailure(self, story, results):
        pass

    class FailingStory(story_module.Story):
      def __init__(self, name):
        super(FailingStory, self).__init__(
            shared_state_class=SimpleSharedState,
            is_local=True, name=name)
        self.was_run = False

      def Run(self, shared_state):
        self.was_run = True
        raise legacy_page_test.Failure

      @property
      def url(self):
        return 'data:,'

    self.StubOutExceptionFormatting()

    story_set = story_module.StorySet()
    for i in range(num_failing_stories):
      story_set.AddStory(FailingStory(name='failing%d' % i))

    options = _GetOptionForUnittest()
    options.output_formats = ['none']
    options.suppress_gtest_report = True
    if options_max_failures:
      options.max_failures = options_max_failures

    results = results_options.CreateResults(EmptyMetadataForTest(), options)
    story_runner.Run(
        DummyTest(), story_set, options,
        results, max_failures=runner_max_failures)
    self.assertEquals(0, GetNumberOfSuccessfulPageRuns(results))
    self.assertTrue(results.had_failures)
    for ii, story in enumerate(story_set.stories):
      self.assertEqual(story.was_run, ii < expected_num_failures)

  def testMaxFailuresNotSpecified(self):
    self._testMaxFailuresOptionIsRespectedAndOverridable(
        num_failing_stories=5, runner_max_failures=None,
        options_max_failures=None, expected_num_failures=5)

  def testMaxFailuresSpecifiedToRun(self):
    # Runs up to max_failures+1 failing tests before stopping, since
    # every tests after max_failures failures have been encountered
    # may all be passing.
    self._testMaxFailuresOptionIsRespectedAndOverridable(
        num_failing_stories=5, runner_max_failures=3,
        options_max_failures=None, expected_num_failures=4)

  def testMaxFailuresOption(self):
    # Runs up to max_failures+1 failing tests before stopping, since
    # every tests after max_failures failures have been encountered
    # may all be passing.
    self._testMaxFailuresOptionIsRespectedAndOverridable(
        num_failing_stories=5, runner_max_failures=3,
        options_max_failures=1, expected_num_failures=2)

  def _CreateErrorProcessingMock(self, method_exceptions=None,
                                 legacy_test=False):
    if legacy_test:
      test_class = legacy_page_test.LegacyPageTest
    else:
      test_class = story_test.StoryTest

    root_mock = mock.NonCallableMock(
        story=mock.NonCallableMagicMock(story_module.Story),
        results=mock.NonCallableMagicMock(page_test_results.PageTestResults),
        test=mock.NonCallableMagicMock(test_class),
        state=mock.NonCallableMagicMock(
            story_module.SharedState,
            CanRunStory=mock.Mock(return_value=True)))

    if method_exceptions:
      root_mock.configure_mock(**{
          path + '.side_effect': exception
          for path, exception in method_exceptions.iteritems()})

    return root_mock

  def testRunStoryAndProcessErrorIfNeeded_success(self):
    root_mock = self._CreateErrorProcessingMock()

    story_runner._RunStoryAndProcessErrorIfNeeded(
        root_mock.story, root_mock.results, root_mock.state, root_mock.test)

    self.assertEquals(root_mock.method_calls, [
        mock.call.results.CreateArtifact(root_mock.story.name, 'logs'),
        mock.call.test.WillRunStory(root_mock.state.platform),
        mock.call.state.WillRunStory(root_mock.story),
        mock.call.state.CanRunStory(root_mock.story),
        mock.call.state.RunStory(root_mock.results),
        mock.call.test.Measure(root_mock.state.platform, root_mock.results),
        mock.call.test.DidRunStory(root_mock.state.platform, root_mock.results),
        mock.call.state.DidRunStory(root_mock.results),
    ])

  def testRunStoryAndProcessErrorIfNeeded_successLegacy(self):
    root_mock = self._CreateErrorProcessingMock(legacy_test=True)

    story_runner._RunStoryAndProcessErrorIfNeeded(
        root_mock.story, root_mock.results, root_mock.state, root_mock.test)

    self.assertEquals(root_mock.method_calls, [
        mock.call.results.CreateArtifact(root_mock.story.name, 'logs'),
        mock.call.state.WillRunStory(root_mock.story),
        mock.call.state.CanRunStory(root_mock.story),
        mock.call.state.RunStory(root_mock.results),
        mock.call.test.DidRunPage(root_mock.state.platform),
        mock.call.state.DidRunStory(root_mock.results),
    ])

  def testRunStoryAndProcessErrorIfNeeded_tryTimeout(self):
    root_mock = self._CreateErrorProcessingMock(method_exceptions={
        'state.WillRunStory': exceptions.TimeoutException('foo')
    })

    story_runner._RunStoryAndProcessErrorIfNeeded(
        root_mock.story, root_mock.results, root_mock.state, root_mock.test)

    self.assertEquals(root_mock.method_calls, [
        mock.call.results.CreateArtifact(root_mock.story.name, 'logs'),
        mock.call.test.WillRunStory(root_mock.state.platform),
        mock.call.state.WillRunStory(root_mock.story),
        mock.call.state.DumpStateUponFailure(
            root_mock.story, root_mock.results),
        mock.call.results.Fail(
            'Exception raised running %s' % root_mock.story.name),
        mock.call.test.DidRunStory(root_mock.state.platform, root_mock.results),
        mock.call.state.DidRunStory(root_mock.results),
    ])

  def testRunStoryAndProcessErrorIfNeeded_tryAppCrash(self):
    tmp = tempfile.NamedTemporaryFile(delete=False)
    tmp.close()
    temp_file_path = tmp.name
    fake_app = fakes.FakeApp()
    fake_app.recent_minidump_path = temp_file_path
    try:
      app_crash_exception = exceptions.AppCrashException(fake_app, msg='foo')
      root_mock = self._CreateErrorProcessingMock(method_exceptions={
          'state.WillRunStory': app_crash_exception
      })

      with self.assertRaises(exceptions.AppCrashException):
        story_runner._RunStoryAndProcessErrorIfNeeded(
            root_mock.story, root_mock.results, root_mock.state, root_mock.test)

      self.assertListEqual(root_mock.method_calls, [
          mock.call.results.CreateArtifact(root_mock.story.name, 'logs'),
          mock.call.test.WillRunStory(root_mock.state.platform),
          mock.call.state.WillRunStory(root_mock.story),
          mock.call.state.DumpStateUponFailure(
              root_mock.story, root_mock.results),
          mock.call.results.AddArtifact(
              root_mock.story.name, 'minidump', temp_file_path),
          mock.call.results.Fail(
              'Exception raised running %s' % root_mock.story.name),
          mock.call.test.DidRunStory(
              root_mock.state.platform, root_mock.results),
          mock.call.state.DidRunStory(root_mock.results),
      ])
    finally:
      os.remove(temp_file_path)

  def testRunStoryAndProcessErrorIfNeeded_tryError(self):
    root_mock = self._CreateErrorProcessingMock(method_exceptions={
        'state.CanRunStory': exceptions.Error('foo')
    })

    with self.assertRaisesRegexp(exceptions.Error, 'foo'):
      story_runner._RunStoryAndProcessErrorIfNeeded(
          root_mock.story, root_mock.results, root_mock.state, root_mock.test)

    self.assertEquals(root_mock.method_calls, [
        mock.call.results.CreateArtifact(root_mock.story.name, 'logs'),
        mock.call.test.WillRunStory(root_mock.state.platform),
        mock.call.state.WillRunStory(root_mock.story),
        mock.call.state.CanRunStory(root_mock.story),
        mock.call.state.DumpStateUponFailure(
            root_mock.story, root_mock.results),
        mock.call.results.Fail(
            'Exception raised running %s' % root_mock.story.name),
        mock.call.test.DidRunStory(root_mock.state.platform, root_mock.results),
        mock.call.state.DidRunStory(root_mock.results),
    ])

  def testRunStoryAndProcessErrorIfNeeded_tryUnsupportedAction(self):
    root_mock = self._CreateErrorProcessingMock(method_exceptions={
        'state.RunStory': page_action.PageActionNotSupported('foo')
    })

    story_runner._RunStoryAndProcessErrorIfNeeded(
        root_mock.story, root_mock.results, root_mock.state, root_mock.test)
    self.assertEquals(root_mock.method_calls, [
        mock.call.results.CreateArtifact(root_mock.story.name, 'logs'),
        mock.call.test.WillRunStory(root_mock.state.platform),
        mock.call.state.WillRunStory(root_mock.story),
        mock.call.state.CanRunStory(root_mock.story),
        mock.call.state.RunStory(root_mock.results),
        mock.call.results.Skip('Unsupported page action: foo'),
        mock.call.test.DidRunStory(root_mock.state.platform, root_mock.results),
        mock.call.state.DidRunStory(root_mock.results),
    ])

  def testRunStoryAndProcessErrorIfNeeded_tryUnhandlable(self):
    root_mock = self._CreateErrorProcessingMock(method_exceptions={
        'test.WillRunStory': Exception('foo')
    })

    with self.assertRaisesRegexp(Exception, 'foo'):
      story_runner._RunStoryAndProcessErrorIfNeeded(
          root_mock.story, root_mock.results, root_mock.state, root_mock.test)

    self.assertEquals(root_mock.method_calls, [
        mock.call.results.CreateArtifact(root_mock.story.name, 'logs'),
        mock.call.test.WillRunStory(root_mock.state.platform),
        mock.call.state.DumpStateUponFailure(
            root_mock.story, root_mock.results),
        mock.call.results.Fail(
            'Exception raised running %s' % root_mock.story.name),
        mock.call.test.DidRunStory(root_mock.state.platform, root_mock.results),
        mock.call.state.DidRunStory(root_mock.results),
    ])

  def testRunStoryAndProcessErrorIfNeeded_finallyException(self):
    exc = Exception('bar')
    root_mock = self._CreateErrorProcessingMock(method_exceptions={
        'state.DidRunStory': exc,
    })

    with self.assertRaisesRegexp(Exception, 'bar'):
      story_runner._RunStoryAndProcessErrorIfNeeded(
          root_mock.story, root_mock.results, root_mock.state, root_mock.test)

    self.assertEquals(root_mock.method_calls, [
        mock.call.results.CreateArtifact(root_mock.story.name, 'logs'),
        mock.call.test.WillRunStory(root_mock.state.platform),
        mock.call.state.WillRunStory(root_mock.story),
        mock.call.state.CanRunStory(root_mock.story),
        mock.call.state.RunStory(root_mock.results),
        mock.call.test.Measure(root_mock.state.platform, root_mock.results),
        mock.call.test.DidRunStory(root_mock.state.platform, root_mock.results),
        mock.call.state.DidRunStory(root_mock.results),
        mock.call.state.DumpStateUponFailure(
            root_mock.story, root_mock.results),
    ])

  def testRunStoryAndProcessErrorIfNeeded_tryTimeout_finallyException(self):
    root_mock = self._CreateErrorProcessingMock(method_exceptions={
        'state.RunStory': exceptions.TimeoutException('foo'),
        'state.DidRunStory': Exception('bar')
    })

    story_runner._RunStoryAndProcessErrorIfNeeded(
        root_mock.story, root_mock.results, root_mock.state, root_mock.test)

    self.assertEquals(root_mock.method_calls, [
        mock.call.results.CreateArtifact(root_mock.story.name, 'logs'),
        mock.call.test.WillRunStory(root_mock.state.platform),
        mock.call.state.WillRunStory(root_mock.story),
        mock.call.state.CanRunStory(root_mock.story),
        mock.call.state.RunStory(root_mock.results),
        mock.call.state.DumpStateUponFailure(
            root_mock.story, root_mock.results),
        mock.call.results.Fail(
            'Exception raised running %s' % root_mock.story.name),
        mock.call.test.DidRunStory(root_mock.state.platform, root_mock.results),
        mock.call.state.DidRunStory(root_mock.results),
    ])

  def testRunStoryAndProcessErrorIfNeeded_tryError_finallyException(self):
    root_mock = self._CreateErrorProcessingMock(method_exceptions={
        'state.WillRunStory': exceptions.Error('foo'),
        'test.DidRunStory': Exception('bar')
    })

    with self.assertRaisesRegexp(exceptions.Error, 'foo'):
      story_runner._RunStoryAndProcessErrorIfNeeded(
          root_mock.story, root_mock.results, root_mock.state, root_mock.test)

    self.assertEquals(root_mock.method_calls, [
        mock.call.results.CreateArtifact(root_mock.story.name, 'logs'),
        mock.call.test.WillRunStory(root_mock.state.platform),
        mock.call.state.WillRunStory(root_mock.story),
        mock.call.state.DumpStateUponFailure(
            root_mock.story, root_mock.results),
        mock.call.results.Fail(
            'Exception raised running %s' % root_mock.story.name),
        mock.call.test.DidRunStory(root_mock.state.platform, root_mock.results),
    ])

  def testRunStoryAndProcessErrorIfNeeded_tryUnsupportedAction_finallyException(
      self):
    root_mock = self._CreateErrorProcessingMock(method_exceptions={
        'test.WillRunStory': page_action.PageActionNotSupported('foo'),
        'state.DidRunStory': Exception('bar')
    })

    story_runner._RunStoryAndProcessErrorIfNeeded(
        root_mock.story, root_mock.results, root_mock.state, root_mock.test)

    self.assertEquals(root_mock.method_calls, [
        mock.call.results.CreateArtifact(root_mock.story.name, 'logs'),
        mock.call.test.WillRunStory(root_mock.state.platform),
        mock.call.results.Skip('Unsupported page action: foo'),
        mock.call.test.DidRunStory(
            root_mock.state.platform, root_mock.results),
        mock.call.state.DidRunStory(root_mock.results),
    ])

  def testRunStoryAndProcessErrorIfNeeded_tryUnhandlable_finallyException(self):
    root_mock = self._CreateErrorProcessingMock(method_exceptions={
        'test.Measure': Exception('foo'),
        'test.DidRunStory': Exception('bar')
    })

    with self.assertRaisesRegexp(Exception, 'foo'):
      story_runner._RunStoryAndProcessErrorIfNeeded(
          root_mock.story, root_mock.results, root_mock.state, root_mock.test)

    self.assertEquals(root_mock.method_calls, [
        mock.call.results.CreateArtifact(root_mock.story.name, 'logs'),
        mock.call.test.WillRunStory(root_mock.state.platform),
        mock.call.state.WillRunStory(root_mock.story),
        mock.call.state.CanRunStory(root_mock.story),
        mock.call.state.RunStory(root_mock.results),
        mock.call.test.Measure(root_mock.state.platform, root_mock.results),
        mock.call.state.DumpStateUponFailure(
            root_mock.story, root_mock.results),
        mock.call.results.Fail(
            'Exception raised running %s' % root_mock.story.name),
        mock.call.test.DidRunStory(root_mock.state.platform, root_mock.results),
    ])


  def testRunBenchmarkDisabledBenchmarkViaCanRunonPlatform(self):
    fake_benchmark = FakeBenchmark()
    fake_benchmark.SUPPORTED_PLATFORMS = []
    options = _GenerateBaseBrowserFinderOptions()
    tmp_path = tempfile.mkdtemp()
    try:
      options.output_dir = tmp_path
      story_runner.RunBenchmark(fake_benchmark, options)
      with open(os.path.join(tmp_path, 'results-chart.json')) as f:
        data = json.load(f)
      self.assertFalse(data['enabled'])
    finally:
      shutil.rmtree(tmp_path)

  def testRunBenchmarkDisabledBenchmark(self):
    fake_benchmark = FakeBenchmark()
    fake_benchmark.disabled = True
    options = _GenerateBaseBrowserFinderOptions()
    tmp_path = tempfile.mkdtemp()
    try:
      options.output_dir = tmp_path
      story_runner.RunBenchmark(fake_benchmark, options)
      with open(os.path.join(tmp_path, 'results-chart.json')) as f:
        data = json.load(f)
      self.assertFalse(data['enabled'])
    finally:
      shutil.rmtree(tmp_path)

  def testRunBenchmarkDisabledBenchmarkCanOverriddenByCommandLine(self):
    fake_benchmark = FakeBenchmark()
    fake_benchmark.disabled = True
    options = _GenerateBaseBrowserFinderOptions()
    options.run_disabled_tests = True
    temp_path = tempfile.mkdtemp()
    try:
      options.output_dir = temp_path
      story_runner.RunBenchmark(fake_benchmark, options)
      with open(os.path.join(temp_path, 'results-chart.json')) as f:
        data = json.load(f)
      self.assertTrue(data['enabled'])
    finally:
      shutil.rmtree(temp_path)

  def testRunBenchmark_AddsOwners_NoComponent(self):
    @benchmark.Owner(emails=['alice@chromium.org'])
    class FakeBenchmarkWithOwner(FakeBenchmark):
      def __init__(self):
        super(FakeBenchmark, self).__init__()
        self._disabled = False
        self._story_disabled = False

    fake_benchmark = FakeBenchmarkWithOwner()
    options = _GenerateBaseBrowserFinderOptions()
    options.output_formats = ['histograms']
    temp_path = tempfile.mkdtemp()
    try:
      options.output_dir = temp_path
      story_runner.RunBenchmark(fake_benchmark, options)

      with open(os.path.join(temp_path, 'histograms.json')) as f:
        data = json.load(f)

      hs = histogram_set.HistogramSet()
      hs.ImportDicts(data)

      generic_diagnostics = hs.GetSharedDiagnosticsOfType(
          generic_set.GenericSet)

      self.assertGreater(len(generic_diagnostics), 0)

      generic_diagnostics_values = [
          list(diagnostic) for diagnostic in generic_diagnostics]

      self.assertIn(['alice@chromium.org'], generic_diagnostics_values)

    finally:
      shutil.rmtree(temp_path)

  def testRunBenchmark_AddsComponent(self):
    @benchmark.Owner(emails=['alice@chromium.org', 'bob@chromium.org'],
                     component='fooBar')
    class FakeBenchmarkWithOwner(FakeBenchmark):
      def __init__(self):
        super(FakeBenchmark, self).__init__()
        self._disabled = False
        self._story_disabled = False

    fake_benchmark = FakeBenchmarkWithOwner()
    options = _GenerateBaseBrowserFinderOptions()
    options.output_formats = ['histograms']
    temp_path = tempfile.mkdtemp()
    try:
      options.output_dir = temp_path
      story_runner.RunBenchmark(fake_benchmark, options)

      with open(os.path.join(temp_path, 'histograms.json')) as f:
        data = json.load(f)

      hs = histogram_set.HistogramSet()
      hs.ImportDicts(data)

      generic_diagnostics = hs.GetSharedDiagnosticsOfType(
          generic_set.GenericSet)

      self.assertGreater(len(generic_diagnostics), 0)

      generic_diagnostics_values = [
          list(diagnostic) for diagnostic in generic_diagnostics]

      self.assertIn(['fooBar'], generic_diagnostics_values)
      self.assertIn(['alice@chromium.org', 'bob@chromium.org'],
                    generic_diagnostics_values)

    finally:
      shutil.rmtree(temp_path)

  def testRunBenchmark_AddsDocumentationUrl(self):
    @benchmark.Owner(emails=['bob@chromium.org'],
                     documentation_url='https://darth.vader')
    class FakeBenchmarkWithOwner(FakeBenchmark):
      def __init__(self):
        super(FakeBenchmark, self).__init__()
        self._disabled = False
        self._story_disabled = False

    fake_benchmark = FakeBenchmarkWithOwner()
    options = _GenerateBaseBrowserFinderOptions()
    options.output_formats = ['histograms']
    temp_path = tempfile.mkdtemp()
    try:
      options.output_dir = temp_path
      story_runner.RunBenchmark(fake_benchmark, options)

      with open(os.path.join(temp_path, 'histograms.json')) as f:
        data = json.load(f)

      hs = histogram_set.HistogramSet()
      hs.ImportDicts(data)

      generic_diagnostics = hs.GetSharedDiagnosticsOfType(
          generic_set.GenericSet)

      self.assertGreater(len(generic_diagnostics), 0)

      generic_diagnostics_values = [
          list(diagnostic) for diagnostic in generic_diagnostics]

      self.assertIn([['Benchmark documentation link', 'https://darth.vader']],
                    generic_diagnostics_values)
      self.assertIn(['bob@chromium.org'],
                    generic_diagnostics_values)

    finally:
      shutil.rmtree(temp_path)

  def testRunBenchmarkStoryTimeDuration(self):
    class FakeBenchmarkWithStories(FakeBenchmark):
      def __init__(self):
        super(FakeBenchmarkWithStories, self).__init__()

      @classmethod
      def Name(cls):
        return 'fake_with_stories'

      def page_set(self):
        number_stories = 3
        story_set = story_module.StorySet()
        for i in xrange(number_stories):
          story_set.AddStory(DummyPage(story_set, name='story_%d' % i))
        return story_set

    fake_benchmark = FakeBenchmarkWithStories()
    options = _GenerateBaseBrowserFinderOptions()
    options.output_formats = ['json-test-results']
    options.pageset_repeat = 2

    tmp_path = tempfile.mkdtemp()

    with mock.patch('telemetry.internal.story_runner.time.time') as time_patch:
      time_patch.side_effect = itertools.count()
      try:
        options.output_dir = tmp_path
        story_runner.RunBenchmark(fake_benchmark, options)
        with open(os.path.join(tmp_path, 'test-results.json')) as f:
          json_results = json.load(f)
          for fake_benchmark in json_results['tests']:
            stories = json_results['tests'][fake_benchmark]
            for story in stories:
              result = stories[story]
              times = result['times']
              self.assertEqual(len(times), 2, times)
              for t in times:
                self.assertGreater(t, 1)
      finally:
        shutil.rmtree(tmp_path)

  def testRunBenchmarkDisabledStoryWithBadName(self):
    fake_benchmark = FakeBenchmark()
    fake_benchmark.story_disabled = True
    options = _GenerateBaseBrowserFinderOptions()
    tmp_path = tempfile.mkdtemp()
    try:
      options.output_dir = tmp_path
      rc = story_runner.RunBenchmark(fake_benchmark, options)
      # Test should return 0 since only error messages are logged.
      self.assertEqual(rc, 0)
    finally:
      shutil.rmtree(tmp_path)

  def testRunBenchmark_TooManyValues(self):
    self.StubOutExceptionFormatting()
    story_set = story_module.StorySet()
    story_set.AddStory(DummyLocalStory(TestSharedPageState, name='story'))
    story_runner.Run(
        _Measurement(), story_set, self.options, self.results, max_num_values=0)
    self.assertTrue(self.results.had_failures)
    self.assertEquals(0, GetNumberOfSuccessfulPageRuns(self.results))
    self.assertIn('Too many values: 1 > 0', self.fake_stdout.getvalue())

  def testRunBenchmarkReturnCodeSuccessfulRun(self):

    class DoNothingSharedState(TestSharedState):
      def RunStory(self, results):
        pass

    class TestBenchmark(benchmark.Benchmark):
      test = DummyTest
      def CreateStorySet(self, options):
        story_set = story_module.StorySet()
        story_set.AddStory(page_module.Page(
            'http://foo', name='foo',
            shared_page_state_class=DoNothingSharedState))
        story_set.AddStory(page_module.Page(
            'http://bar', name='bar',
            shared_page_state_class=DoNothingSharedState))
        return story_set

    sucessful_benchmark = TestBenchmark()
    options = _GenerateBaseBrowserFinderOptions()
    options.output_dir = '/does/not/exist'
    options.output_formats = ['none']
    return_code = story_runner.RunBenchmark(sucessful_benchmark, options)
    self.assertEquals(0, return_code)

  def testRunBenchmarkReturnCodeCaughtException(self):

    class StoryFailureSharedState(TestSharedState):
      def RunStory(self, results):
        raise exceptions.AppCrashException()

    class TestBenchmark(benchmark.Benchmark):
      test = DummyTest
      def CreateStorySet(self, options):
        story_set = story_module.StorySet()
        story_set.AddStory(page_module.Page(
            'http://foo', name='foo',
            shared_page_state_class=StoryFailureSharedState))
        story_set.AddStory(page_module.Page(
            'http://bar', name='bar',
            shared_page_state_class=StoryFailureSharedState))
        return story_set

    story_failure_benchmark = TestBenchmark()
    options = _GenerateBaseBrowserFinderOptions()
    options.output_dir = '/does/not/exist'
    options.output_formats = ['none']
    return_code = story_runner.RunBenchmark(story_failure_benchmark, options)
    self.assertEquals(1, return_code)

  def testRunBenchmarkReturnCodeUnCaughtException(self):
    class UnhandledFailureSharedState(TestSharedState):
      def RunStory(self, results):
        raise MemoryError('Unexpected exception')

    class TestBenchmark(benchmark.Benchmark):
      test = DummyTest
      def CreateStorySet(self, options):
        story_set = story_module.StorySet()
        story_set.AddStory(page_module.Page(
            'http://foo', name='foo',
            shared_page_state_class=UnhandledFailureSharedState))
        story_set.AddStory(page_module.Page(
            'http://bar', name='bar',
            shared_page_state_class=UnhandledFailureSharedState))
        return story_set

    unhandled_failure_benchmark = TestBenchmark()
    options = _GenerateBaseBrowserFinderOptions()
    options.output_dir = '/does/not/exist'
    options.output_formats = ['none']
    return_code = story_runner.RunBenchmark(
        unhandled_failure_benchmark, options)
    self.assertEquals(2, return_code)

  def testDownloadMinimalServingDirs(self):
    foo_page = page_module.Page(
        'file://foo/foo', name='foo', tags=['foo'],
        shared_page_state_class=FooStoryState)
    bar_page = page_module.Page(
        'file://bar/bar', name='bar', tags=['bar'],
        shared_page_state_class=FooStoryState)
    bucket = cloud_storage.PUBLIC_BUCKET

    class TestBenchmark(benchmark.Benchmark):
      test = DummyTest
      def CreateStorySet(self, options):
        story_set = story_module.StorySet(cloud_storage_bucket=bucket)
        story_set.AddStory(foo_page)
        story_set.AddStory(bar_page)
        return story_set

    def options_callback(options):
      options.story_tag_filter = 'foo'

    test_benchmark = TestBenchmark()
    options = _GenerateBaseBrowserFinderOptions(options_callback)
    options.output_dir = '/does/not/exist'
    options.output_formats = ['none']
    patch_method = 'py_utils.cloud_storage.GetFilesInDirectoryIfChanged'
    with mock.patch(patch_method) as cloud_patch:
      story_runner.RunBenchmark(test_benchmark, options)
      # Foo is the only included story serving dir.
      self.assertEqual(cloud_patch.call_count, 1)
      cloud_patch.assert_called_once_with(foo_page.serving_dir, bucket)


class BenchmarkJsonResultsTest(unittest.TestCase):

  def setUp(self):
    self._temp_dir = tempfile.mkdtemp()
    self._options = _GenerateBaseBrowserFinderOptions()
    self._options.suppress_gtest_report = True
    self._options.output_formats = ['json-test-results']
    self._options.output_dir = self._temp_dir

  def tearDown(self):
    shutil.rmtree(self._temp_dir)

  def testArtifactLogsContainHandleableException(self):

    class StoryFailureSharedState(TestSharedState):
      def RunStory(self, results):
        logging.warning('This will fail gracefully')
        raise exceptions.TimeoutException('karma!')

    class TestBenchmark(benchmark.Benchmark):
      test = DummyTest

      @classmethod
      def Name(cls):
        return 'TestBenchmark'

      def CreateStorySet(self, options):
        story_set = story_module.StorySet()
        story_set.AddStory(page_module.Page(
            'http://foo', name='foo',
            shared_page_state_class=StoryFailureSharedState))
        story_set.AddStory(page_module.Page(
            'http://bar', name='bar',
            shared_page_state_class=StoryFailureSharedState))
        return story_set

    story_failure_benchmark = TestBenchmark()
    return_code = story_runner.RunBenchmark(
        story_failure_benchmark, self._options)
    self.assertEquals(1, return_code)
    json_data = {}
    with open(os.path.join(self._temp_dir, 'test-results.json')) as f:
      json_data = json.load(f)
    foo_artifacts = json_data['tests']['TestBenchmark']['foo']['artifacts']
    foo_artifact_log_path = os.path.join(
        self._temp_dir, foo_artifacts['logs'][0])
    with open(foo_artifact_log_path) as f:
      foo_log = f.read()

    self.assertIn('Handleable error', foo_log)

    # Ensure that foo_log contains the warning log message.
    self.assertIn('This will fail gracefully', foo_log)

    # Also the python crash stack.
    self.assertIn("raise exceptions.TimeoutException('karma!')", foo_log)

  def testArtifactLogsContainUnhandleableException(self):
    class UnhandledFailureSharedState(TestSharedState):
      def RunStory(self, results):
        logging.warning('This will fail badly')
        raise MemoryError('this is a fatal exception')

    class TestBenchmark(benchmark.Benchmark):
      test = DummyTest

      @classmethod
      def Name(cls):
        return 'TestBenchmark'

      def CreateStorySet(self, options):
        story_set = story_module.StorySet()
        story_set.AddStory(page_module.Page(
            'http://foo', name='foo',
            shared_page_state_class=UnhandledFailureSharedState))
        story_set.AddStory(page_module.Page(
            'http://bar', name='bar',
            shared_page_state_class=UnhandledFailureSharedState))
        return story_set

    unhandled_failure_benchmark = TestBenchmark()
    return_code = story_runner.RunBenchmark(
        unhandled_failure_benchmark, self._options)
    self.assertEquals(2, return_code)

    json_data = {}
    with open(os.path.join(self._temp_dir, 'test-results.json')) as f:
      json_data = json.load(f)

    foo_artifacts = json_data['tests']['TestBenchmark']['foo']['artifacts']
    foo_artifact_log_path = os.path.join(
        self._temp_dir, foo_artifacts['logs'][0])
    with open(foo_artifact_log_path) as f:
      foo_log = f.read()

    self.assertIn('Unhandleable error', foo_log)

    # Ensure that foo_log contains the warning log message.
    self.assertIn('This will fail badly', foo_log)

    # Also the python crash stack.
    self.assertIn('MemoryError: this is a fatal exception', foo_log)
    self.assertIn("raise MemoryError('this is a fatal exception')",
                  foo_log)

    # Assert that the second story got written as a SKIP as it failed
    # to run because of the exception.
    bar_log = json_data['tests']['TestBenchmark']['bar']
    self.assertEquals(bar_log['expected'], 'PASS')
    self.assertEquals(bar_log['actual'], 'SKIP')

  def testUnexpectedSkipsWithFiltering(self):
    class UnhandledFailureSharedState(TestSharedState):
      def RunStory(self, results):
        if results.current_page.name in stories_to_crash:
          raise MemoryError('this is an unexpected exception')

    class TestBenchmark(benchmark.Benchmark):
      test = DummyTest

      @classmethod
      def Name(cls):
        return 'TestBenchmark'

      def CreateStorySet(self, options):
        story_set = story_module.StorySet()
        for i in range(50):
          story_set.AddStory(page_module.Page(
              'http://foo_%s' % i, name='story_%s' % i,
              shared_page_state_class=UnhandledFailureSharedState))
        return story_set

    # Set up the test so that it throws unexpected crashes from any story
    # between story 30 to story 50.
    # Also set the filtering to only run from story 10 --> story 40
    stories_to_crash = set('story_%s' % i for i in range(30, 50))

    def options_callback(options):
      options.story_shard_begin_index = 10
      options.story_shard_end_index = 41

    options = _GenerateBaseBrowserFinderOptions(options_callback)
    options.suppress_gtest_report = True
    options.output_formats = ['json-test-results']
    options.output_dir = self._temp_dir

    unhandled_failure_benchmark = TestBenchmark()
    return_code = story_runner.RunBenchmark(
        unhandled_failure_benchmark, self._options)
    self.assertEquals(2, return_code)

    # The results should contain entries of story 10 --> story 40. Of those
    # entries, story 31's actual result is 'FAIL' and
    # stories from 31 to 40 will shows 'SKIP'.
    json_data = {}
    with open(os.path.join(self._temp_dir, 'test-results.json')) as f:
      json_data = json.load(f)
    stories = json_data['tests']['TestBenchmark']
    self.assertEquals(len(stories.keys()), 31)

    for i in range(10, 30):
      self.assertEquals(stories['story_%s' % i]['actual'], 'PASS')

    self.assertEquals(stories['story_30']['actual'], 'FAIL')

    for i in range(31, 41):
      self.assertEquals(stories['story_%s' % i]['actual'], 'SKIP')
