#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# drmemory_analyze.py

''' Given a Dr. Memory output file, parses errors and uniques them.'''

from collections import defaultdict
import common
import logging
import optparse
import os
import re
import subprocess
import sys
import time


class DrMemoryAnalyzer:
  ''' Given a set of Dr.Memory output files, parse all the errors out of
  them, unique them and output the results.'''

  def __init__(self):
    self.known_errors = set()

  def ReadLine(self):
    self.line_ = self.cur_fd_.readline()

  def ReadSection(self):
    result = [self.line_]
    self.ReadLine()
    while len(self.line_.strip()) > 0:
      result.append(self.line_)
      self.ReadLine()
    return result

  def ParseReportFile(self, filename):
    ret = []

    self.cur_fd_ = open(filename, 'r')
    while True:
      self.ReadLine()
      if (self.line_ == ''): break

      match = re.search("^Error #[0-9]+: (.*)", self.line_)
      if match:
        self.line_ = match.groups()[0].strip() + "\n"
        tmp = self.ReadSection()
        ret.append("".join(tmp).strip())

      if re.search("SUPPRESSIONS USED:", self.line_):
        self.ReadLine()
        while self.line_.strip() != "":
          line = self.line_.strip()
          (count, name) = re.match(" *([0-9]+)x(?: \(leaked .*\))?: (.*)",
                                   line).groups()
          count = int(count)
          self.used_suppressions[name] += count
          self.ReadLine()

      if self.line_.startswith("ASSERT FAILURE"):
        ret.append(self.line_.strip())

    self.cur_fd_.close()
    return ret

  def Report(self, filenames, testcase, check_sanity):
    sys.stdout.flush()
    # TODO(timurrrr): support positive tests / check_sanity==True

    to_report = []
    self.used_suppressions = defaultdict(int)
    for f in filenames:
      cur_reports = self.ParseReportFile(f)

      # Filter out the reports that were there in previous tests.
      for r in cur_reports:
        if r in self.known_errors:
          pass  # TODO: print out a hash once we add hashes to the reports.
        else:
          self.known_errors.add(r)
          to_report.append(r)

    common.PrintUsedSuppressionsList(self.used_suppressions)

    if not to_report:
      logging.info("PASS: No error reports found")
      return 0

    logging.error("Found %i error reports" % len(to_report))
    for report in to_report:
      if testcase:
        logging.error("\n%s\nNote: observed on `%s`\n" %
                      (report, testcase))
      else:
        logging.error("\n%s\n" % report)
    logging.error("Total: %i error reports" % len(to_report))
    return -1

if __name__ == '__main__':
  '''For testing only. The DrMemoryAnalyzer class should be imported instead.'''
  retcode = 0
  parser = optparse.OptionParser("usage: %prog <files to analyze>")
  (options, args) = parser.parse_args()
  if len(args) == 0:
    parser.error("no filename specified")
  filenames = args

  analyzer = DrMemoryAnalyzer()
  retcode = analyzer.Report(filenames, None, False)

  sys.exit(retcode)
