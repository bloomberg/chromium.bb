# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import csv
import logging
import os
import sys
import time
import unittest

import chrome_remote_control
import chrome_remote_control.browser_options
from gpu_tools import page_set

class MeasurementFailure(Exception):
  """Exception that can be thrown from MeasurePage to indicate an undisired but
  designed-for problem."""
  pass

class MultiPageBenchmark(object):
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
    self.options = None
    self.page_failures = []
    self.field_names = None

  def AddOptions(self, parser):
    """Override to expose command-line options for this benchmark.

    The provided parser is an optparse.OptionParser instance and accepts all
    normal results. The parsed options are availble in MeasurePage as
    self.options."""
    pass

  def CustomizeBrowserOptions(self, options):
    """Override to add test-specific options to the BrowserOptions object"""
    pass

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

  def Run(self, results_writer, browser, options, ps):
    """Runs the actual benchmark, starting the browser using outputting results
    to results_writer.

    If args is not specified, sys.argv[1:] is used.
    """
    self.options = options
    self.field_names = None

    with browser.ConnectToNthTab(0) as tab:
      for page in ps.pages:
        self._RunPage(results_writer, page, tab)
    self.options = None
    self.field_names = None

  def _RunPage(self, results_writer, page, tab):
    # Load the page.
    try:
      logging.debug('Loading %s...', page.url)
      tab.page.Navigate(page.url)

      # TODO(dtu): Detect HTTP redirects.
      time.sleep(2)  # Wait for unpredictable redirects.
      tab.WaitForDocumentReadyStateToBeInteractiveOrBetter()
    except chrome_remote_control.TimeoutException, ex:
      logging.warning('Timed out while loading: %s', page.url)
      self.page_failures.append({'page': page,
                                 'exception': ex})
      return

    # Measure the page.
    try:
      results = self.MeasurePage(page, tab)
    except Exception, ex:
      if isinstance(ex, MeasurementFailure):
        logging.info('%s failed: %s:', page.url, str(ex))
      else:
        import traceback
        traceback.print_exc()
        logging.warning('%s had unexpected failure: %s:', page.url, str(ex))

      self.page_failures.append({'page': page,
                                 'exception': ex})
      return

    # Output.
    assert 'url' not in results

    if not self.field_names:
      self.field_names = list(results.keys())
      self.field_names.sort()
      self.field_names.insert(0, 'url')
      results_writer.writerow(self.field_names)

    row = []
    row.append(page.url)
    for name in self.field_names:
      if name == 'url':
        continue
      # If this assertion pops, your MeasurePage is returning inconsistent
      # results! You must return the same dict keys every time!
      assert name in results
      row.append(results[name])
    results_writer.writerow(row)


def Main(benchmark, args=None):
  """Turns a MultiPageBenchmark into a command-line program.

  If args is not specified, sys.argv[1:] is used.
  """
  options = chrome_remote_control.BrowserOptions()
  parser = options.CreateParser('%prog [options] <page_set>')
  benchmark.AddOptions(parser)
  _, args = parser.parse_args(args)

  if len(args) != 1:
    parser.print_usage()
    import page_sets
    sys.stderr.write('Available pagesets:\n%s\n\n' % ',\n'.join(
        [os.path.relpath(f) for f in page_sets.GetAllPageSetFilenames()]))
    sys.exit(1)

  if not os.path.exists(args[0]):
    sys.stderr.write('Pageset %s does not exist\n' % args[0])
    sys.exit(1)

  ps = page_set.PageSet()
  ps.LoadFromFile(args[0])

  benchmark.CustomizeBrowserOptions(options)
  possible_browser = chrome_remote_control.FindBrowser(options)
  if possible_browser == None:
    sys.stderr.write(
      'No browser found.\n' +
      'Use --browser=list to figure out which are available.\n')
    sys.exit(1)

  with possible_browser.Create() as browser:
    benchmark.Run(csv.writer(sys.stdout), browser, options, ps)

  if len(benchmark.page_failures):
    logging.warning('Failed pages: %s', '\n'.join(
        [failure['page'].url for failure in benchmark.page_failures]))
  return len(benchmark.page_failures)


class MultiPageBenchmarkUnitTest(unittest.TestCase):
  """unittest.TestCase-derived class to help in the construction of unit tests
  for a benchmark."""

  @property
  def unittest_data_dir(self):
    return os.path.relpath(os.path.join(os.path.dirname(__file__),
                                        '..', 'unittest_data'))

  def CreatePageSetFromFileInUnittestDataDir(self, test_filename):
    path = os.path.join(self.unittest_data_dir, test_filename)
    self.assertTrue(os.path.exists(path))
    page = page_set.Page(test_filename)

    ps = page_set.PageSet()
    ps.pages.append(page)
    return ps

  def CustomizeOptionsForTest(self, options):
    """Override to customize default options."""
    pass

  def RunBenchmark(self, benchmark, ps):
    """Runs a benchmark against a pageset, returning the rows its outputs."""
    rows = []
    class LocalWriter(object):
      @staticmethod
      def writerow(row):
        rows.append(row)

    assert chrome_remote_control.browser_options.options_for_unittests
    options = (
      chrome_remote_control.browser_options.options_for_unittests.Copy())
    temp_parser = options.CreateParser()
    benchmark.AddOptions(temp_parser)
    defaults = temp_parser.get_default_values()
    for k, v in defaults.__dict__.items():
      if hasattr(options, k):
        continue
      setattr(options, k, v)

    benchmark.CustomizeBrowserOptions(options)
    self.CustomizeOptionsForTest(options)
    possible_browser = chrome_remote_control.FindBrowser(options)
    with possible_browser.Create() as browser:
      with browser.CreateTemporaryHTTPServer(self.unittest_data_dir) as server:
        for page in ps.pages:
          page.url = '%s/%s' % (server.url, page.url)
        benchmark.Run(LocalWriter(), browser, options, ps)
    return rows
