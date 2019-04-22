# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import codecs
import optparse
import os
import sys
import time

from py_utils import cloud_storage  # pylint: disable=import-error

from telemetry.core import util
from telemetry.internal.results import artifact_results
from telemetry.internal.results import chart_json_output_formatter
from telemetry.internal.results import csv_output_formatter
from telemetry.internal.results import gtest_progress_reporter
from telemetry.internal.results import histogram_set_json_output_formatter
from telemetry.internal.results import html_output_formatter
from telemetry.internal.results import json_3_output_formatter
from telemetry.internal.results import page_test_results
from telemetry.internal.results import progress_reporter

# Allowed output formats. The default is the first item in the list.

_OUTPUT_FORMAT_CHOICES = (
    'chartjson',
    'csv',
    'gtest',
    'histograms',
    'html',
    'json-test-results',
    'none',
    )

_DEFAULT_OUTPUT_FORMAT = 'html'


# Filenames to use for given output formats.
_OUTPUT_FILENAME_LOOKUP = {
    'chartjson': 'results-chart.json',
    'csv': 'results.csv',
    'histograms': 'histograms.json',
    'html': 'results.html',
    'json-test-results': 'test-results.json',
}


def AddResultsOptions(parser):
  group = optparse.OptionGroup(parser, 'Results options')
  group.add_option(
      '--output-format',
      action='append',
      dest='output_formats',
      choices=_OUTPUT_FORMAT_CHOICES,
      default=[],
      help='Output format. Defaults to "%%default". '
      'Can be %s.' % ', '.join(_OUTPUT_FORMAT_CHOICES))
  group.add_option(
      '--output-dir',
      default=util.GetBaseDir(),
      help='Where to save output data after the run.')
  group.add_option(
      '--output-trace-tag',
      default='',
      help='Append a tag to the key of each result trace. Use '
      'with html output formats.')
  group.add_option(
      '--reset-results', action='store_true', help='Delete all stored results.')
  group.add_option(
      '--upload-results',
      action='store_true',
      help='Upload the results to cloud storage.')
  group.add_option(
      '--upload-bucket',
      default='output',
      help='Storage bucket to use for the uploaded results. ' +
      'Defaults to output bucket. Supported values are: ' +
      ', '.join(cloud_storage.BUCKET_ALIAS_NAMES) +
      '; or a valid cloud storage bucket name.')
  group.add_option(
      '--results-label',
      default=None,
      help='Optional label to use for the results of a run .')
  group.add_option(
      '--suppress_gtest_report',
      default=False,
      help='Whether to suppress GTest progress report.')
  parser.add_option_group(group)


def ProcessCommandLineArgs(args):
  try:
    os.makedirs(args.output_dir)
  except OSError:
    # Do nothing if the output directory already exists. Existing files will
    # get overwritten.
    pass

  args.output_dir = os.path.expanduser(args.output_dir)


def _GetOutputStream(output_format, output_dir):
  assert output_format in _OUTPUT_FORMAT_CHOICES, 'Must specify a valid format.'
  assert output_format not in ('gtest', 'none'), (
      'Cannot set stream for \'gtest\' or \'none\' output formats.')

  assert output_format in _OUTPUT_FILENAME_LOOKUP, (
      'No known filename for the \'%s\' output format' % output_format)
  output_file = os.path.join(output_dir, _OUTPUT_FILENAME_LOOKUP[output_format])

  # TODO(eakuefner): Factor this hack out after we rewrite HTMLOutputFormatter.
  if output_format in ['html', 'csv']:
    open(output_file, 'a').close() # Create file if it doesn't exist.
    return codecs.open(output_file, mode='r+', encoding='utf-8')
  else:
    return open(output_file, mode='w+')


def _GetProgressReporter(output_skipped_tests_summary, suppress_gtest_report):
  if suppress_gtest_report:
    return progress_reporter.ProgressReporter()

  return gtest_progress_reporter.GTestProgressReporter(
      sys.stdout, output_skipped_tests_summary=output_skipped_tests_summary)


def CreateResults(benchmark_metadata, options,
                  should_add_value=lambda name, is_first: True,
                  benchmark_enabled=True):
  """
  Args:
    options: Contains the options specified in AddResultsOptions.
  """
  if not options.output_formats:
    options.output_formats = [_DEFAULT_OUTPUT_FORMAT]

  artifacts = artifact_results.NoopArtifactResults(options.output_dir)

  upload_bucket = None
  if options.upload_results:
    upload_bucket = options.upload_bucket
    if upload_bucket in cloud_storage.BUCKET_ALIASES:
      upload_bucket = cloud_storage.BUCKET_ALIASES[upload_bucket]

  output_formatters = []
  for output_format in options.output_formats:
    if output_format == 'none' or output_format == "gtest":
      continue
    # pylint: disable=redefined-variable-type
    if isinstance(artifacts, artifact_results.NoopArtifactResults):
      artifacts = artifact_results.ArtifactResults(options.output_dir)
    output_stream = _GetOutputStream(output_format, options.output_dir)
    if output_format == 'html':
      output_formatters.append(html_output_formatter.HtmlOutputFormatter(
          output_stream, benchmark_metadata, options.reset_results,
          upload_bucket))
    elif output_format == 'json-test-results':
      # Only create artifact results if we're going to actually output them
      # through an output format.
      artifacts = artifact_results.ArtifactResults(options.output_dir)
      output_formatters.append(json_3_output_formatter.JsonOutputFormatter(
          output_stream, artifacts))
    elif output_format == 'chartjson':
      output_formatters.append(
          chart_json_output_formatter.ChartJsonOutputFormatter(
              output_stream, benchmark_metadata))
    elif output_format == 'csv':
      output_formatters.append(
          csv_output_formatter.CsvOutputFormatter(
              output_stream, options.reset_results))
    elif output_format == 'histograms':
      output_formatters.append(
          histogram_set_json_output_formatter.HistogramSetJsonOutputFormatter(
              output_stream, benchmark_metadata, options.reset_results))
    else:
      # Should never be reached. The parser enforces the choices.
      raise Exception('Invalid --output-format "%s". Valid choices are: %s'
                      % (output_format, ', '.join(_OUTPUT_FORMAT_CHOICES)))

  # TODO(chrishenry): This is here to not change the output of
  # gtest. Let's try enabling skipped tests summary for gtest test
  # results too (in a separate patch), and see if we break anything.
  output_skipped_tests_summary = 'gtest' in options.output_formats

  reporter = _GetProgressReporter(output_skipped_tests_summary,
                                  options.suppress_gtest_report)

  results = page_test_results.PageTestResults(
      output_formatters=output_formatters, progress_reporter=reporter,
      output_dir=options.output_dir,
      should_add_value=should_add_value,
      benchmark_enabled=benchmark_enabled,
      upload_bucket=upload_bucket,
      artifact_results=artifacts,
      benchmark_metadata=benchmark_metadata)

  results.telemetry_info.benchmark_name = benchmark_metadata.name
  results.telemetry_info.benchmark_descriptions = benchmark_metadata.description
  results.telemetry_info.benchmark_start_epoch = time.time()
  if options.results_label:
    results.telemetry_info.label = options.results_label

  return results
