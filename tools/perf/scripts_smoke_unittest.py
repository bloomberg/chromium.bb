# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys
import unittest


class ScriptsSmokeTest(unittest.TestCase):

  perf_dir = os.path.dirname(__file__)

  def RunPerfScript(self, command):
    args = [sys.executable] + command.split(' ')
    proc = subprocess.Popen(args, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, cwd=self.perf_dir)
    stdout = proc.communicate()[0]
    return_code = proc.returncode
    return return_code, stdout

  def testRunBenchmarkHelp(self):
    return_code, stdout = self.RunPerfScript('run_benchmark help')
    self.assertIn('Available commands are', stdout)
    self.assertEquals(return_code, 0)

  def testRunBenchmarkRunListsOutBenchmarks(self):
    return_code, stdout = self.RunPerfScript('run_benchmark run')
    self.assertIn('Pass --browser to list benchmarks', stdout)
    self.assertNotEquals(return_code, 0)

  def testRunBenchmarkRunNonExistingBenchmark(self):
    return_code, stdout = self.RunPerfScript('run_benchmark foo')
    self.assertIn('No benchmark named "foo"', stdout)
    self.assertNotEquals(return_code, 0)

  def testRunBenchmarkListListsOutBenchmarks(self):
    return_code, stdout = self.RunPerfScript('run_benchmark list')
    self.assertIn('Pass --browser to list benchmarks', stdout)
    self.assertIn('kraken', stdout)
    self.assertEquals(return_code, 0)

  def testRunRecordWprHelp(self):
    return_code, stdout = self.RunPerfScript('record_wpr')
    self.assertIn('optional arguments:', stdout)
    self.assertEquals(return_code, 0)
