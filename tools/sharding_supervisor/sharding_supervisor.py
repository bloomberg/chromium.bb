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
import Queue
import random
import re
import sys
import threading

from stdio_buffer import StdioBuffer
from xml.dom import minidom

# Add tools/ to path
BASE_PATH = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.join(BASE_PATH, ".."))
try:
  import find_depot_tools
  # Fixes a bug in Windows where some shards die upon starting
  # TODO(charleslee): actually fix this bug
  import subprocess2 as subprocess
except ImportError:
  # Unable to find depot_tools, so just use standard subprocess
  import subprocess

SS_USAGE = "python %prog [options] path/to/test [gtest_args]"
SS_DEFAULT_NUM_CORES = 4
SS_DEFAULT_SHARDS_PER_CORE = 5  # num_shards = cores * SHARDS_PER_CORE
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
      env=env,
      bufsize=0,
      universal_newlines=True)


class ShardRunner(threading.Thread):
  """Worker thread that manages a single shard at a time.

  Attributes:
    supervisor: The ShardingSupervisor that this worker reports to.
    counter: Called to get the next shard index to run.
    test_start: Regex that detects when a test runs.
    test_ok: Regex that detects a passing test.
    test_fail: Regex that detects a failing test.
    current_test: The name of the currently running test.
  """

  def __init__(self, supervisor, counter, test_start, test_ok, test_fail):
    """Inits ShardRunner and sets the current test to nothing."""
    threading.Thread.__init__(self)
    self.supervisor = supervisor
    self.counter = counter
    self.test_start = test_start
    self.test_ok = test_ok
    self.test_fail = test_fail
    self.current_test = ""

  def ReportFailure(self, description, index, test_name):
    """Assembles and reports a failure line to be printed later."""
    log_line = "%s (%i): %s\n" % (description, index, test_name)
    self.supervisor.LogTestFailure(log_line)

  def ProcessLine(self, index, line):
    """Checks a shard output line for test status, and reports a failure or
    incomplete test if needed.
    """
    results = self.test_start.search(line)
    if results:
      if self.current_test:
        self.ReportFailure("INCOMPLETE", index, self.current_test)
      self.current_test = results.group(1)
      self.supervisor.IncrementTestCount()
      return

    results = self.test_ok.search(line)
    if results:
      self.current_test = ""
      return

    results = self.test_fail.search(line)
    if results:
      self.ReportFailure("FAILED", index, results.group(1))
      self.current_test = ""

  def run(self):
    """Runs shards and outputs the results.

    Gets the next shard index from the supervisor, runs it in a subprocess,
    and collects the output. The output is read character by character in
    case the shard crashes without an ending newline. Each line is processed
    as it is finished.
    """
    while True:
      try:
        index = self.counter.get_nowait()
      except Queue.Empty:
        break
      shard_running = True
      shard = RunShard(
          self.supervisor.test, self.supervisor.total_shards, index,
          self.supervisor.gtest_args, subprocess.PIPE, subprocess.PIPE)
      buffer = StdioBuffer(shard)
      # Spawn two threads to collect stdio output
      stdout_collector_thread = buffer.handle_pipe(sys.stdout, shard.stdout)
      stderr_collector_thread = buffer.handle_pipe(sys.stderr, shard.stderr)
      while shard_running:
        pipe, line = buffer.readline()
        if pipe is None and line is None:
          shard_running = False
        if not line and not shard_running:
          break
        self.ProcessLine(index, line)
        self.supervisor.LogOutputLine(index, line, pipe)
      stdout_collector_thread.join()
      stderr_collector_thread.join()
      if self.current_test:
        self.ReportFailure("INCOMPLETE", index, self.current_test)
      self.supervisor.ShardIndexCompleted(index)
      if shard.returncode != 0:
        self.supervisor.LogShardFailure(index)


class ShardingSupervisor(object):
  """Supervisor object that handles the worker threads.

  Attributes:
    test: Name of the test to shard.
    num_shards_to_run: Total number of shards to split the test into.
    num_runs: Total number of worker threads to create for running shards.
    color: Indicates which coloring mode to use in the output.
    original_order: True if shard output should be printed as it comes.
    prefix: True if each line should indicate the shard index.
    retry_percent: Integer specifying the max percent of tests to retry.
    gtest_args: The options to pass to gtest.
    failed_tests: List of statements from shard output indicating a failure.
    failed_shards: List of shards that contained failing tests.
    shards_completed: List of flags indicating which shards have finished.
    shard_output: Buffer that stores output from each shard as (stdio, line).
    test_counter: Stores the total number of tests run.
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

  def __init__(self, test, num_shards_to_run, num_runs, color, original_order,
               prefix, retry_percent, timeout, total_slaves, slave_index,
               gtest_args):
    """Inits ShardingSupervisor with given options and gtest arguments."""
    self.test = test
    # Number of shards to run locally.
    self.num_shards_to_run = num_shards_to_run
    # Total shards in the test suite running across all slaves.
    self.total_shards = num_shards_to_run * total_slaves
    self.slave_index = slave_index
    self.num_runs = num_runs
    self.color = color
    self.original_order = original_order
    self.prefix = prefix
    self.retry_percent = retry_percent
    self.timeout = timeout
    self.gtest_args = gtest_args
    self.failed_tests = []
    self.failed_shards = []
    self.shards_completed = [False] * self.num_shards_to_run
    self.shard_output = [Queue.Queue() for _ in range(self.num_shards_to_run)]
    self.test_counter = itertools.count()

  def ShardTest(self):
    """Runs the test and manages the worker threads.

    Runs the test and outputs a summary at the end. All the tests in the
    suite are run by creating (cores * runs_per_core) threads and
    (cores * shards_per_core) shards. When all the worker threads have
    finished, the lines saved in failed_tests are printed again. If enabled,
    and failed tests that do not have FLAKY or FAILS in their names are run
    again, serially, and the results are printed.

    Returns:
      1 if some unexpected (not FLAKY or FAILS) tests failed, 0 otherwise.
    """

    # Regular expressions for parsing GTest logs. Test names look like
    #   SomeTestCase.SomeTest
    #   SomeName/SomeTestCase.SomeTest/1
    # This regex also matches SomeName.SomeTest/1 and
    # SomeName/SomeTestCase.SomeTest, which should be harmless.
    test_name_regex = r"((\w+/)?\w+\.\w+(/\d+)?)"

    # Regex for filtering out ANSI escape codes when using color.
    ansi_regex = r"(?:\x1b\[.*?[a-zA-Z])?"

    test_start = re.compile(
        ansi_regex + r"\[\s+RUN\s+\] " + ansi_regex + test_name_regex)
    test_ok = re.compile(
        ansi_regex + r"\[\s+OK\s+\] " + ansi_regex + test_name_regex)
    test_fail = re.compile(
        ansi_regex + r"\[\s+FAILED\s+\] " + ansi_regex + test_name_regex)

    workers = []
    counter = Queue.Queue()
    start_point = self.num_shards_to_run * self.slave_index
    for i in range(start_point, start_point + self.num_shards_to_run):
      counter.put(i)

    for i in range(self.num_runs):
      worker = ShardRunner(
          self, counter, test_start, test_ok, test_fail)
      worker.start()
      workers.append(worker)
    if self.original_order:
      for worker in workers:
        worker.join()
    else:
      self.WaitForShards()

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
    else:
      self.WriteText(sys.stdout, "\nALL SHARDS PASSED!\n", "\x1b[1;5;32m")
    self.PrintSummary(self.failed_tests)
    if self.retry_percent < 0:
      return len(self.failed_shards) > 0

    self.failed_tests = [x for x in self.failed_tests if x.find("FAILS_") < 0]
    self.failed_tests = [x for x in self.failed_tests if x.find("FLAKY_") < 0]
    if not self.failed_tests:
      return 0
    return self.RetryFailedTests()

  def LogTestFailure(self, line):
    """Saves a line in the lsit of failed tests to be printed at the end."""
    if line not in self.failed_tests:
      self.failed_tests.append(line)

  def LogShardFailure(self, index):
    """Records that a test in the given shard has failed."""
    self.failed_shards.append(index)

  def WaitForShards(self):
    """Prints the output from each shard in consecutive order, waiting for
    the current shard to finish before starting on the next shard.
    """
    try:
      for shard_index in range(self.num_shards_to_run):
        while True:
          try:
            pipe, line = self.shard_output[shard_index].get(True, self.timeout)
          except Queue.Empty:
            # Shard timed out, notice failure and move on.
            self.LogShardFailure(shard_index)
            # TODO(maruel): Print last test. It'd be simpler to have the
            # processing in the main thread.
            # TODO(maruel): Make sure the worker thread terminates.
            sys.stdout.write('TIMED OUT\n\n')
            LogTestFailure(
                'FAILURE: SHARD %d TIMED OUT; %d seconds' % (
                    shard_index, self.timeout))
            break
          if line is self.SHARD_COMPLETED:
            break
          sys.stdout.write(line)
    except:
      sys.stdout.flush()
      print 'CAUGHT EXCEPTION: dumping remaining data:'
      for shard_index in range(self.num_shards_to_run):
        while True:
          try:
            pipe, line = self.shard_output[shard_index].get(False)
          except Queue.Empty:
            # Shard timed out, notice failure and move on.
            self.LogShardFailure(shard_index)
            break
          if line is self.SHARD_COMPLETED:
            break
          sys.stdout.write(line)
      raise

  def LogOutputLine(self, index, line, pipe=sys.stdout):
    """Either prints the shard output line immediately or saves it in the
    output buffer, depending on the settings. Also optionally adds a prefix.
    Adds a (sys.stdout, line) or (sys.stderr, line) tuple in the output queue.
    """
    # Fix up the index.
    array_index = index - (self.num_shards_to_run * self.slave_index)
    if self.prefix:
      line = "%i>%s" % (index, line)
    if self.original_order:
      pipe.write(line)
    else:
      self.shard_output[array_index].put((pipe, line))

  def IncrementTestCount(self):
    """Increments the number of tests run. This is relevant to the
    --retry-percent option.
    """
    self.test_counter.next()

  def ShardIndexCompleted(self, index):
    """Records that a shard has finished so the output from the next shard
    can now be printed.
    """
    # Fix up the index.
    array_index = index - (self.num_shards_to_run * self.slave_index)
    self.shard_output[array_index].put((sys.stdout, self.SHARD_COMPLETED))

  def RetryFailedTests(self):
    """Reruns any failed tests serially and prints another summary of the
    results if no more than retry_percent failed.
    """
    num_tests_run = self.test_counter.next()
    if len(self.failed_tests) > self.retry_percent * num_tests_run:
      sys.stdout.write("\nNOT RETRYING FAILED TESTS (too many failed)\n")
      return 1
    self.WriteText(sys.stdout, "\nRETRYING FAILED TESTS:\n", "\x1b[1;5;33m")
    sharded_description = re.compile(r": (?:\d+>)?(.*)")
    gtest_filters = [sharded_description.search(line).group(1)
                     for line in self.failed_tests]
    sys.stdout.write("\nRETRY GTEST FILTERS: %r\n" % gtest_filters)
    failed_retries = []

    for test_filter in gtest_filters:
      args = [self.test, "--gtest_filter=" + test_filter]
      # Don't update the xml output files during retry.
      stripped_gtests_args = RemoveGTestOutput(self.gtest_args)
      args.extend(stripped_gtests_args)
      sys.stdout.write("\nRETRY COMMAND: %r\n" % args)
      rerun = subprocess.Popen(args)
      rerun.wait()
      if rerun.returncode != 0:
        failed_retries.append(test_filter)

    self.WriteText(sys.stdout, "RETRY RESULTS:\n", "\x1b[1;5;33m")
    self.PrintSummary(failed_retries)
    return len(failed_retries) > 0

  def PrintSummary(self, failed_tests):
    """Prints a summary of the test results.

    If any shards had failing tests, the list is sorted and printed. Then all
    the lines that indicate a test failure are reproduced.
    """
    if failed_tests:
      self.WriteText(sys.stdout, "FAILED TESTS:\n", "\x1b[1;5;31m")
      for line in failed_tests:
        sys.stdout.write(line)
    else:
      self.WriteText(sys.stdout, "ALL TESTS PASSED!\n", "\x1b[1;5;32m")

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
      "--reorder", action="store_true",
      help="ensure that all output from an earlier shard is printed before"
      " output from a later shard")
  # TODO(charleslee): for backwards compatibility with master.cfg file
  parser.add_option(
      "--original-order", action="store_true",
      help="print shard output in its orginal jumbled order of execution"
      " (useful for debugging flaky tests)")
  parser.add_option(
      "--prefix", action="store_true",
      help="prefix each line of shard output with 'N>', where N is the shard"
      " index (forced True when --original-order is True)")
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

  if options.original_order:
    options.prefix = True

  # TODO(charleslee): for backwards compatibility with buildbot's log_parser
  if options.reorder:
    options.original_order = False
    options.prefix = True

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
    shard.communicate()
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
      options.original_order, options.prefix, options.retry_percent,
      options.timeout, options.total_slaves, options.slave_index, gtest_args)
  return ss.ShardTest()


if __name__ == "__main__":
  sys.exit(main())
