# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from collections import defaultdict
import os
import sys

from chrome_remote_control import page_test

# Get build/android/pylib scripts into our path.
# TODO(tonyg): Move perf_tests_helper.py to a common location.
sys.path.append(
    os.path.abspath(
        os.path.join(os.path.dirname(__file__),
                     '../../../build/android/pylib')))
from perf_tests_helper import PrintPerfResult  # pylint: disable=F0401

class MeasurementFailure(page_test.Failure):
  """Exception that can be thrown from MeasurePage to indicate an undesired but
  designed-for problem."""
  pass


class BenchmarkResults(page_test.PageTestResults):
  def __init__(self):
    super(BenchmarkResults, self).__init__()
    self.results_summary = defaultdict(list)
    self.page_results = []
    self.field_names = None
    self.field_units = {}

    self._page = None
    self._page_values = {}

  def WillMeasurePage(self, page):
    self._page = page
    self._page_values = {}

  def Add(self, name, units, value):
    assert name not in self._page_values, 'Result names must be unique'
    assert name != 'url', 'The name url cannot be used'
    if self.field_names:
      assert name in self.field_names, """MeasurePage returned inconsistent
results! You must return the same dict keys every time."""
    else:
      self.field_units[name] = units
    self._page_values[name] = value

  def DidMeasurePage(self):
    assert self._page, 'Failed to call WillMeasurePage'

    if not self.field_names:
      self.field_names = self._page_values.keys()
      self.field_names.sort()

    self.page_results.append(self._page_values)
    for name in self.field_names:
      units = self.field_units[name]
      value = self._page_values[name]
      self.results_summary[(name, units)].append(value)


class CsvBenchmarkResults(BenchmarkResults):
  def __init__(self, results_writer):
    super(CsvBenchmarkResults, self).__init__()
    self._results_writer = results_writer
    self._did_write_header = False

  def DidMeasurePage(self):
    super(CsvBenchmarkResults, self).DidMeasurePage()

    if not self._did_write_header:
      self._did_write_header = True
      row = ['url']
      for name in self.field_names:
        row.append('%s (%s)' % (name, self.field_units[name]))
      self._results_writer.writerow(row)

    row = [self._page.url]
    for name in self.field_names:
      value = self._page_values[name]
      row.append(value)
    self._results_writer.writerow(row)

  def PrintSummary(self, trace_tag):
    for measurement_units, values in self.results_summary.iteritems():
      measurement, units = measurement_units
      trace = measurement + (trace_tag or '')
      PrintPerfResult(measurement, trace, values, units)


# TODO(nduca): Rename to page_benchmark
class MultiPageBenchmark(page_test.PageTest):
  """Glue code for running a benchmark across a set of pages.

  To use this, subclass from the benchmark and override MeasurePage. For
  example:

     class BodyChildElementBenchmark(MultiPageBenchmark):
        def MeasurePage(self, page, tab, results):
           body_child_count = tab.runtime.Evaluate(
               'document.body.children.length')
           results.Add('body_children', 'count', body_child_count)

     if __name__ == '__main__':
         multi_page_benchmark.Main(BodyChildElementBenchmark())

  All benchmarks should include a unit test!

     TODO(nduca): Add explanation of how to write the unit test.

  To add test-specific options:

     class BodyChildElementBenchmark(MultiPageBenchmark):
        def AddOptions(parser):
           parser.add_option('--element', action='store', default='body')

        def MeasurePage(self, page, tab, results):
           body_child_count = tab.runtime.Evaluate(
              'document.querySelector('%s').children.length')
           results.Add('children', 'count', child_count)
  """
  def __init__(self):
    super(MultiPageBenchmark, self).__init__('_RunTest')

  def _RunTest(self, page, tab, results):
    results.WillMeasurePage(page)
    self.MeasurePage(page, tab, results)
    results.DidMeasurePage()

  def MeasurePage(self, page, tab, results):
    """Override to actually measure the page's performance.

    page is a page_set.Page
    tab is an instance of chrome_remote_control.Tab

    Should call results.Add(name, units, value) for each result, or raise an
    exception on failure. The name and units of each Add() call must be
    the same across all iterations. The name 'url' must not be used.

    Prefer field names that are in accordance with python variable style. E.g.
    field_name.

    Put together:

       def MeasurePage(self, page, tab, results):
         res = tab.runtime.Evaluate('2+2')
         if res != 4:
           raise Exception('Oh, wow.')
         results.Add('two_plus_two', 'count', res)
    """
    raise NotImplementedError()
