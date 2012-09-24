# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import csv
import logging
import os
import sys
import traceback

from chrome_remote_control import browser_finder
from chrome_remote_control import browser_options
from chrome_remote_control import page_runner
from chrome_remote_control import page_set
from chrome_remote_control import page_test
from chrome_remote_control import util

class MeasurementFailure(page_test.Failure):
  """Exception that can be thrown from MeasurePage to indicate an undesired but
  designed-for problem."""
  pass

class BenchmarkResults(page_test.PageTestResults):
  def __init__(self):
    super(BenchmarkResults, self).__init__()
    self.page_results = []

  def AddPageResults(self, page, results):
    self.page_results.append({'page': page,
                              'results': results})

class CsvBenchmarkResults(page_test.PageTestResults):
  def __init__(self, results_writer):
    super(CsvBenchmarkResults, self).__init__()
    self._results_writer = results_writer
    self.field_names = None

  def AddPageResults(self, page, results):
    assert 'url' not in results

    if not self.field_names:
      self.field_names = results.keys()
      self.field_names.sort()
      results_writer.writerow(['url'] + self.field_names)

    row = [page.url]
    for name in self.field_names:
      # If this assertion pops, your MeasurePage is returning inconsistent
      # results! You must return the same dict keys every time!
      assert name in results, """MeasurePage returned inconsistent results! You
must return the same dict keys every time."""
      row.append(results[name])
    self._results_writer.writerow(row)


# TODO(nduca): Rename to page_benchmark
class MultiPageBenchmark(page_test.PageTest):
  """Glue code for running a benchmark across a set of pages.

  To use this, subclass from the benchmark and override MeasurePage. For
  example:

     class BodyChildElementBenchmark(MultiPageBenchmark):
        def MeasurePage(self, page, tab):
           body_child_count = tab.runtime.Evaluate(
               'document.body.children.length')
           return {'body_child_count': body_child_count}

     if __name__ == '__main__':
         multi_page_benchmark.Main(BodyChildElementBenchmark())

  All benchmarks should include a unit test!

     TODO(nduca): Add explanation of how to write the unit test.

  To add test-specific options:

     class BodyChildElementBenchmark(MultiPageBenchmark):
        def AddOptions(parser):
           parser.add_option('--element', action='store', default='body')

        def MeasurePage(self, page, tab):
           body_child_count = tab.runtime.Evaluate(
              'document.querySelector('%s').children.length')
           return {'child_count': child_count}
  """
  def __init__(self):
    super(MultiPageBenchmark, self).__init__('_RunTest')

  def _RunTest(self, page, tab, results):
    page_results = self.MeasurePage(page, tab)
    results.AddPageResults(page, page_results)

  def MeasurePage(self, page, tab):
    """Override to actually measure the page's performance.

    page is a page_set.Page
    tab is an instance of chrome_remote_control.Tab

    Should return a dictionary of measured values on success, or raise an
    exception on failure. The fields in the dictionary must be the same across
    all iterations. The dictionary must not include the field 'url.' The
    MultiPageBenchmark will add that automatically to the final output.

    Prefer field names that are in accordance with python variable style. E.g.
    field_name.

    Put together:

       def MeasurePage(self, page, tab):
         res = tab.runtime.Evaluate('2+2')
         if res != 4:
           raise Exception('Oh, wow.')
         return {'two_plus_two': res}
    """
    raise NotImplementedError()


def Main(benchmark, args=None):
  """Turns a MultiPageBenchmark into a command-line program.

  If args is not specified, sys.argv[1:] is used.
  """
  options = browser_options.BrowserOptions()
  parser = options.CreateParser('%prog [options] <page_set>')
  benchmark.AddOptions(parser)
  _, args = parser.parse_args(args)

  if len(args) != 1:
    parser.print_usage()
    import page_sets
    sys.stderr.write('Available pagesets:\n%s\n\n' % ',\n'.join(
        [os.path.relpath(f) for f in page_sets.GetAllPageSetFilenames()]))
    sys.exit(1)

  ps = page_set.PageSet.FromFile(args[0])

  benchmark.CustomizeBrowserOptions(options)
  possible_browser = browser_finder.FindBrowser(options)
  if possible_browser == None:
    sys.stderr.write(
      'No browser found.\n' +
      'Use --browser=list to figure out which are available.\n')
    sys.exit(1)

  results = CsvBenchmarkResults(csv.writer(sys.stdout))
  with page_runner.PageRunner(ps) as runner:
    runner.Run(options, possible_browser, benchmark, results)

  if len(results.page_failures):
    logging.warning('Failed pages: %s', '\n'.join(
        [failure['page'].url for failure in results.page_failures]))
  return max(255, len(results.page_failures))
