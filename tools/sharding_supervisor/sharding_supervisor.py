#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Shards a given test suite and runs the shards in parallel.

ShardingSupervisor is called to process the command line options and creates
the specified number of worker threads. These threads then run each shard of
the test in a separate process and report on the results. When all the shards
have been completed, the supervisor reprints any lines indicating a test
failure for convenience. If only one shard is to be run, a single subprocess
is started for that shard and the output is identical to gtest's output.
"""


import cStringIO
import itertools
import optparse
import os
import random
import re
import subprocess
import sys
import threading

from xml.dom import minidom

# Add tools/ to path
BASE_PATH = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.join(BASE_PATH, ".."))

SS_USAGE = "python %prog [options] path/to/test [gtest_args]"
SS_DEFAULT_NUM_CORES = 4
SS_DEFAULT_SHARDS_PER_CORE = 1  # num_shards = cores * SHARDS_PER_CORE
SS_DEFAULT_RUNS_PER_CORE = 1  # num_workers = cores * RUNS_PER_CORE
SS_DEFAULT_RETRY_PERCENT = 5  # --retry-failed ignored if more than 5% fail
SS_DEFAULT_TIMEOUT = 530  # Slightly less than buildbot's default 600 seconds


def DetectNumCores():
  """Detects the number of cores on the machine.

  Returns:
    The number of cores on the machine or DEFAULT_NUM_CORES if it could not
    be found.
  """
  try:
    # Override on some Chromium Valgrind bots.
    if "CHROME_VALGRIND_NUMCPUS" in os.environ:
      return int(os.environ["CHROME_VALGRIND_NUMCPUS"])
    # Linux, Unix, MacOS
    if hasattr(os, "sysconf"):
      if "SC_NPROCESSORS_ONLN" in os.sysconf_names:
        # Linux, Unix
        return int(os.sysconf("SC_NPROCESSORS_ONLN"))
      else:
        # OSX
        return int(os.popen2("sysctl -n hw.ncpu")[1].read())
    # Windows
    return int(os.environ["NUMBER_OF_PROCESSORS"])
  except ValueError:
    return SS_DEFAULT_NUM_CORES


def GetGTestOutput(args):
  """Extracts gtest_output from the args. Returns none if not present."""

  for arg in args:
    if '--gtest_output=' in arg:
      return arg.split('=')[1]
  return None


def AppendToGTestOutput(gtest_args, value):
  args = gtest_args[:]
  current_value = GetGTestOutput(args)
  if not current_value:
    return gtest_args

  current_arg = '--gtest_output=' + current_value
  args.remove(current_arg)
  args.append('--gtest_output=' + current_value + value)
  return args


def RemoveGTestOutput(gtest_args):
  args = gtest_args[:]
  current_value = GetGTestOutput(args)
  if not current_value:
    return gtest_args

  args.remove('--gtest_output=' + current_value)
  return args


def AppendToXML(final_xml, generic_path, shard):
  """Combine the shard xml file with the final xml file."""

  path = generic_path + str(shard)

  try:
    with open(path) as shard_xml_file:
      shard_xml = minidom.parse(shard_xml_file)
  except IOError:
    # If the shard crashed, gtest will not have generated an xml file.
    return final_xml

  if not final_xml:
    # Out final xml is empty, let's prepopulate it with the first one we see.
    return shard_xml

  shard_node = shard_xml.documentElement
  final_node = final_xml.documentElement

  testcases = shard_node.getElementsByTagName('testcase')
  final_testcases = final_node.getElementsByTagName('testcase')

  final_testsuites = final_node.getElementsByTagName('testsuite')
  final_testsuites_by_name = dict(
      (suite.getAttribute('name'), suite) for suite in final_testsuites)

  for testcase in testcases:
    name = testcase.getAttribute('name')
    classname = testcase.getAttribute('classname')
    failures = testcase.getElementsByTagName('failure')
    status = testcase.getAttribute('status')
    elapsed = testcase.getAttribute('time')

    # don't bother updating the final xml if there is no data.
    if status == 'notrun':
      continue

    # Look in our final xml to see if it's there.
    # There has to be a better way...
    merged_into_final_testcase = False
    for final_testcase in final_testcases:
      final_name = final_testcase.getAttribute('name')
      final_classname = final_testcase.getAttribute('classname')
      if final_name == name and final_classname == classname:
        # We got the same entry.
        final_testcase.setAttribute('status', status)
        final_testcase.setAttribute('time', elapsed)
        for failure in failures:
          final_testcase.appendChild(failure)
        merged_into_final_testcase = True

    # We couldn't find an existing testcase to merge the results into, so we
    # copy the node into the existing test suite.
    if not merged_into_final_testcase:
      testsuite = testcase.parentNode
      final_testsuite = final_testsuites_by_name[testsuite.getAttribute('name')]
      final_testsuite.appendChild(testcase)

  return final_xml


def RunShard(test, total_shards, index, gtest_args, stdout, stderr):
  """Runs a single test shard in a subprocess.

  Returns:
    The Popen object representing the subprocess handle.
  """
  args = [test]

  # If there is a gtest_output
  test_args = AppendToGTestOutput(gtest_args, str(index))
  args.extend(test_args)
  env = os.environ.copy()
  env["GTEST_TOTAL_SHARDS"] = str(total_shards)
  env["GTEST_SHARD_INDEX"] = str(index)

  # Use a unique log file for each shard
  # Allows ui_tests to be run in parallel on the same machine
  env["CHROME_LOG_FILE"] = "chrome_log_%d" % index

  return subprocess.Popen(
      args, stdout=stdout,
      stderr=stderr,
      shell=False,
      env=env,
      bufsize=0,
      universal_newlines=True)


class ShardRunner(threading.Thread):
  """Worker thread that manages a single shard at a time.

  Attributes:
    supervisor: The ShardingSupervisor that this worker reports to.
    shard_index: Shard index to run.
  """

  def __init__(self, supervisor, shard_index):
    """Inits ShardRunner."""
    threading.Thread.__init__(self)
    self.supervisor = supervisor
    self.shard_index = shard_index

    self.stdout = ''
    self.shard = RunShard(
        self.supervisor.test, self.supervisor.total_shards, self.shard_index,
        self.supervisor.gtest_args, subprocess.PIPE, subprocess.STDOUT)

    # Do not block program exit in case we have to leak the thread.
    self.daemon = True
    self.flag = threading.Event()

  def run(self):
    """Runs the shard and outputs the results."""
    self.stdout, _ = self.shard.communicate()
    self.flag.set()


class ShardingSupervisor(object):
  """Supervisor object that handles the worker threads.

  Attributes:
    test: Name of the test to shard.
    num_shards_to_run: Total number of shards to split the test into.
    num_runs: Total number of worker threads to create for running shards.
    color: Indicates which coloring mode to use in the output.
    retry_percent: Integer specifying the max percent of tests to retry.
    gtest_args: The options to pass to gtest.
    failed_shards: List of shards that contained failing tests.
    total_slaves: Total number of slaves running this test.
    slave_index: Current slave to run tests for.

  If total_slaves is set, we run only a subset of the tests. This is meant to be
  used when we want to shard across machines as well as across cpus. In that
  case the number of shards to execute will be the same, but they will be
  smaller, as the total number of shards in the test suite will be multiplied
  by 'total_slaves'.

  For example, if you are on a quad core machine, the sharding supervisor by
  default will use 20 shards for the whole suite. However, if you set
  total_slaves to 2, it will split the suite in 40 shards and will only run
  shards [0-19] or shards [20-39] depending if you set slave_index to 0 or 1.
  """

  SHARD_COMPLETED = object()

  def __init__(self, test, num_shards_to_run, num_runs, color,
               retry_percent, timeout, total_slaves, slave_index, gtest_args):
    """Inits ShardingSupervisor with given options and gtest arguments."""
    self.test = test
    # Number of shards to run locally.
    self.num_shards_to_run = num_shards_to_run
    # Total shards in the test suite running across all slaves.
    self.total_shards = num_shards_to_run * total_slaves
    self.slave_index = slave_index
    self.num_runs = num_runs
    self.color = color
    self.retry_percent = retry_percent
    self.timeout = timeout
    self.gtest_args = gtest_args
    self.failed_shards = []

  def ShardTest(self):
    """Runs the test and manages the worker threads.

    Runs the test and outputs a summary at the end. All the tests in the
    suite are run by creating (cores * runs_per_core) threads and
    (cores * shards_per_core) shards.

    Returns:
      1 on failure, 0 otherwise.
    """

    workers = []
    start_point = self.num_shards_to_run * self.slave_index
    for i in range(start_point, start_point + self.num_shards_to_run):
      worker = ShardRunner(self, i)
      worker.start()
      workers.append(worker)

    for worker in workers:
      worker.flag.wait(timeout=self.timeout)
      if worker.is_alive():
        # Print something to prevent buildbot from killing us
        # because of no output for long time.
        print 'TIMED OUT WAITING FOR SHARD, KILLING'
        sys.stdout.flush()

        # Make sure we count the timeout as a failure.
        self.LogShardFailure(worker.shard_index)

        worker.shard.kill()
        worker.flag.wait(timeout=self.timeout)
      else:
        if worker.shard.returncode != 0:
          self.LogShardFailure(worker.shard_index)

      if worker.is_alive():
        print 'LEAKING THREAD, SORRY'
      else:
        # Note: print guarantees there will be a newline at the end. This helps
        # preventing mixed-up logs.
        print worker.stdout

      sys.stdout.flush()

    # All the shards are done.  Merge all the XML files and generate the
    # main one.
    output_arg = GetGTestOutput(self.gtest_args)
    if output_arg:
      xml, xml_path = output_arg.split(':', 1)
      assert(xml == 'xml')
      final_xml = None
      for i in range(start_point, start_point + self.num_shards_to_run):
        final_xml = AppendToXML(final_xml, xml_path, i)

      if final_xml:
        with open(xml_path, 'w') as final_file:
          final_xml.writexml(final_file)

    num_failed = len(self.failed_shards)
    if num_failed > 0:
      self.failed_shards.sort()
      self.WriteText(sys.stdout,
                     "\nFAILED SHARDS: %s\n" % str(self.failed_shards),
                     "\x1b[1;5;31m")
      return 1
    else:
      self.WriteText(sys.stdout, "\nALL SHARDS PASSED!\n", "\x1b[1;5;32m")
      return 0

  def LogShardFailure(self, index):
    """Records that a test in the given shard has failed."""
    # There might be some duplicates in case a shard times out.
    if index not in self.failed_shards:
      self.failed_shards.append(index)

  def WriteText(self, pipe, text, ansi):
    """Writes the text to the pipe with the ansi escape code, if colored
    output is set, for Unix systems.
    """
    if self.color:
      pipe.write(ansi)
    pipe.write(text)
    if self.color:
      pipe.write("\x1b[m")


def main():
  parser = optparse.OptionParser(usage=SS_USAGE)
  parser.add_option(
      "-n", "--shards_per_core", type="int", default=SS_DEFAULT_SHARDS_PER_CORE,
      help="number of shards to generate per CPU")
  parser.add_option(
      "-r", "--runs_per_core", type="int", default=SS_DEFAULT_RUNS_PER_CORE,
      help="number of shards to run in parallel per CPU")
  parser.add_option(
      "-c", "--color", action="store_true",
      default=sys.platform != "win32" and sys.stdout.isatty(),
      help="force color output, also used by gtest if --gtest_color is not"
      " specified")
  parser.add_option(
      "--no-color", action="store_false", dest="color",
      help="disable color output")
  parser.add_option(
      "-s", "--runshard", type="int", help="single shard index to run")
  parser.add_option(
      "--random-seed", action="store_true",
      help="shuffle the tests with a random seed value")
  parser.add_option(
      "--retry-failed", action="store_true",
      help="retry tests that did not pass serially")
  parser.add_option(
      "--retry-percent", type="int",
      default=SS_DEFAULT_RETRY_PERCENT,
      help="ignore --retry-failed if more than this percent fail [0, 100]"
      " (default = %i)" % SS_DEFAULT_RETRY_PERCENT)
  parser.add_option(
      "-t", "--timeout", type="int", default=SS_DEFAULT_TIMEOUT,
      help="timeout in seconds to wait for a shard (default=%default s)")
  parser.add_option(
      "--total-slaves", type="int", default=1,
      help="if running a subset, number of slaves sharing the test")
  parser.add_option(
      "--slave-index", type="int", default=0,
      help="if running a subset, index of the slave to run tests for")

  parser.disable_interspersed_args()
  (options, args) = parser.parse_args()

  if not args:
    parser.error("You must specify a path to test!")
  if not os.path.exists(args[0]):
    parser.error("%s does not exist!" % args[0])

  num_cores = DetectNumCores()

  if options.shards_per_core < 1:
    parser.error("You must have at least 1 shard per core!")
  num_shards_to_run = num_cores * options.shards_per_core

  if options.runs_per_core < 1:
    parser.error("You must have at least 1 run per core!")
  num_runs = num_cores * options.runs_per_core

  test = args[0]
  gtest_args = ["--gtest_color=%s" % {
      True: "yes", False: "no"}[options.color]] + args[1:]

  if options.random_seed:
    seed = random.randint(1, 99999)
    gtest_args.extend(["--gtest_shuffle", "--gtest_random_seed=%i" % seed])

  if options.retry_failed:
    if options.retry_percent < 0 or options.retry_percent > 100:
      parser.error("Retry percent must be an integer [0, 100]!")
  else:
    options.retry_percent = -1

  if options.runshard != None:
    # run a single shard and exit
    if (options.runshard < 0 or options.runshard >= num_shards_to_run):
      parser.error("Invalid shard number given parameters!")
    shard = RunShard(
        test, num_shards_to_run, options.runshard, gtest_args, None, None)
    shard.communicate(timeout=self.timeout)
    return shard.poll()

  # When running browser_tests, load the test binary into memory before running
  # any tests. This is needed to prevent loading it from disk causing the first
  # run tests to timeout flakily. See: http://crbug.com/124260
  if "browser_tests" in test:
    args = [test]
    args.extend(gtest_args)
    args.append("--warmup")
    result = subprocess.call(args,
                             bufsize=0,
                             universal_newlines=True)
    # If the test fails, don't run anything else.
    if result != 0:
      return result

  # shard and run the whole test
  ss = ShardingSupervisor(
      test, num_shards_to_run, num_runs, options.color,
      options.retry_percent, options.timeout, options.total_slaves,
      options.slave_index, gtest_args)
  return ss.ShardTest()


if __name__ == "__main__":
  sys.exit(main())
