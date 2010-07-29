#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# tsan_analyze.py

''' Given a ThreadSanitizer output file, parses errors and uniques them.'''

import gdb_helper

import logging
import optparse
import os
import re
import subprocess
import sys
import time

# Global symbol table (ugh)
TheAddressTable = None

class _StackTraceLine(object):
  def __init__(self, line, address, binary):
    self.raw_line_ = line
    self.address = address
    self.binary = binary
  def __str__(self):
    global TheAddressTable
    file, line = TheAddressTable.GetFileLine(self.binary, self.address)
    if (file is None) or (line is None):
      return self.raw_line_
    else:
      return self.raw_line_.replace(self.binary, '%s:%s' % (file, line))

class TsanAnalyzer:
  ''' Given a set of ThreadSanitizer output files, parse all the errors out of
  them, unique them and output the results.'''

  LOAD_LIB_RE = re.compile('--[0-9]+-- ([^(:]*) \((0x[0-9a-f]+)\)')
  TSAN_LINE_RE = re.compile('==[0-9]+==\s*[#0-9]+\s*'
                            '([0-9A-Fa-fx]+):'
                            '(?:[^ ]* )*'
                            '([^ :\n]+)'
                            '')

  THREAD_CREATION_STR = ("INFO: T.* "
      "(has been created by T.* at this point|is program's main thread)")

  SANITY_TEST_SUPPRESSION = "ThreadSanitizer sanity test"
  def __init__(self, source_dir, use_gdb=False):
    '''Reads in a set of files.

    Args:
      source_dir: Path to top of source tree for this build
    '''

    self._use_gdb = use_gdb

  def ReadLine(self):
    self.line_ = self.cur_fd_.readline()
    self.stack_trace_line_ = None
    if not self._use_gdb:
      return
    global TheAddressTable
    match = TsanAnalyzer.LOAD_LIB_RE.match(self.line_)
    if match:
      binary, ip = match.groups()
      TheAddressTable.AddBinaryAt(binary, ip)
      return
    match = TsanAnalyzer.TSAN_LINE_RE.match(self.line_)
    if match:
      address, binary_name = match.groups()
      stack_trace_line = _StackTraceLine(self.line_, address, binary_name)
      TheAddressTable.Add(stack_trace_line.binary, stack_trace_line.address)
      self.stack_trace_line_ = stack_trace_line

  def ReadSection(self):
    result = [self.line_]
    if re.search("{{{", self.line_):
      while not re.search('}}}', self.line_):
        self.ReadLine()
        if self.stack_trace_line_ is None:
          result.append(self.line_)
        else:
          result.append(self.stack_trace_line_)
    return result

  def ParseReportFile(self, filename):
    self.cur_fd_ = open(filename, 'r')

    while True:
      # Read race reports.
      self.ReadLine()
      if (self.line_ == ''):
        break

      tmp = []
      while re.search(TsanAnalyzer.THREAD_CREATION_STR, self.line_):
        tmp.extend(self.ReadSection())
        self.ReadLine()
      if re.search("Possible data race", self.line_):
        tmp.extend(self.ReadSection())
        self.races.append(tmp)

      match = re.search(" used_suppression:\s+([0-9]+)\s(.*)", self.line_)
      if match:
        count, supp_name = match.groups()
        count = int(count)
        if supp_name in self.used_suppressions:
          self.used_suppressions[supp_name] += count
        else:
          self.used_suppressions[supp_name] = count
    self.cur_fd_.close()

  def Report(self, files, check_sanity=False):
    '''TODO!!!
      files: A list of filenames.
    '''
    global TheAddressTable
    if self._use_gdb:
      TheAddressTable = gdb_helper.AddressTable()
    else:
      TheAddressTable = None
    self.races = []
    self.used_suppressions = {}
    for file in files:
      self.ParseReportFile(file)
    if self._use_gdb:
      TheAddressTable.ResolveAll()

    is_sane = False
    print "-----------------------------------------------------"
    print "Suppressions used:"
    print "  count name"
    for item in sorted(self.used_suppressions.items(), key=lambda (k,v): (v,k)):
      print "%7s %s" % (item[1], item[0])
      if item[0].startswith(TsanAnalyzer.SANITY_TEST_SUPPRESSION):
        is_sane = True
    print "-----------------------------------------------------"
    sys.stdout.flush()

    retcode = 0
    if len(self.races) > 0:
      logging.error("FAIL! Found %i race reports" % len(self.races))
      for report_list in self.races:
        report = ''
        for line in report_list:
          report += str(line)
        logging.error('\n' + report)
      retcode = -1

    # Report tool's insanity even if there were errors.
    if check_sanity and not is_sane:
      logging.error("FAIL! Sanity check failed!")
      retcode = -3

    if retcode != 0:
      return retcode
    logging.info("PASS: No race reports found")
    return 0

if __name__ == '__main__':
  '''For testing only. The TsanAnalyzer class should be imported instead.'''
  retcode = 0
  parser = optparse.OptionParser("usage: %prog [options] <files to analyze>")
  parser.add_option("", "--source_dir",
                    help="path to top of source tree for this build"
                    "(used to normalize source paths in baseline)")

  (options, args) = parser.parse_args()
  if len(args) == 0:
    parser.error("no filename specified")
  filenames = args

  analyzer = TsanAnalyzer(options.source_dir, use_gdb=True)
  retcode = analyzer.Report(filenames)

  sys.exit(retcode)
