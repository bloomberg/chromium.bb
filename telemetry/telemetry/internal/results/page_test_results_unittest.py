# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import codecs
import json
import os
import shutil
import sys
import tempfile
import unittest

import mock

from telemetry import story
from telemetry.core import exceptions
from telemetry.internal.results import chart_json_output_formatter
from telemetry.internal.results import histogram_set_json_output_formatter
from telemetry.internal.results import html_output_formatter
from telemetry.internal.results import page_test_results
from telemetry.internal.results import results_options
from telemetry.internal.results import results_processor
from telemetry import page as page_module
from tracing.trace_data import trace_data
from tracing.value import histogram as histogram_module
from tracing.value import histogram_set
from tracing.value.diagnostics import diagnostic
from tracing.value.diagnostics import generic_set
from tracing.value.diagnostics import reserved_infos


def _CreateException():
  try:
    raise exceptions.IntentionalException
  except Exception: # pylint: disable=broad-except
    return sys.exc_info()


class PageTestResultsTest(unittest.TestCase):
  def setUp(self):
    story_set = story.StorySet()
    story_set.AddStory(page_module.Page("http://www.foo.com/", story_set,
                                        name='http://www.foo.com/'))
    story_set.AddStory(page_module.Page("http://www.bar.com/", story_set,
                                        name='http://www.bar.com/'))
    story_set.AddStory(page_module.Page("http://www.baz.com/", story_set,
                                        name='http://www.baz.com/'))
    self.story_set = story_set
    self._output_dir = tempfile.mkdtemp()
    self._time_module = mock.patch(
        'telemetry.internal.results.page_test_results.time').start()
    self._time_module.time.return_value = 0

  def tearDown(self):
    shutil.rmtree(self._output_dir)
    mock.patch.stopall()

  @property
  def pages(self):
    return self.story_set.stories

  @property
  def mock_time(self):
    return self._time_module.time

  @property
  def intermediate_dir(self):
    return os.path.join(self._output_dir, 'artifacts', 'test_run')

  def CreateResults(self, **kwargs):
    kwargs.setdefault('output_dir', self._output_dir)
    kwargs.setdefault('intermediate_dir', self.intermediate_dir)
    return page_test_results.PageTestResults(**kwargs)

  def GetResultRecords(self):
    results_file = os.path.join(
        self.intermediate_dir, page_test_results.TELEMETRY_RESULTS)
    with open(results_file) as f:
      return [json.loads(line) for line in f]

  def testFailures(self):
    with self.CreateResults() as results:
      results.WillRunPage(self.pages[0])
      results.Fail(_CreateException())
      results.DidRunPage(self.pages[0])

      results.WillRunPage(self.pages[1])
      results.DidRunPage(self.pages[1])

    all_story_runs = list(results.IterStoryRuns())
    self.assertEqual(len(all_story_runs), 2)
    self.assertTrue(results.had_failures)
    self.assertTrue(all_story_runs[0].failed)
    self.assertTrue(all_story_runs[1].ok)

  def testSkips(self):
    with self.CreateResults() as results:
      results.WillRunPage(self.pages[0])
      results.Skip('testing reason')
      results.DidRunPage(self.pages[0])

      results.WillRunPage(self.pages[1])
      results.DidRunPage(self.pages[1])

    all_story_runs = list(results.IterStoryRuns())
    self.assertTrue(all_story_runs[0].skipped)
    self.assertEqual(self.pages[0], all_story_runs[0].story)

    self.assertEqual(2, len(all_story_runs))
    self.assertTrue(results.had_skips)
    self.assertTrue(all_story_runs[0].skipped)
    self.assertTrue(all_story_runs[1].ok)

  def testBenchmarkInterruption(self):
    reason = 'This is a reason'
    with self.CreateResults() as results:
      self.assertIsNone(results.benchmark_interruption)
      self.assertFalse(results.benchmark_interrupted)
      results.InterruptBenchmark(reason)

    self.assertEqual(results.benchmark_interruption, reason)
    self.assertTrue(results.benchmark_interrupted)

  def testUncaughtExceptionInterruptsBenchmark(self):
    with self.assertRaises(ValueError):
      with self.CreateResults() as results:
        results.WillRunPage(self.pages[0])
        raise ValueError('expected error')

    self.assertTrue(results.benchmark_interrupted)
    self.assertEqual(results.benchmark_interruption,
                     "ValueError('expected error',)")

  def testPassesNoSkips(self):
    with self.CreateResults() as results:
      results.WillRunPage(self.pages[0])
      results.Fail(_CreateException())
      results.DidRunPage(self.pages[0])

      results.WillRunPage(self.pages[1])
      results.DidRunPage(self.pages[1])

      results.WillRunPage(self.pages[2])
      results.Skip('testing reason')
      results.DidRunPage(self.pages[2])

    all_story_runs = list(results.IterStoryRuns())
    self.assertEqual(3, len(all_story_runs))
    self.assertTrue(all_story_runs[0].failed)
    self.assertTrue(all_story_runs[1].ok)
    self.assertTrue(all_story_runs[2].skipped)

  def testAddMeasurementAsScalar(self):
    with self.CreateResults() as results:
      results.WillRunPage(self.pages[0])
      results.AddMeasurement('a', 'seconds', 3)
      results.DidRunPage(self.pages[0])

    test_results = results_options.ReadIntermediateResults(
        self.intermediate_dir)['testResults']
    self.assertTrue(len(test_results), 1)
    measurements = results_options.ReadMeasurements(test_results[0])
    self.assertEqual(measurements, {'a': {'unit': 'seconds', 'samples': [3]}})

  def testAddMeasurementAsList(self):
    with self.CreateResults() as results:
      results.WillRunPage(self.pages[0])
      results.AddMeasurement('a', 'seconds', [1, 2, 3])
      results.DidRunPage(self.pages[0])

    test_results = results_options.ReadIntermediateResults(
        self.intermediate_dir)['testResults']
    self.assertTrue(len(test_results), 1)
    measurements = results_options.ReadMeasurements(test_results[0])
    self.assertEqual(measurements,
                     {'a': {'unit': 'seconds', 'samples': [1, 2, 3]}})

  def testAddMeasurementAsHistogram(self):
    # TODO(999484): Temporary test while we migrate benchmarks writing "ad hoc"
    # histograms to directly use measurements instead.
    with self.CreateResults() as results:
      results.WillRunPage(self.pages[0])
      results.AddHistogram(histogram_module.Histogram.Create(
          'a', 'ms', [1000, 2000, 3000]))
      results.DidRunPage(self.pages[0])

    test_results = results_options.ReadIntermediateResults(
        self.intermediate_dir)['testResults']
    self.assertTrue(len(test_results), 1)
    measurements = results_options.ReadMeasurements(test_results[0])
    self.assertEqual(measurements,
                     {'a': {'unit': 'ms', 'samples': [1000, 2000, 3000]}})

  def testAddMeasurementWithStoryGroupingKeys(self):
    with self.CreateResults() as results:
      self.pages[0].grouping_keys['foo'] = 'bar'
      self.pages[0].grouping_keys['answer'] = '42'
      results.WillRunPage(self.pages[0])
      results.AddMeasurement('a', 'seconds', 3)
      results.DidRunPage(self.pages[0])

    values = list(results.IterAllLegacyValues())
    self.assertEqual(1, len(values))
    self.assertEqual(values[0].grouping_label, '42_bar')

  def testNonNumericMeasurementIsInvalid(self):
    with self.CreateResults() as results:
      results.WillRunPage(self.pages[0])
      with self.assertRaises(AssertionError):
        results.AddMeasurement('url', 'string', 'foo')
      results.DidRunPage(self.pages[0])

  def testMeasurementUnitChangeRaises(self):
    with self.CreateResults() as results:
      results.WillRunPage(self.pages[0])
      results.AddMeasurement('a', 'seconds', 3)
      results.DidRunPage(self.pages[0])

      results.WillRunPage(self.pages[1])
      with self.assertRaises(AssertionError):
        results.AddMeasurement('a', 'foobgrobbers', 3)
      results.DidRunPage(self.pages[1])

  def testNoSuccessesWhenAllPagesFailOrSkip(self):
    with self.CreateResults() as results:
      results.WillRunPage(self.pages[0])
      results.Fail('message')
      results.DidRunPage(self.pages[0])

      results.WillRunPage(self.pages[1])
      results.Skip('message')
      results.DidRunPage(self.pages[1])

    self.assertFalse(results.had_successes)

  def testIterAllLegacyValuesForSuccessfulPages(self):
    with self.CreateResults() as results:
      results.WillRunPage(self.pages[0])
      results.AddMeasurement('a', 'seconds', 3)
      results.DidRunPage(self.pages[0])

      results.WillRunPage(self.pages[1])
      results.AddMeasurement('a', 'seconds', 3)
      results.DidRunPage(self.pages[1])

      results.WillRunPage(self.pages[2])
      results.AddMeasurement('a', 'seconds', 3)
      results.DidRunPage(self.pages[2])

    expected = [
        (v.page, v.name, v.units, v.value)
        for v in results.IterAllLegacyValues()]
    self.assertEqual([
        (self.pages[0], 'a', 'seconds', 3),
        (self.pages[1], 'a', 'seconds', 3),
        (self.pages[2], 'a', 'seconds', 3)], expected)

  def testAddTraces(self):
    with self.CreateResults() as results:
      results.WillRunPage(self.pages[0])
      results.AddTraces(trace_data.CreateTestTrace(1))
      results.DidRunPage(self.pages[0])

      results.WillRunPage(self.pages[1])
      results.AddTraces(trace_data.CreateTestTrace(2))
      results.DidRunPage(self.pages[1])

    runs = list(results.IterRunsWithTraces())
    self.assertEqual(2, len(runs))

  def testAddTracesForSamePage(self):
    with self.CreateResults() as results:
      results.WillRunPage(self.pages[0])
      results.AddTraces(trace_data.CreateTestTrace(1))
      results.AddTraces(trace_data.CreateTestTrace(2))
      results.DidRunPage(self.pages[0])

    runs = list(results.IterRunsWithTraces())
    self.assertEqual(1, len(runs))

  def testOutputEmptyResults_ChartJSON(self):
    output_file = os.path.join(self._output_dir, 'chart.json')
    with open(output_file, 'w') as stream:
      formatter = chart_json_output_formatter.ChartJsonOutputFormatter(stream)
      with self.CreateResults(
          output_formatters=[formatter],
          benchmark_name='fake_benchmark_name'):
        pass

    with open(output_file) as f:
      chartjson_output = json.load(f)

    self.assertFalse(chartjson_output['enabled'])
    self.assertEqual(chartjson_output['benchmark_name'], 'fake_benchmark_name')

  def testOutputEmptyResults_HTML(self):
    output_file = os.path.join(self._output_dir, 'results.html')
    with codecs.open(output_file, 'w', encoding='utf-8') as stream:
      formatter = html_output_formatter.HtmlOutputFormatter(stream)
      with self.CreateResults(output_formatters=[formatter]):
        pass

    self.assertGreater(os.stat(output_file).st_size, 0)

  def testOutputEmptyResults_Histograms(self):
    output_file = os.path.join(self._output_dir, 'histograms.json')
    with open(output_file, 'w') as stream:
      formatter = histogram_set_json_output_formatter.\
          HistogramSetJsonOutputFormatter(stream)
      with self.CreateResults(output_formatters=[formatter]):
        pass

    with open(output_file) as f:
      self.assertEqual(f.read(), '[]')

  def testAddMetricPageResults(self):
    hs = histogram_set.HistogramSet()
    hs.AddHistogram(histogram_module.Histogram('foo', 'count'))
    hs.AddSharedDiagnosticToAllHistograms(
        'bar', generic_set.GenericSet(['baz']))
    histogram_dicts = hs.AsDicts()

    with self.CreateResults() as results:
      results.WillRunPage(self.pages[0])
      run = results.current_story_run
      results.DidRunPage(self.pages[0])

      # Pretend we got some results by running metrics.
      results.AddMetricPageResults({
          'run': run,
          'fail': [],
          'histogram_dicts': histogram_dicts,
          'scalars': []
      })

    self.assertEqual(results.AsHistogramDicts(), histogram_dicts)

  def testAddSharedDiagnostics(self):
    with self.CreateResults(benchmark_name='benchmark_name') as results:
      results.WillRunPage(self.pages[0])
      results.DidRunPage(self.pages[0])
      results.AddSharedDiagnostics(os_name='linux')
      results.PopulateHistogramSet()

    histogram_dicts = results.AsHistogramDicts()
    self.assertEqual(1, len(histogram_dicts))

    diag = diagnostic.Diagnostic.FromDict(histogram_dicts[0])
    self.assertIsInstance(diag, generic_set.GenericSet)

  def testPopulateHistogramSet_UsesScalarValueData(self):
    with self.CreateResults() as results:
      results.WillRunPage(self.pages[0])
      results.AddMeasurement('a', 'seconds', 3)
      results.DidRunPage(self.pages[0])
      results.PopulateHistogramSet()

    hs = histogram_set.HistogramSet()
    hs.ImportDicts(results.AsHistogramDicts())
    self.assertEqual(1, len(hs))
    self.assertEqual('a', hs.GetFirstHistogram().name)

  def testPopulateHistogramSet_UsesHistogramSetData(self):
    with self.CreateResults(benchmark_name='benchmark_name') as results:
      results.WillRunPage(self.pages[0])
      results.AddHistogram(histogram_module.Histogram('foo', 'count'))
      results.DidRunPage(self.pages[0])
      results.PopulateHistogramSet()

    histogram_dicts = results.AsHistogramDicts()
    self.assertEqual(8, len(histogram_dicts))

    hs = histogram_set.HistogramSet()
    hs.ImportDicts(histogram_dicts)

    hist = hs.GetHistogramNamed('foo')
    self.assertItemsEqual(hist.diagnostics[reserved_infos.BENCHMARKS.name],
                          ['benchmark_name'])

  def testBeginFinishBenchmarkRecords(self):
    self.mock_time.side_effect = [1234567890.987]
    with self.CreateResults(benchmark_name='some benchmark',
                            benchmark_description='a description') as results:
      results.WillRunPage(self.pages[0])
      results.DidRunPage(self.pages[0])
      results.WillRunPage(self.pages[1])
      results.DidRunPage(self.pages[1])
      results.AddSharedDiagnostics(
          owners=['test'],
          bug_components=['1', '2'],
          documentation_urls=[['documentation', 'url']],
          architecture='arch',
          device_id='id',
          os_name='os',
          os_version='ver',
      )

    records = self.GetResultRecords()
    self.assertEqual(len(records), 4)  # Start, Result, Result, Finish.
    self.assertEqual(records[0], {
        'benchmarkRun': {
            'startTime': '2009-02-13T23:31:30.987000Z',
        }
    })
    self.assertEqual(records[1]['testResult']['status'], 'PASS')
    self.assertEqual(records[2]['testResult']['status'], 'PASS')
    self.assertEqual(records[3], {
        'benchmarkRun': {
            'finalized': True,
            'interrupted': False,
            'diagnostics': {
                'benchmarks': ['some benchmark'],
                'benchmarkDescriptions': ['a description'],
                'owners': ['test'],
                'bugComponents': ['1', '2'],
                'documentationLinks': [['documentation', 'url']],
                'architectures': ['arch'],
                'deviceIds': ['id'],
                'osNames': ['os'],
                'osVersions': ['ver'],
            },
        },
    })

  def testBeginFinishBenchmarkRecords_interrupted(self):
    self.mock_time.side_effect = [1234567890.987]
    with self.CreateResults(benchmark_name='some benchmark',
                            benchmark_description='a description') as results:
      results.WillRunPage(self.pages[0])
      results.Fail('fatal error')
      results.DidRunPage(self.pages[0])
      results.InterruptBenchmark('some reason')

    records = self.GetResultRecords()
    self.assertEqual(len(records), 3)  # Start, Result, Finish.
    self.assertEqual(records[0], {
        'benchmarkRun': {
            'startTime': '2009-02-13T23:31:30.987000Z',
        }
    })
    self.assertEqual(records[1]['testResult']['status'], 'FAIL')
    self.assertEqual(records[2], {
        'benchmarkRun': {
            'finalized': True,
            'interrupted': True,
            'diagnostics': {
                'benchmarks': ['some benchmark'],
                'benchmarkDescriptions': ['a description'],
            },
        }
    })

  @mock.patch('py_utils.cloud_storage.Insert')
  def testUploadArtifactsToCloud(self, cs_insert_mock):
    cs_path_name = 'https://cs_foo'
    cs_insert_mock.return_value = cs_path_name
    with self.CreateResults(upload_bucket='abc') as results:
      results.WillRunPage(self.pages[0])
      with results.CreateArtifact('screenshot.png') as screenshot1:
        pass
      results.DidRunPage(self.pages[0])

      results.WillRunPage(self.pages[1])
      with results.CreateArtifact('log.txt') as log2:
        pass
      results.DidRunPage(self.pages[1])

      results_processor.UploadArtifactsToCloud(results)

    cs_insert_mock.assert_has_calls(
        [mock.call('abc', mock.ANY, screenshot1.name),
         mock.call('abc', mock.ANY, log2.name)],
        any_order=True)

    # Assert that the path is now the cloud storage path
    for run in results.IterStoryRuns():
      for artifact in run.IterArtifacts():
        self.assertEqual(cs_path_name, artifact.url)

  @mock.patch('py_utils.cloud_storage.Insert')
  def testUploadArtifactsToCloud_withNoOpArtifact(self, _):
    with self.CreateResults(
        upload_bucket='abc', output_dir=None, intermediate_dir=None) as results:
      results.WillRunPage(self.pages[0])
      with results.CreateArtifact('screenshot.png'):
        pass
      results.DidRunPage(self.pages[0])

      results.WillRunPage(self.pages[1])
      with results.CreateArtifact('log.txt'):
        pass
      results.DidRunPage(self.pages[1])

      # Just make sure that this does not crash
      results_processor.UploadArtifactsToCloud(results)

  def testCreateArtifactsForDifferentPages(self):
    with self.CreateResults() as results:
      results.WillRunPage(self.pages[0])
      with results.CreateArtifact('log.txt') as log_file:
        log_file.write('page0\n')
      results.DidRunPage(self.pages[0])

      results.WillRunPage(self.pages[1])
      with results.CreateArtifact('log.txt') as log_file:
        log_file.write('page1\n')
      results.DidRunPage(self.pages[1])

    all_story_runs = list(results.IterStoryRuns())
    log0_path = all_story_runs[0].GetArtifact('log.txt').local_path
    with open(log0_path) as f:
      self.assertEqual(f.read(), 'page0\n')

    log1_path = all_story_runs[1].GetArtifact('log.txt').local_path
    with open(log1_path) as f:
      self.assertEqual(f.read(), 'page1\n')
