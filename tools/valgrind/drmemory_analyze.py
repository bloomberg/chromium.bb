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

    self.reports = []
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
      match_syscall = re.search("system call (.*)\n", tmp_line)
      if match_syscall:
        syscall_name = match_syscall.groups()[0]
        result.append(" #%2i <sys.call> %s\n" % (cnt, syscall_name))
        cnt = cnt + 1
        self.ReadLine() # skip "<system call> line
        self.ReadLine()
        continue

      match_binary_fname = re.search("(0x[0-9a-fA-F]+) <.*> (.*)!([^+]*)"
                                     "(?:\+0x[0-9a-fA-F]+)?\n", tmp_line)
      self.ReadLine()
      match_src_line = re.search("\s*(.*):([0-9]+)(?:\+0x[0-9a-fA-F]+)?",
                                 self.line_)
      if match_src_line:
        self.ReadLine()
        if match_binary_fname:
          pc, binary, fname = match_binary_fname.groups()
          if re.search(CUT_STACK_BELOW, fname):
            break
          report_line = (" #%2i %s %-50s" % (cnt, pc, fname))
          if not re.search("\.exe$", binary):
            # Print the DLL name
            report_line += " " + binary
          src, lineno = match_src_line.groups()
          if src != "??":
            for pat in FILE_PREFIXES_TO_CUT:
              idx = src.rfind(pat)
              if idx != -1:
                src = src[idx+len(pat):]
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
      match = re.search("^Error #[0-9]+: (.*)", self.line_)
      if match:
        self.line_ = match.groups()[0].strip() + "\n"
        tmp.extend(self.ReadSection())
        self.reports.append(tmp)
      elif self.line_.startswith("ASSERT FAILURE"):
        self.reports.append(self.line_.strip())

    self.cur_fd_.close()

  def Report(self, check_sanity):
    sys.stdout.flush()
    #TODO(timurrrr): support positive tests / check_sanity==True

    if len(self.reports) > 0:
      logging.error("Found %i error reports" % len(self.reports))
      for report_list in self.reports:
        report = ''
        for line in report_list:
          report += str(line)
        logging.error('\n' + report)
      logging.error("Total: %i error reports" % len(self.reports))
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
  if len(args) == 0:
    parser.error("no filename specified")
  filenames = args

  analyzer = DrMemoryAnalyze(options.source_dir, filenames)
  retcode = analyzer.Report(False)

  sys.exit(retcode)
