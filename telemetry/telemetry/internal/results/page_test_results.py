# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import json
import logging
import os
import posixpath
import shutil
import tempfile
import time
import traceback

from telemetry import value as value_module
from telemetry.internal.results import chart_json_output_formatter
from telemetry.internal.results import gtest_progress_reporter
from telemetry.internal.results import results_processor
from telemetry.internal.results import story_run
from telemetry.value import list_of_scalar_values
from telemetry.value import scalar

from tracing.value import convert_chart_json
from tracing.value import histogram_set
from tracing.value.diagnostics import all_diagnostics
from tracing.value.diagnostics import reserved_infos
from tracing.value.diagnostics import generic_set


TELEMETRY_RESULTS = '_telemetry_results.jsonl'
HISTOGRAM_DICTS_NAME = 'histogram_dicts.json'


class PageTestResults(object):
  def __init__(self, output_formatters=None, progress_stream=None,
               output_dir=None, intermediate_dir=None,
               benchmark_name=None, benchmark_description=None,
               upload_bucket=None, results_label=None):
    """
    Args:
      output_formatters: A list of output formatters. The output
          formatters are typically used to format the test results, such
          as CsvOutputFormatter, which output the test results as CSV.
      progress_stream: A file-like object where to write progress reports as
          stories are being run. Can be None to suppress progress reporting.
      output_dir: A string specifying the directory where to store the test
          artifacts, e.g: trace, videos, etc.
      benchmark_name: A string with the name of the currently running benchmark.
      benchmark_description: A string with a description of the currently
          running benchmark.
      upload_bucket: A string identifting a cloud storage bucket where to
          upload artifacts.
      results_label: A string that serves as an identifier for the current
          benchmark run.
    """
    super(PageTestResults, self).__init__()
    self._progress_reporter = gtest_progress_reporter.GTestProgressReporter(
        progress_stream)
    self._output_formatters = (
        output_formatters if output_formatters is not None else [])
    self._output_dir = output_dir
    self._intermediate_dir = intermediate_dir
    if intermediate_dir is None and output_dir is not None:
      self._intermediate_dir = os.path.join(output_dir, 'artifacts')
    self._upload_bucket = upload_bucket

    self._current_story_run = None
    self._all_story_runs = []
    self._all_stories = set()
    self._representative_value_for_each_value_name = {}

    self._histograms = histogram_set.HistogramSet()

    self._benchmark_name = benchmark_name or '(unknown benchmark)'
    self._benchmark_description = benchmark_description or ''

    # |_interruption| is None if the benchmark has not been interrupted.
    # Otherwise it is a string explaining the reason for the interruption.
    # Interruptions occur for unrecoverable exceptions.
    self._interruption = None
    self._results_label = results_label

    self._diagnostics = {
        reserved_infos.BENCHMARKS.name: [self.benchmark_name],
        reserved_infos.BENCHMARK_DESCRIPTIONS.name:
            [self.benchmark_description],
    }

    # If the object has been finalized, no more results can be added to it.
    self._finalized = False
    self._start_time = time.time()
    self._results_stream = None
    if self._intermediate_dir is not None:
      if not os.path.exists(self._intermediate_dir):
        os.makedirs(self._intermediate_dir)
      self._results_stream = open(
          os.path.join(self._intermediate_dir, TELEMETRY_RESULTS), 'w')
      self._RecordBenchmarkStart()

  @property
  def benchmark_name(self):
    return self._benchmark_name

  @property
  def benchmark_description(self):
    return self._benchmark_description

  @property
  def benchmark_start_us(self):
    return self._start_time * 1e6

  @property
  def benchmark_interrupted(self):
    return bool(self._interruption)

  @property
  def benchmark_interruption(self):
    """Returns a string explaining why the benchmark was interrupted."""
    return self._interruption

  @property
  def start_datetime(self):
    return datetime.datetime.utcfromtimestamp(self._start_time)

  @property
  def label(self):
    return self._results_label

  @property
  def output_dir(self):
    return self._output_dir

  @property
  def upload_bucket(self):
    return self._upload_bucket

  @property
  def finalized(self):
    return self._finalized

  def AsHistogramDicts(self):
    return self._histograms.AsDicts()

  def PopulateHistogramSet(self):
    if len(self._histograms):
      return

    # We ensure that html traces are serialized and uploaded if necessary
    results_processor.SerializeAndUploadHtmlTraces(self)

    chart_json = chart_json_output_formatter.ResultsAsChartDict(self)
    chart_json['label'] = self.label
    chart_json['benchmarkStartMs'] = self.benchmark_start_us / 1000.0

    file_descriptor, chart_json_path = tempfile.mkstemp()
    os.close(file_descriptor)
    json.dump(chart_json, file(chart_json_path, 'w'))

    vinn_result = convert_chart_json.ConvertChartJson(chart_json_path)

    os.remove(chart_json_path)

    if vinn_result.returncode != 0:
      logging.error('Error converting chart json to Histograms:\n' +
                    vinn_result.stdout)
      return []
    self._histograms.ImportDicts(json.loads(vinn_result.stdout))

  @property
  def current_page(self):
    """DEPRECATED: Use current_story instead."""
    return self.current_story

  @property
  def current_story(self):
    assert self._current_story_run, 'Not currently running test.'
    return self._current_story_run.story

  @property
  def current_story_run(self):
    return self._current_story_run

  @property
  def had_successes(self):
    """If there were any actual successes, not including skipped stories."""
    return any(run.ok for run in self._all_story_runs)

  @property
  def num_successful(self):
    """Number of successful stories."""
    return sum(1 for run in self._all_story_runs if run.ok)

  @property
  def num_expected(self):
    """Number of stories that succeeded or were expected skips."""
    return sum(1 for run in self._all_story_runs if run.is_expected)

  @property
  def had_failures(self):
    """If there where any failed stories."""
    return any(run.failed for run in self._all_story_runs)

  @property
  def num_failed(self):
    """Number of failed stories."""
    return sum(1 for run in self._all_story_runs if run.failed)

  @property
  def had_skips(self):
    """If there where any skipped stories."""
    return any(run.skipped for run in self._IterAllStoryRuns())

  @property
  def num_skipped(self):
    """Number of skipped stories."""
    return sum(1 for run in self._all_story_runs if run.skipped)

  def _IterAllStoryRuns(self):
    # TODO(crbug.com/973837): Check whether all clients can just be switched
    # to iterate over _all_story_runs directly.
    for run in self._all_story_runs:
      yield run
    if self._current_story_run:
      yield self._current_story_run

  @property
  def empty(self):
    """Whether there were any story runs."""
    return not self._all_story_runs

  def _WriteJsonLine(self, data, close=False):
    if self._results_stream is not None:
      # Use a compact encoding and sort keys to get deterministic outputs.
      self._results_stream.write(
          json.dumps(data, sort_keys=True, separators=(',', ':')) + '\n')
      if close:
        self._results_stream.close()
      else:
        self._results_stream.flush()

  def _RecordBenchmarkStart(self):
    self._WriteJsonLine({
        'benchmarkRun': {
            'startTime': self.start_datetime.isoformat() + 'Z',
        }
    })

  def _RecordBenchmarkFinish(self):
    self._WriteJsonLine({
        'benchmarkRun': {
            'finalized': self.finalized,
            'interrupted': self.benchmark_interrupted,
            'diagnostics': self._diagnostics,
        }
    }, close=True)

  def IterStoryRuns(self):
    return iter(self._all_story_runs)

  def IterAllLegacyValues(self):
    for run in self._IterAllStoryRuns():
      for value in run.values:
        yield value

  def __enter__(self):
    return self

  def __exit__(self, _, exc_value, __):
    self.Finalize(exc_value)

  def WillRunPage(self, page, story_run_index=0):
    assert not self.finalized, 'Results are finalized, cannot run more stories.'
    assert not self._current_story_run, 'Did not call DidRunPage.'
    self._current_story_run = story_run.StoryRun(
        page, test_prefix=self.benchmark_name, index=story_run_index,
        intermediate_dir=self._intermediate_dir)
    self._progress_reporter.WillRunStory(self)

  def DidRunPage(self, page):  # pylint: disable=unused-argument
    """
    Args:
      page: The current page under test.
    """
    assert self._current_story_run, 'Did not call WillRunPage.'
    self._current_story_run.Finish()
    self._progress_reporter.DidRunStory(self)
    self._all_story_runs.append(self._current_story_run)
    story = self._current_story_run.story
    self._all_stories.add(story)
    self._current_story_run = None

  def AddMetricPageResults(self, result):
    """Add results from metric computation.

    Args:
      result: A dict produced by results_processor._ComputeMetricsInPool.
    """
    self._current_story_run = result['run']
    try:
      for fail in result['fail']:
        self.Fail(fail)
      if result['histogram_dicts']:
        self._histograms.ImportDicts(result['histogram_dicts'])
        # Saving histograms as an artifact is a temporary hack to keep
        # things working while we gradually move code from Telemetry to
        # Results Processor.
        # TODO(crbug.com/981349): Remove this after metrics running is
        # implemented in Results Processor.
        with self.CreateArtifact(HISTOGRAM_DICTS_NAME) as f:
          json.dump(result['histogram_dicts'], f)
      for value in result['scalars']:
        self._AddValue(value)
    finally:
      self._current_story_run = None

  def InterruptBenchmark(self, reason):
    """Mark the benchmark as interrupted.

    Interrupted benchmarks are assumed to be stuck in some irrecoverably
    broken state.

    Note that the interruption_reason will always be the first interruption.
    This is because later interruptions may be simply additional fallout from
    the first interruption.
    """
    assert not self.finalized, 'Results are finalized, cannot interrupt.'
    assert reason, 'A reason string to interrupt must be provided.'
    logging.fatal(reason)
    self._interruption = self._interruption or reason

  def AddHistogram(self, hist):
    """DEPRECATED: New clients should use AddMeasurement instead."""
    diags = self._GetDiagnostics()
    for diag in diags.itervalues():
      self._histograms.AddSharedDiagnostic(diag)
    self._histograms.AddHistogram(hist, diags)
    self.current_story_run.AddMeasurement(
        hist.name, hist.unit, hist.sample_values, hist.description or None)

  def _GetDiagnostics(self):
    """Get benchmark and current story details as histogram diagnostics.

    Diagnostics of the DateRange type are converted to milliseconds.
    """
    return dict(_WrapDiagnostics([
        (reserved_infos.BENCHMARKS, self.benchmark_name),
        (reserved_infos.BENCHMARK_START, self.benchmark_start_us / 1e3),
        (reserved_infos.BENCHMARK_DESCRIPTIONS, self.benchmark_description),
        (reserved_infos.LABELS, self.label),
        (reserved_infos.HAD_FAILURES, self.current_story_run.failed),
        (reserved_infos.STORIES, self.current_story.name),
        (reserved_infos.STORY_TAGS, self.current_story.GetStoryTagsList()),
        (reserved_infos.STORYSET_REPEATS, self.current_story_run.index),
        (reserved_infos.TRACE_START, self.current_story_run.start_us / 1e3),
    ]))

  def AddMeasurement(self, name, unit, samples, description=None):
    """Record a measurement of the currently running story.

    Measurements are numeric values obtained directly by a benchmark while
    a story is running (e.g. by evaluating some JavaScript on the page or
    calling some platform methods). These are appended together with
    measurements obtained by running metrics on collected traces (if any)
    after the benchmark run has finished.

    TODO(crbug.com/999484): Currently measurements are stored as legacy
    Telemetry values. This will allow clients to switch to this new API while
    preserving the existing behavior. When no more clients create legacy
    values on their own, the implementation details below can be changed.

    Args:
      name: A string with the name of the measurement (e.g. 'score', 'runtime',
        etc).
      unit: A string specifying the unit used for measurements (e.g. 'ms',
        'count', etc).
      samples: Either a single numeric value or a list of numeric values to
        record as part of this measurement.
      description: An optional string with a short human readable description
        of the measurement.
    """
    assert self._current_story_run, 'Not currently running a story.'
    value = _MeasurementToValue(
        self.current_story, name, unit, samples, description)
    self._AddValue(value)
    self.current_story_run.AddMeasurement(name, unit, samples, description)

  def _AddValue(self, value):
    """DEPRECATED: Use AddMeasurement instead."""
    assert self._current_story_run, 'Not currently running a story.'
    self._ValidateValue(value)
    self._current_story_run.AddLegacyValue(value)

  def AddSharedDiagnostics(self,
                           owners=None,
                           bug_components=None,
                           documentation_urls=None,
                           architecture=None,
                           device_id=None,
                           os_name=None,
                           os_version=None):
    """Add diagnostics to all histograms and save it to intermediate results."""
    diag_values = [
        (reserved_infos.OWNERS, owners),
        (reserved_infos.BUG_COMPONENTS, bug_components),
        (reserved_infos.DOCUMENTATION_URLS, documentation_urls),
        (reserved_infos.ARCHITECTURES, architecture),
        (reserved_infos.DEVICE_IDS, device_id),
        (reserved_infos.OS_NAMES, os_name),
        (reserved_infos.OS_VERSIONS, os_version),
    ]

    for name, value in _WrapDiagnostics(diag_values):
      self._histograms.AddSharedDiagnosticToAllHistograms(name, value)
      # Results Processor supports only GenericSet diagnostics for now.
      assert isinstance(value, generic_set.GenericSet)
      self._diagnostics[name] = list(value)

  def Fail(self, failure):
    """Mark the current story run as failed.

    This method will print a GTest-style failure annotation and mark the
    current story run as failed.

    Args:
      failure: A string or exc_info describing the reason for failure.
    """
    # TODO(#4258): Relax this assertion.
    assert self._current_story_run, 'Not currently running test.'
    if isinstance(failure, basestring):
      failure_str = 'Failure recorded for page %s: %s' % (
          self._current_story_run.story.name, failure)
    else:
      failure_str = ''.join(traceback.format_exception(*failure))
    logging.error(failure_str)
    self._current_story_run.SetFailed(failure_str)

  def Skip(self, reason, is_expected=True):
    assert self._current_story_run, 'Not currently running test.'
    self._current_story_run.Skip(reason, is_expected)

  def CreateArtifact(self, name):
    assert self._current_story_run, 'Not currently running test.'
    return self._current_story_run.CreateArtifact(name)

  def CaptureArtifact(self, name):
    assert self._current_story_run, 'Not currently running test.'
    return self._current_story_run.CaptureArtifact(name)

  def AddTraces(self, traces, tbm_metrics=None):
    """Associate some recorded traces with the current story run.

    Args:
      traces: A TraceDataBuilder object with traces recorded from all
        tracing agents.
      tbm_metrics: Optional list of TBMv2 metrics to be computed from the
        input traces.
    """
    assert self._current_story_run, 'Not currently running test.'
    for part, filename in traces.IterTraceParts():
      artifact_name = posixpath.join('trace', part, os.path.basename(filename))
      with self.CaptureArtifact(artifact_name) as artifact_path:
        shutil.copy(filename, artifact_path)
    if tbm_metrics:
      self._current_story_run.SetTbmMetrics(tbm_metrics)

  def _ValidateValue(self, value):
    assert isinstance(value, value_module.Value)
    if value.name not in self._representative_value_for_each_value_name:
      self._representative_value_for_each_value_name[value.name] = value
    representative_value = self._representative_value_for_each_value_name[
        value.name]
    assert value.IsMergableWith(representative_value)

  def Finalize(self, exc_value=None):
    """Finalize this object to prevent more results from being recorded.

    When progress reporting is enabled, also prints a final summary with the
    number of story runs that suceeded, failed, or were skipped.

    It's fine to call this method multiple times, later calls are just a no-op.
    """
    if self.finalized:
      return

    if exc_value is not None:
      self.InterruptBenchmark(repr(exc_value))
      self._current_story_run = None
    else:
      assert self._current_story_run is None, (
          'Cannot finalize while stories are still running.')

    self._finalized = True
    self._progress_reporter.DidFinishAllStories(self)

    # Make sure that html traces are recorded as artifacts.
    # TODO(crbug.com/981349): Remove this after trace serialization is
    # implemented in Results Processor.
    results_processor.SerializeAndUploadHtmlTraces(self)

    # TODO(crbug.com/981349): Ideally we want to write results for each story
    # run individually at DidRunPage when the story finished executing. For
    # now, however, we need to wait until this point after html traces have
    # been serialized, uploaded, and recorded as artifacts for them to show up
    # in intermediate results. When both trace serialization and artifact
    # upload are handled by results_processor, remove the for-loop from here
    # and write results instead at the end of each story run.
    for run in self._all_story_runs:
      self._WriteJsonLine(run.AsDict())
    self._RecordBenchmarkFinish()

    for output_formatter in self._output_formatters:
      output_formatter.Format(self)
      output_formatter.PrintViewResults()
      output_formatter.output_stream.close()

  def FindAllPageSpecificValuesNamed(self, value_name):
    """DEPRECATED: New benchmarks should not use legacy values."""
    return [v for v in self.IterAllLegacyValues() if v.name == value_name]

  def IterRunsWithTraces(self):
    for run in self._IterAllStoryRuns():
      if run.HasArtifactsInDir('trace/'):
        yield run


def _MeasurementToValue(story, name, unit, samples, description):
  if isinstance(samples, list):
    return list_of_scalar_values.ListOfScalarValues(
        story, name=name, units=unit, values=samples, description=description)
  else:
    return scalar.ScalarValue(
        story, name=name, units=unit, value=samples, description=description)


def _WrapDiagnostics(info_value_pairs):
  """Wrap diagnostic values in corresponding Diagnostics classes.

  Args:
    info_value_pairs: any iterable of pairs (info, value), where info is one
        of reserved infos defined in tracing.value.diagnostics.reserved_infos,
        and value can be any json-serializable object.

  Returns:
    An iterator over pairs (diagnostic name, diagnostic value).
  """
  for info, value in info_value_pairs:
    if value is None or value == []:
      continue
    if info.type == 'GenericSet' and not isinstance(value, list):
      value = [value]
    diag_class = all_diagnostics.GetDiagnosticClassForName(info.type)
    yield info.name, diag_class(value)
