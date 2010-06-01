#!/usr/bin/python
# Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# drmemory_analyze.py

''' Given a ThreadSanitizer output file, parses errors and uniques them.'''

import logging
import optparse
import os
import re
import subprocess
import sys
import time

class _StackTraceLine(object):
  def __init__(self, line, address, binary):
    self.raw_line_ = line
    self.address = address
    self.binary = binary
  def __str__(self):
    return self.raw_line_

class DrMemoryAnalyze:
  ''' Given a set of Dr.Memory output files, parse all the errors out of
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

  def ReadLine(self):
    self.line_ = self.cur_fd_.readline()
    self.stack_trace_line_ = None

  def ReadSection(self):
    FILE_PREFIXES_TO_CUT = [
        "build\\src\\",
        "crt_bld\\self_x86\\",
    ]
    CUT_STACK_BELOW = ".*testing.*Test.*Run.*"

    result = [self.line_]
    self.ReadLine()
    cnt = 1
    while len(self.line_.strip()) > 0:
      tmp_line = self.line_
      match1 = re.search("system call (.*)\n", tmp_line)
      if match1:
        result.append(" #%2i <sys.call> %s\n" % (cnt, match1.groups()[0]))
        cnt = cnt + 1
        self.ReadLine() # skip "<system call> line
        self.ReadLine()
        continue
      match1 = re.search("(0x[0-9a-fA-F]+) <.*> (.*)!([^+]*)"
                         "(?:\+0x[0-9a-fA-F]+)?\n", tmp_line)
      self.ReadLine()
      match2 = re.search("\s*(.*):([0-9]+)(?:\+0x[0-9a-fA-F]+)?", self.line_)
      if match2:
        self.ReadLine()
        if match1:
          pc, binary, fname = match1.groups()
          if re.search(CUT_STACK_BELOW, fname):
            break
          src, lineno = match2.groups()
          binary_fname = "%s!%s" % (binary, fname)
          report_line = (" #%2i %s %s" % (cnt, pc, binary_fname))
          if src != "??":
            for pat in FILE_PREFIXES_TO_CUT:
              idx = src.rfind(pat)
              if idx != -1:
                src = src[idx+len(pat):]
            if (len(binary_fname) < 40):
              report_line += " " * (40 - len(binary_fname))
            report_line += " " + src
            if int(lineno) != 0:
              report_line += ":%i" % int(lineno)
          result.append(report_line + "\n")
          cnt = cnt + 1
    return result

  def ParseReportFile(self, filename):
    self.cur_fd_ = open(filename, 'r')

    while True:
      self.ReadLine()
      if (self.line_ == ''):
        break

      if re.search("Grouping errors that", self.line_):
        # DrMemory has finished working.
        break

      tmp = []
      match = re.search("Error .*: (.*)", self.line_)
      if match:
        self.line_ = match.groups()[0].strip() + "\n"
        tmp.extend(self.ReadSection())
        self.races.append(tmp)

    self.cur_fd_.close()

  def Report(self, check_sanity):
    sys.stdout.flush()
    #TODO(timurrrr): support positive tests / check_sanity==True

    if len(self.races) > 0:
      logging.error("Found %i error reports" % len(self.races))
      for report_list in self.races:
        report = ''
        for line in report_list:
          report += str(line)
        logging.error('\n' + report)
      return -1
    logging.info("PASS: No error reports found")
    return 0

if __name__ == '__main__':
  '''For testing only. The DrMemoryAnalyze class should be imported instead.'''
  retcode = 0
  parser = optparse.OptionParser("usage: %prog [options] <files to analyze>")
  parser.add_option("", "--source_dir",
                    help="path to top of source tree for this build"
                    "(used to normalize source paths in baseline)")

  (options, args) = parser.parse_args()
  if not len(args) >= 1:
    parser.error("no filename specified")
  filenames = args

  analyzer = DrMemoryAnalyze(options.source_dir, filenames)
  retcode = analyzer.Report(False)

  sys.exit(retcode)
