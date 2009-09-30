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
    self.used_suppressions = {}
    for file in files:
      self.ParseReportFile(file)

  def ParseReportFile(self, filename):
    f = open(filename, 'r')
    while True:
      # Read race reports.
      line = f.readline()
      if (line == ''):
        break

      if re.search("ERROR SUMMARY", line):
        # TSAN has finished working. The remaining reports are duplicates.
        break

      tmp = ""
      while (re.search("INFO: T.* "
             "(has been created by T.* at this point"
             "|is program's main thread)", line)):
        tmp = tmp + line
        while not re.search("}}}", line):
          line = f.readline()
          tmp = tmp + line
        line = f.readline()

      if (re.search("Possible data race", line)):
        tmp = tmp + line
        while not re.search("}}}", line):
          line = f.readline()
          tmp = tmp + line
        self.races.append(tmp.strip())

    while True:
      # Read the list of used suppressions.
      line = f.readline()
      if (line == ''):
        break
      match = re.search(" used_suppression:\s+([0-9]+)\s(.*)", line)
      if match:
        count, supp_name = match.groups()
        if supp_name in self.used_suppressions:
          self.used_suppressions[supp_name] += count
        else:
          self.used_suppressions[supp_name] = count
    f.close()

  def Report(self):
    print "-----------------------------------------------------"
    print "Suppressions used:"
    print "  count name"
    for item in sorted(self.used_suppressions.items(), key=lambda (k,v): (v,k)):
      print "%7s %s" % (item[1], item[0])
    print "-----------------------------------------------------"
    sys.stdout.flush()

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
