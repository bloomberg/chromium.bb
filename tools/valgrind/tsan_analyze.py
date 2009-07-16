#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# tsan_analyze.py

''' Given a ThreadSanitizer output file, parses errors and uniques them.'''

import logging
import optparse
import os
import re
import subprocess
import sys
import time

class TsanAnalyze:
  ''' Given a set of ThreadSanitizer output files, parse all the errors out of
  them, unique them and output the results.'''

  def __init__(self, source_dir, files):
    '''Reads in a set of files.

    Args:
      source_dir: Path to top of source tree for this build
      files: A list of filenames.
    '''

    self.races = []
    for file in files:
      self.races = self.races + self.GetReportFrom(file)

  def GetReportFrom(self, filename):
    ret = []
    f = open(filename, 'r')
    while True:
      line = f.readline()
      if (line == ''):
        break
      tmp = ""
      while (re.match(".*INFO: T.* " +
             "(has been created by T.* at this point|is program's main thread)"
             ".*", line)):
        tmp = tmp + line
        while not re.match(".*}}}.*", line):
          line = f.readline()
          tmp = tmp + line
        line = f.readline()

      if (re.match(".*Possible data race.*", line)):
        tmp = tmp + line
        while not re.match(".*}}}.*", line):
          line = f.readline()
          tmp = tmp + line
        ret.append(tmp.strip())
    f.close()
    return ret

  def Report(self):
    if len(self.races) > 0:
      logging.error("Found %i race reports" % len(self.races))
      for report in self.races:
        logging.error("\n" + report)
      return -1
    logging.info("PASS: No race reports found")
    return 0

if __name__ == '__main__':
  '''For testing only. The TsanAnalyze class should be imported instead.'''
  retcode = 0
  parser = optparse.OptionParser("usage: %prog [options] <files to analyze>")
  parser.add_option("", "--source_dir",
                    help="path to top of source tree for this build"
                    "(used to normalize source paths in baseline)")

  (options, args) = parser.parse_args()
  if not len(args) >= 1:
    parser.error("no filename specified")
  filenames = args

  analyzer = TsanAnalyze(options.source_dir, filenames)
  retcode = analyzer.Report()

  sys.exit(retcode)
