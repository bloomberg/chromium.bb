#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

import pyauto_nacl  # Must be imported before pyauto
import pyauto
import nacl_utils


class NaClUMATest(pyauto.PyUITest):
  """Tests for the UMA stats NaCl records."""

  # Surf to about:histograms and scrape the UMA data.
  def getHistogramData(self, path):
    js = """\
var results = document.getElementsByTagName('pre');
var text = '';
for (var i = 0; i < results.length; ++i) {
  text = text + results[i].innerHTML + '\\n';
}
window.domAutomationController.send(text);
"""
    self.NavigateToURL('about:histograms/%s' % path)
    data = self.ExecuteJavascript(js, 0, 0)
    return data.replace('<br>', '\n');

  # Turn scraped UMA data into a 2-level dictionary.
  # Histogram name -> histogram bucket -> count
  def parseHistogramData(self, data):
    spacer_pattern = re.compile('\d+\s+\.\.\.')
    data_pattern = re.compile('(\d+)\s*-*O\s*\(\s*(\d+)')

    hists = {}
    name = None
    for line in data.strip().split('\n'):
      line = line.strip()
      if line == '':
        continue

      if line.startswith('Histogram: '):
        # Start a new histogram
        name = line.split()[1]
        self.assertFalse(name in hists, name)
        hists[name] = {}
      elif spacer_pattern.match(line) is not None:
        # Just a visual spacer
        pass
      else:
        # Actual data
        result = data_pattern.match(line)
        self.assertFalse(result is None, line)
        self.assertEqual(len(result.groups()), 2)
        label = int(result.group(1))
        count = int(result.group(2))
        self.assertFalse(label in hists, label)
        if count != 0:
          hists[name][label] = count
    return hists

  def getHistograms(self, path):
    return self.parseHistogramData(self.getHistogramData(path))

  def assertHistogramCount(self, hists, name, count):
    self.assertTrue(name in hists, hists)
    self.assertEqual(sum(hists[name].values()), count)

  # Make sure the histogram parser works.
  def testParseHistogram(self):
    hist_data = """
Histogram: NaCl.LoadStatus.Plugin recorded 2 samples, average = 0.0 (flags = 0x1)
0  ------------------------------------------------------------------------O (2 = 100.0%)
1  ...

Histogram: NaCl.ManifestDownloadTime recorded 2 samples, average = 5.5 (flags = 0x1)
0  ...
3  ------------------------------------------------------------------------O (1 = 50.0%) {0.0%}
4  ...
8  ------------------------------------------------------------------------O (1 = 50.0%) {50.0%}
9  ...

Histogram: NaCl.NexeDownloadTime recorded 2 samples, average = 6.5 (flags = 0x1)
0  ...
6  ------------------------------------------------------------------------O (1 = 50.0%) {0.0%}
7  ------------------------------------------------------------------------O (1 = 50.0%) {50.0%}
8  ...

Histogram: NaCl.NexeSize recorded 2 samples, average = 490.0 (flags = 0x1)
0    ...
483  ------------------------------------------------------------------------O (2 = 100.0%) {0.0%}
546  ...

Histogram: NaCl.NexeStartupTime recorded 2 samples, average = 150.5 (flags = 0x1)
0    ...
137  ------------------------------------------------------------------------O (1 = 50.0%) {0.0%}
149  ------------------------------------------------------------------------O (1 = 50.0%) {50.0%}
162  ...

Histogram: NaCl.NexeStartupTimePerMB recorded 2 samples, average = 313.5 (flags = 0x1)
0    ...
296  ------------------------------------------------------------------------O (2 = 100.0%) {0.0%}
323  ...

Histogram: NaCl.OSArch recorded 2 samples, average = 1.0 (flags = 0x1)
0  O                                                                         (0 = 0.0%)
1  ------------------------------------------------------------------------O (2 = 100.0%) {0.0%}
2  ...
"""

    hists_expected = {
        'NaCl.LoadStatus.Plugin': {0: 2},
        'NaCl.ManifestDownloadTime': {8: 1, 3: 1},
        'NaCl.NexeDownloadTime': {6: 1, 7: 1},
        'NaCl.NexeSize': {483: 2},
        'NaCl.NexeStartupTime': {137: 1, 149: 1},
        'NaCl.NexeStartupTimePerMB': {296: 2},
        'NaCl.OSArch': {1: 2},
        }

    hists = self.parseHistogramData(hist_data)
    self.assertEqual(hists, hists_expected)

  def testHistogramIncrement(self):
    # An arbitrary page with a single working nexe.
    page = 'ppapi_ppb_core.html'
    url = self.GetHttpURLForDataPath(page)

    # Make sure UMA stats are logged.
    self.NavigateToURL(url)
    nacl_utils.WaitForNexeLoad(self)
    nacl_utils.VerifyAllTestsPassed(self)
    hists = self.getHistograms('NaCl')

    # Keep in sync with plugin_error.h
    ERROR_SUCCESS = 0
    # Keep in sync with nacl_error_code.h
    LOAD_OK = 0

    self.assertHistogramCount(hists, 'NaCl.LoadStatus.Plugin', 1)
    self.assertEqual(hists['NaCl.LoadStatus.Plugin'][ERROR_SUCCESS], 1)
    self.assertHistogramCount(hists, 'NaCl.LoadStatus.SelLdr', 1)
    self.assertEqual(hists['NaCl.LoadStatus.SelLdr'][LOAD_OK], 1)
    self.assertHistogramCount(hists, 'NaCl.ManifestDownloadTime', 1)
    self.assertHistogramCount(hists, 'NaCl.NexeDownloadTime', 1)
    self.assertHistogramCount(hists, 'NaCl.NexeSize', 1)
    self.assertHistogramCount(hists, 'NaCl.NexeStartupTime', 1)
    self.assertHistogramCount(hists, 'NaCl.NexeStartupTimePerMB', 1)
    self.assertHistogramCount(hists, 'NaCl.OSArch', 1)

    # Do it again.
    self.NavigateToURL(url)
    nacl_utils.WaitForNexeLoad(self)
    nacl_utils.VerifyAllTestsPassed(self)
    hists = self.getHistograms('NaCl')

    self.assertHistogramCount(hists, 'NaCl.LoadStatus.Plugin', 2)
    self.assertEqual(hists['NaCl.LoadStatus.Plugin'][ERROR_SUCCESS], 2)
    self.assertHistogramCount(hists, 'NaCl.LoadStatus.SelLdr', 2)
    self.assertEqual(hists['NaCl.LoadStatus.SelLdr'][LOAD_OK], 2)
    self.assertHistogramCount(hists, 'NaCl.ManifestDownloadTime', 2)
    self.assertHistogramCount(hists, 'NaCl.NexeDownloadTime', 2)
    self.assertHistogramCount(hists, 'NaCl.NexeSize', 2)
    self.assertHistogramCount(hists, 'NaCl.NexeStartupTime', 2)
    self.assertHistogramCount(hists, 'NaCl.NexeStartupTimePerMB', 2)
    self.assertHistogramCount(hists, 'NaCl.OSArch', 2)

  def testValidatorFail(self):
    # This test should induce a validation failure.
    page = 'ppapi_bad_native.html'
    url = self.GetHttpURLForDataPath(page)

    # Make sure UMA stats are logged.
    self.NavigateToURL(url)
    nacl_utils.WaitForNexeLoad(self)
    nacl_utils.VerifyAllTestsPassed(self)
    hists = self.getHistograms('NaCl')

    # Keep in sync with plugin_error.h
    ERROR_SEL_LDR_START_STATUS = 27
    self.assertHistogramCount(hists, 'NaCl.LoadStatus.Plugin', 1)
    self.assertEqual(
        hists['NaCl.LoadStatus.Plugin'][ERROR_SEL_LDR_START_STATUS],
        1)

    # Keep in sync with nacl_error_code.h
    LOAD_VALIDATION_FAILED = 57
    self.assertHistogramCount(hists, 'NaCl.LoadStatus.SelLdr', 1)
    self.assertEqual(
        hists['NaCl.LoadStatus.SelLdr'][LOAD_VALIDATION_FAILED],
        1)


if __name__ == '__main__':
  pyauto_nacl.Main()
