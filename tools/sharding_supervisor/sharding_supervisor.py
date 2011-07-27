#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
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


from cStringIO import StringIO
import optparse
import os
import pty
import Queue
import random
import re
import subprocess
import sys
import threading


SS_USAGE = "Usage: python %prog [options] path/to/test [gtest_args]"
SS_DEFAULT_NUM_CORES = 4
SS_DEFAULT_SHARDS_PER_CORE = 5 # num_shards = cores * SHARDS_PER_CORE
SS_DEFAULT_RUNS_PER_CORE = 1 # num_workers = cores * RUNS_PER_CORE


def DetectNumCores():
  """Detects the number of cores on the machine.

  Returns:
    The number of cores on the machine or DEFAULT_NUM_CORES if it could not
    be found.
  """
  try:
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


def RunShard(test, num_shards, index, gtest_args, stdout, stderr):
  """Runs a single test shard in a subprocess.

  Returns:
    The Popen object representing the subprocess handle.
  """
  args = [test]
  args.extend(gtest_args)
  env = os.environ.copy()
  env["GTEST_TOTAL_SHARDS"] = str(num_shards)
  env["GTEST_SHARD_INDEX"] = str(index)
  return subprocess.Popen(
      args, stdout=stdout, stderr=stderr, env=env, bufsize=0)


class ShardRunner(threading.Thread):
  """Worker thread that manages a single shard at a time.

  Attributes:
    supervisor: The ShardingSupervisor that this worker reports to.
    counter: Called to get the next shard index to run.
  """

  def __init__(self, supervisor, counter, test_fail, test_timeout):
    """Inits ShardRunner with a supervisor and counter."""
    threading.Thread.__init__(self)
    self.supervisor = supervisor
    self.counter = counter
    self.test_fail = test_fail
    self.test_timeout = test_timeout

  def run(self):
    """Runs shards and outputs the results.

    Gets the next shard index from the supervisor, runs it in a subprocess,
    and collects the output. Each line is prefixed with 'N>', where N is the
    current shard index.
    """
    while True:
      try:
        index = self.counter.get_nowait()
      except Queue.Empty:
        break
      prefix = "%i>" % index
      chars = StringIO()
      shard_running = True
      shard = RunShard(
          self.supervisor.test, self.supervisor.num_shards, index,
          self.supervisor.gtest_args, subprocess.PIPE, subprocess.STDOUT)
      while shard_running:
        char = shard.stdout.read(1)
        if not char and shard.poll() is not None:
          shard_running = False
        chars.write(char)
        if char == "\n" or not shard_running:
          line = chars.getvalue()
          results = (self.test_fail.search(line) or
                     self.test_timeout.search(line))
          if results:
            log_line = prefix + "".join(results.group(0)) + "\n"
            self.supervisor.LogLineFailure(log_line)
          line = prefix + line
          self.supervisor.LogOutputLine(index, line)
          chars.close()
          chars = StringIO()
      self.supervisor.ShardIndexCompleted(index)
      if shard.returncode != 0:
        self.supervisor.LogShardFailure(index)


class ShardingSupervisor(object):
  """Supervisor object that handles the worker threads.

  Attributes:
    test: Name of the test to shard.
    num_shards: Total number of shards to split the test into.
    num_runs: Total number of worker threads to create for running shards.
    color: Indicates which coloring mode to use in the output.
    gtest_args: The options to pass to gtest.
    failure_log: List of statements from shard output indicating a failure.
    failed_shards: List of shards that contained failing tests.
  """

  SHARD_COMPLETED = object()

  def __init__(
      self, test, num_shards, num_runs, color, reorder, gtest_args):
    """Inits ShardingSupervisor with given options and gtest arguments."""
    self.test = test
    self.num_shards = num_shards
    self.num_runs = num_runs
    self.color = color
    self.reorder = reorder
    self.gtest_args = gtest_args
    self.failure_log = []
    self.failed_shards = []
    self.shards_completed = [False] * num_shards
    self.shard_output = [Queue.Queue() for _ in range(num_shards)]

  def ShardTest(self):
    """Runs the test and manages the worker threads.

    Runs the test and outputs a summary at the end. All the tests in the
    suite are run by creating (cores * runs_per_core) threads and
    (cores * shards_per_core) shards. When all the worker threads have
    finished, the lines saved in the failure_log are printed again.

    Returns:
      The number of shards that had failing tests.
    """

    # Regular expressions for parsing GTest logs. Test names look like
    #   SomeTestCase.SomeTest
    #   SomeName/SomeTestCase.SomeTest/1
    # This regex also matches SomeName.SomeTest/1 and
    # SomeName/SomeTestCase.SomeTest, which should be harmless.
    test_name_regex = r"((\w+/)?\w+\.\w+(/\d+)?)"

    # Regex for filtering out ANSI escape codes when using color.
    ansi_code_regex = r"(\x1b\[.*?[a-zA-Z])?"

    test_fail = re.compile(
        ansi_code_regex + "(\[\s+FAILED\s+\] )" + ansi_code_regex +
        test_name_regex)
    test_timeout = re.compile(
        "(Test timeout \([0-9]+ ms\) exceeded for )" + test_name_regex)

    workers = []
    counter = Queue.Queue()
    for i in range(self.num_shards):
      counter.put(i)

    # Disable stdout buffering to read shard output one character at a time.
    # This allows for shard crashes that do not end with a "\n".
    sys.stdout = os.fdopen(sys.stdout.fileno(), "w", 0)

    for i in range(self.num_runs):
      worker = ShardRunner(self, counter, test_fail, test_timeout)
      worker.start()
      workers.append(worker)
    if self.reorder:
      self.WaitForShards()
    else:
      for worker in workers:
        worker.join()

    # Re-enable stdout buffering.
    sys.stdout = sys.__stdout__

    return self.PrintSummary()

  def LogLineFailure(self, line):
    """Saves a line in the failure log to be printed at the end.

    Args:
      line: The line to save in the failure_log.
    """
    if line not in self.failure_log:
      self.failure_log.append(line)

  def LogShardFailure(self, index):
    """Records that a test in the given shard has failed.

    Args:
      index: The index of the failing shard.
    """
    self.failed_shards.append(index)

  def WaitForShards(self):
    for shard_index in range(self.num_shards):
      while True:
        line = self.shard_output[shard_index].get()
        if line is self.SHARD_COMPLETED:
          break
        sys.stdout.write(line)

  def LogOutputLine(self, index, line):
    if self.reorder:
      self.shard_output[index].put(line)
    else:
      sys.stdout.write(line)

  def ShardIndexCompleted(self, index):
    self.shard_output[index].put(self.SHARD_COMPLETED)

  def PrintSummary(self):
    """Prints a summary of the test results.

    If any shards had failing tests, the list is sorted and printed. Then all
    the lines that indicate a test failure are reproduced.

    Returns:
      The number of shards that had failing tests.
    """
    sys.stderr.write("\n")
    num_failed = len(self.failed_shards)
    if num_failed > 0:
      self.failed_shards.sort()
      if self.color:
        sys.stderr.write("\x1b[1;5;31m")
      sys.stderr.write("FAILED SHARDS: %s\n" % str(self.failed_shards))
    else:
      if self.color:
        sys.stderr.write("\x1b[1;5;32m")
      sys.stderr.write("ALL SHARDS PASSED!\n")
    if self.failure_log:
      if self.color:
        sys.stderr.write("\x1b[1;5;31m")
      sys.stderr.write("FAILED TESTS:\n")
      if self.color:
        sys.stderr.write("\x1b[0;37m")
      for line in self.failure_log:
        sys.stderr.write(line)
    if self.color:
      sys.stderr.write("\x1b[0;37m")
    return num_failed


def main():
  parser = optparse.OptionParser(usage=SS_USAGE)
  parser.add_option(
      "-n", "--shards_per_core", type="int", default=SS_DEFAULT_SHARDS_PER_CORE,
      help="number of shards to generate per CPU")
  parser.add_option(
      "-r", "--runs_per_core", type="int", default=SS_DEFAULT_RUNS_PER_CORE,
      help="number of shards to run in parallel per CPU")
  parser.add_option(
      "-c", "--color", action="store_true", default=sys.stdout.isatty(),
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
  parser.add_option("--random-seed", action="store_true",
      help="shuffle the tests with a random seed value")
  parser.disable_interspersed_args()
  (options, args) = parser.parse_args()

  if not args:
    parser.error("You must specify a path to test!")
  if not os.path.exists(args[0]):
    parser.error("%s does not exist!" % args[0])

  num_cores = DetectNumCores()

  if options.shards_per_core < 1:
    parser.error("You must have at least 1 shard per core!")
  num_shards = num_cores * options.shards_per_core

  if options.runs_per_core < 1:
    parser.error("You must have at least 1 run per core!")
  num_runs = num_cores * options.runs_per_core

  gtest_args = ["--gtest_color=%s" % {
      True: "yes", False: "no"}[options.color]] + args[1:]

  if options.random_seed:
    seed = random.randint(1, 99999)
    gtest_args.extend(["--gtest_shuffle", "--gtest_random_seed=%i" % seed])

  if options.runshard != None:
    # run a single shard and exit
    if (options.runshard < 0 or options.runshard >= num_shards):
      parser.error("Invalid shard number given parameters!")
    shard = RunShard(
        args[0], num_shards, options.runshard, gtest_args, None, None)
    shard.communicate()
    return shard.poll()

  # shard and run the whole test
  ss = ShardingSupervisor(args[0], num_shards, num_runs, options.color,
                          options.reorder, gtest_args)
  return ss.ShardTest()


if __name__ == "__main__":
  sys.exit(main())

