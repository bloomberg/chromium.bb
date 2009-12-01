#!/usr/bin/python
# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# heapcheck_test.py

"""Wrapper for running the test under heapchecker and analyzing the output."""

import datetime
import logging
import os
import re

import common
import google.path_utils
import suppressions


class HeapcheckWrapper(object):
  TMP_FILE = 'heapcheck.log'

  def __init__(self, supp_files):
    self._mode = 'strict'
    self._timeout = 1200
    self._nocleanup_on_exit = False
    self._suppressions = []
    for fname in supp_files:
      self._suppressions.extend(suppressions.ReadSuppressionsFromFile(fname))
    if os.path.exists(self.TMP_FILE):
      os.remove(self.TMP_FILE)

  def PutEnvAndLog(self, env_name, env_value):
    """Sets the env var |env_name| to |env_value| and writes to logging.info.
    """
    os.putenv(env_name, env_value)
    logging.info('export %s=%s', env_name, env_value)

  def Execute(self):
    """Executes the app to be tested."""
    logging.info('starting execution...')
    proc = ['sh', google.path_utils.ScriptDir() + '/heapcheck_std.sh']
    proc += self._args
    self.PutEnvAndLog('G_SLICE', 'always-malloc')
    self.PutEnvAndLog('NSS_DISABLE_ARENA_FREE_LIST', '1')
    self.PutEnvAndLog('GTEST_DEATH_TEST_USE_FORK', '1')
    self.PutEnvAndLog('HEAPCHECK', self._mode)

    common.RunSubprocess(proc, self._timeout)

    # Always return true, even if running the subprocess failed. We depend on
    # Analyze to determine if the run was valid. (This behaviour copied from
    # the purify_test.py script.)
    return True

  def Analyze(self, log_lines):
    """Analyzes the app's output and applies suppressions to the reports.

    Analyze() searches the logs for leak reports and tries to apply
    suppressions to them. Unsuppressed reports and other log messages are
    dumped as is.

    Args:
      log_lines: An iterator over the app's log lines.
    Returns:
      1, if unsuppressed reports remain in the output, 0 otherwise.
    """
    leak_report = re.compile(
        'Leak of ([0-9]*) bytes in ([0-9]*) objects allocated from:')
    stack_line = re.compile('\s*@\s*0x[0-9a-fA-F]*\s*(\S*)')
    return_code = 0
    # leak signature: [number of bytes, number of objects]
    cur_leak_signature = None
    cur_stack = []
    cur_report = []
    # Statistics grouped by suppression description:
    # [hit count, bytes, objects].
    used_suppressions = {}
    for line in log_lines:
      line = line.rstrip()  # remove the trailing \n
      match = stack_line.match(line)
      if match:
        cur_stack.append(match.groups()[0])
        cur_report.append(line)
        continue
      else:
        if cur_stack:
          # Try to find the suppression that applies to the current leak stack.
          description = ''
          for supp in self._suppressions:
            if supp.Match(cur_stack):
              cur_stack = []
              description = supp.description
              break
          if cur_stack:
            # Print the report and set the return code to 1.
            print ('Leak of %d bytes in %d objects allocated from:'
                   % tuple(cur_leak_signature))
            print '\n'.join(cur_report)
            return_code = 1
          else:
            # Update the suppressions histogram.
            if description in used_suppressions:
              hits, bytes, objects = used_suppressions[description]
              hits += 1
              bytes += cur_leak_signature[0]
              objects += cur_leak_signature[1]
              used_suppressions[description] = [hits, bytes, objects]
            else:
              used_suppressions[description] = [1] + cur_leak_signature
        cur_stack = []
        cur_report = []
        cur_leak_signature = None
        match = leak_report.match(line)
        if match:
          cur_leak_signature = map(int, match.groups())
        else:
          print line
    # Print the list of suppressions used.
    if used_suppressions:
      print
      print '-----------------------------------------------------'
      print 'Suppressions used:'
      print '   count    bytes  objects name'
      histo = {}
      for description in used_suppressions:
        hits, bytes, objects = used_suppressions[description]
        line = '%8d %8d %8d %s' % (hits, bytes, objects, description)
        if hits in histo:
          histo[hits].append(line)
        else:
          histo[hits] = [line]
      keys = histo.keys()
      keys.sort()
      for count in keys:
        for line in histo[count]:
          print line
      print '-----------------------------------------------------'

    return return_code

  def RunTestsAndAnalyze(self):
    self.Execute()
    log_file = file(self.TMP_FILE, 'r')
    ret = self.Analyze(log_file)
    log_file.close()
    return ret

  def Main(self, args):
    self._args = args
    start = datetime.datetime.now()
    retcode = -1
    retcode = self.RunTestsAndAnalyze()
    end = datetime.datetime.now()
    seconds = (end - start).seconds
    hours = seconds / 3600
    seconds %= 3600
    minutes = seconds / 60
    seconds %= 60
    logging.info('elapsed time: %02d:%02d:%02d', hours, minutes, seconds)
    return retcode


def RunTool(args, supp_files):
  tool = HeapcheckWrapper(supp_files)
  return tool.Main(args[1:])
