#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script takes in a list of test targets to be run on both Linux and
# Fuchsia devices and then compares their output to each other, extracting the
# relevant performance data from the output of gtest.

import os
import re
import subprocess
import sys

from collections import defaultdict
from typing import *

import target_spec


def RunCommand(command: List[str], msg: str,
               ignore_errors: bool = False) -> str:
  "One-shot start and complete command with useful default kwargs"
  proc = subprocess.run(
      command,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      stdin=subprocess.DEVNULL)
  out = proc.stdout.decode("utf-8", errors="ignore")
  err = proc.stderr.decode("utf-8", errors="ignore")
  if proc.returncode != 0:
    sys.stderr.write("{}\nreturn code: {}\nstdout: {}\nstderr: {}".format(
        msg, proc.returncode, out, err))
    if not ignore_errors:
      raise subprocess.SubprocessError(
          "Command failed to complete successfully. {}".format(command))
  return out


class TestTarget(object):
  """TestTarget encapsulates a single BUILD.gn target, extracts a name from the
  target string, and manages the building and running of the target for both
  Linux and Fuchsia.
  """

  def __init__(self, target: str) -> None:
    self._target = target
    self._name = target.split(":")[-1]
    filter_file = "testing/buildbot/filters/fuchsia.{}.filter".format(
        self._name)
    if not os.path.isfile(filter_file):
      self._filter_flag = ""
    else:
      self._filter_flag = "--test-launcher-filter-file={}".format(filter_file)

  def ExecFuchsia(self, out_dir: str, run_locally: bool) -> str:
    runner_name = "{}/bin/run_{}".format(out_dir, self._name)
    command = [runner_name, self._filter_flag]
    if not run_locally:
      command.append("-d")
    result = RunCommand(
        command,
        "Test {} failed on fuchsia!".format(self._target),
        ignore_errors=True)
    return result

  def ExecLinux(self, out_dir: str, run_locally: bool) -> str:
    local_path = "{}/{}".format(out_dir, self._name)
    command = []  # type: List[str]
    if not run_locally:
      user = target_spec.linux_device_user
      ip = target_spec.linux_device_hostname
      host_machine = "{0}@{1}".format(user, ip)
      # Next is the transfer of all the directories to the destination device.
      self.TransferDependencies(out_dir, host_machine)
      command = [
          "ssh", "{}@{}".format(user, ip), "xvfb-run -a {1}/{0}/{1} {2}".format(
              out_dir, self._name, self._filter_flag)
      ]
    else:
      command = [local_path, self._filter_flag]
    result = RunCommand(
        command,
        "Test {} failed on linux!".format(self._target),
        ignore_errors=True)
    # Clean up the copy of the test files on the host after execution
    RunCommand(["rm", "-rf", self._name],
               "Failed to remove host directory for {}".format(self._target))
    return result

  def TransferDependencies(self, out_dir: str, host: str):
    gn_desc = ["gn", "desc", out_dir, self._target, "runtime_deps"]
    out = RunCommand(
        gn_desc, "Failed to get dependencies of target {}".format(self._target))

    paths = []
    for line in out.split("\n"):
      if line == "":
        continue
      line = out_dir + "/" + line.strip()
      line = os.path.abspath(line)
      paths.append(line)
    common = os.path.commonpath(paths)
    paths = [os.path.relpath(path, common) for path in paths]

    archive_name = self._name + ".tar.gz"
    # Compress the dependencies of the test.
    RunCommand(
        ["tar", "-czf", archive_name] + paths,
        "{} dependency compression failed".format(self._target),
    )
    # Make sure the containing directory exists on the host, for easy cleanup.
    RunCommand(["ssh", host, "mkdir -p {}".format(self._name)],
               "Failed to create directory on host for {}".format(self._target))
    # Transfer the test deps to the host.
    RunCommand(
        [
            "scp", archive_name, "{}:{}/{}".format(host, self._name,
                                                   archive_name)
        ],
        "{} dependency transfer failed".format(self._target),
    )
    # Decompress the dependencies once they're on the host.
    RunCommand(
        [
            "ssh", host, "tar -xzf {0}/{1} -C {0}".format(
                self._name, archive_name)
        ],
        "{} dependency decompression failed".format(self._target),
    )
    # Clean up the local copy of the archive that is no longer needed.
    RunCommand(
        ["rm", archive_name],
        "{} dependency archive cleanup failed".format(self._target),
    )


def ExtractParts(string: str) -> Tuple[str, float, str]:
  """This function accepts lines like the one that follow this sentence, and
  attempts to extract all of the relevant pieces of information from it.

   task: 1_threads_scheduling_to_io_pump= .47606626678091973 us/task

  The above line would be split into chunks as follows:

  info=data units

  info and units can be any string, and data must be a valid float. data and
  units must be separated by a space, and info and data must be separated by
  at least an '='
  """
  pieces = string.split("=")
  info = pieces[0].strip()
  measure = pieces[1].strip().split(" ")
  data = float(measure[0])
  units = measure[1].strip()
  return info, data, units


class ResultLine(object):
  """This class is a single line of the comparison between linux and fuchsia.
  GTests output several lines of important info per test, which are collected,
  and then the pertinent pieces of information are extracted and stored in a
  ResultLine for each line, containing a shared description and unit, as well as
  linux and fuchsia performance scores.
  """

  @classmethod
  def FromLines(cls, linux_line: str, fuchsia_line: str):
    linux_info, linux_val, linux_unit = ExtractParts(linux_line)
    fuchsia_info, fuchsia_val, fuchsia_unit = ExtractParts(fuchsia_line)

    if linux_info != fuchsia_info:
      print("Info mismatch! fuchsia was: {}".format(fuchsia_info))
    if linux_unit != fuchsia_unit:
      print("Unit mismatch! fuchsia was: {}".format(fuchsia_unit))
    return ResultLine(linux_info, linux_val, fuchsia_val, fuchsia_unit)

  def __init__(self, desc: str, linux: float, fuchsia: float,
               unit: str) -> None:
    self.desc = desc  # type: str
    self.linux = linux  # type: float
    self.fuchsia = fuchsia  # type: float
    self.unit = unit  # type: str

  def comparison(self) -> float:
    return (self.fuchsia / self.linux) * 100.0

  def ToCsvFormat(self) -> str:
    return ",".join([
        self.desc.replace(",", ";"),
        str(self.linux),
        str(self.fuchsia),
        str(self.comparison()),
        self.unit,
    ])

  def __format__(self, format_spec: str) -> str:
    return "{} in {}: linux:{}, fuchsia:{}, ratio:{}".format(
        self.desc, self.unit, self.linux, self.fuchsia, self.comparison())


class TestComparison(object):
  """This class represents a single test target, and all of its informative
  lines of output for each test case, extracted into statistical comparisons of
  this run on linux v fuchsia.
  """

  def __init__(self, name: str, tests: Dict[str, List[ResultLine]]) -> None:
    self.suite_name = name
    self.tests = tests

  def MakeCsvFormat(self) -> str:
    out_lines = []  # type: List[str]
    for test_name, lines in self.tests.items():
      for line in lines:
        out_lines.append("{},{},{}".format(self.suite_name, test_name,
                                           line.ToCsvFormat()))
    return "\n".join(out_lines) + "\n"
    # Has a + "\n" to simplify writing a list of these to a file

  def __format__(self, format_spec: str) -> str:
    out_lines = [self.suite_name]
    for test_case, lines in self.tests.items():
      out_lines.append("  {}".format(test_case))
      for line in lines:
        out_lines.append("    {}".format(line))
    return "\n".join(out_lines)


def ExtractCases(out_lines: List[str]) -> Dict[str, List[str]]:
  """ExtractCases attempts to associate GTest names to the lines of output that
  they produce. Given a list of input like the following:

  [==========] Running 24 tests from 10 test cases.
  [----------] Global test environment set-up.
  [----------] 9 tests from ScheduleWorkTest
  [ RUN      ] ScheduleWorkTest.ThreadTimeToIOFromOneThread
  *RESULT task: 1_threads_scheduling_to_io_pump= .47606626678091973 us/task
  RESULT task_min_batch_time: 1_threads_scheduling_to_io_pump= .335 us/task
  RESULT task_max_batch_time: 1_threads_scheduling_to_io_pump= 5.071 us/task
  *RESULT task_thread_time: 1_threads_scheduling_to_io_pump= .3908787013 us/task
  [       OK ] ScheduleWorkTest.ThreadTimeToIOFromOneThread (5352 ms)
  [ RUN      ] ScheduleWorkTest.ThreadTimeToIOFromTwoThreads
  *RESULT task: 2_threads_scheduling_to_io_pump= 6.216794903666874 us/task
  RESULT task_min_batch_time: 2_threads_scheduling_to_io_pump= 2.523 us/task
  RESULT task_max_batch_time: 2_threads_scheduling_to_io_pump= 142.989 us/task
  *RESULT task_thread_time: 2_threads_scheduling_to_io_pump= 2.02621823 us/task
  [       OK ] ScheduleWorkTest.ThreadTimeToIOFromTwoThreads (5022 ms)
  [ RUN      ] ScheduleWorkTest.ThreadTimeToIOFromFourThreads

  {'ScheduleWorkTest.ThreadTimeToIOFromOneThread':[
    'task: 1_threads_scheduling_to_io_pump= .47606626678091973 us/task',
    'task_min_batch_time: 1_threads_scheduling_to_io_pump= .335 us/task',
    'task_max_batch_time: 1_threads_scheduling_to_io_pump= 5.071 us/task',
    'task_thread_time: 1_threads_scheduling_to_io_pump= .390834314 us/task'],
  'ScheduleWorkTest.ThreadTimeToIOFromTwoThreads':[
    'task: 2_threads_scheduling_to_io_pump= 6.216794903666874 us/task',
    'task_min_batch_time: 2_threads_scheduling_to_io_pump= 2.523 us/task',
    'task_max_batch_time: 2_threads_scheduling_to_io_pump= 142.989 us/task',
    'task_thread_time: 2_threads_scheduling_to_io_pump= 2.02620013 us/task'],
  'ScheduleWorkTest.ThreadTimeToIOFromFourThreads':[]}
  """
  lines = []
  for line in out_lines:
    if "RUN" in line or "RESULT" in line:
      lines.append(line)
  cases = {}
  name = ""
  case_lines = []  # type: List[str]
  for line in lines:
    # We've hit a new test suite, write the old accumulators and start new
    # ones. The name variable is checked to make sure this isn't the first one
    # in the list of lines
    if "RUN" in line:
      if name != "":
        cases[name] = case_lines
        case_lines = []
      name = line.split("]")[-1]  # Get the actual name of the test case.

    else:
      if "RESULT" not in line:
        print("{} did not get filtered!".format(line))
      line_trimmed = line.split("RESULT")[-1].strip()
      case_lines.append(line_trimmed)
  return cases


def CollateTests(linux_lines: List[str], fuchsia_lines: List[str],
                 test_target: str) -> TestComparison:
  """This function takes the GTest output of a single test target, and matches
  the informational sections of the outputs together, before collapsing them
  down into ResultLines attached to TestComparisons.
  """

  linux_cases = ExtractCases(linux_lines)
  fuchsia_cases = ExtractCases(fuchsia_lines)

  comparisons = {}
  for case_name, linux_case_lines in linux_cases.items():
    # If fuchsia didn't contain that test case, skip it, but alert the user.
    if not case_name in fuchsia_cases:
      print("Fuchsia is missing test case {} in target {}".format(
          case_name, test_target))
      continue

    fuchsia_case_lines = fuchsia_cases[case_name]

    # Each test case should output its informational lines in the same order, so
    # if tests only produce partial output, any tailing info should be dropped,
    # and only data that was produced by both tests will be compared.
    paired_case_lines = zip(linux_case_lines, fuchsia_case_lines)
    if len(linux_case_lines) != len(fuchsia_case_lines):
      print("Linux and Fuchsia have produced different output lengths for the "
            "test {}!".format(case_name))
    desc_lines = [ResultLine.FromLines(*pair) for pair in paired_case_lines]
    comparisons[case_name] = desc_lines

  for case_name in fuchsia_cases.keys():
    if case_name not in comparisons.keys():
      print("Linux is missing test case {} in target {}".format(
          case_name, test_target))

  return TestComparison(test_target, comparisons)


def RunTest(target: TestTarget, run_locally: bool = False) -> TestComparison:

  linux_output = target.ExecLinux(target_spec.linux_out_dir, run_locally)
  print("Ran Linux")
  fuchsia_output = target.ExecFuchsia(target_spec.fuchsia_out_dir, run_locally)
  print("Ran Fuchsia")
  tmp = CollateTests(
      linux_output.split("\n"), fuchsia_output.split("\n"), target.Name)
  print("Collated {} tests together for suite {}".format(
      len(tmp.tests), tmp.suite_name))
  return tmp


def RunGnForDirectory(dir_name: str, target_os: str) -> None:
  if not os.path.exists(dir_name):
    os.makedirs(dir_name)
  with open("{}/{}".format(dir_name, "args.gn"), "w") as args_file:
    args_file.write("is_debug = false\n")
    args_file.write("dcheck_always_on = false\n")
    args_file.write("is_component_build = false\n")
    args_file.write("use_goma = true\n")
    args_file.write("target_os = \"{}\"\n".format(target_os))

  subprocess.run(["gn", "gen", dir_name]).check_returncode()


def GenerateTestData(runs: int) -> List[List[TestComparison]]:
  DIR_SOURCE_ROOT = os.path.abspath(
      os.path.join(os.path.dirname(__file__), *([os.pardir] * 3)))
  os.chdir(DIR_SOURCE_ROOT)

  # Grab parameters from config file.
  linux_dir = target_spec.linux_out_dir
  fuchsia_dir = target_spec.fuchsia_out_dir
  test_input = []  # type: List[TestTarget]
  for target in target_spec.test_targets:
    test_input.append(TestTarget(target))
  print("Test targets collected:\n{}".format("\n".join(
      [test.Target for test in test_input])))

  RunGnForDirectory(linux_dir, "linux")
  RunGnForDirectory(fuchsia_dir, "fuchsia")

  # Build test targets in both output directories.
  for directory in [linux_dir, fuchsia_dir]:
    build_command = ["autoninja", "-C", directory] \
                  + [test.Target for test in test_input]
    RunCommand(build_command,
               "autoninja failed in directory {}".format(directory))
  print("Builds completed.")

  # Execute the tests, one at a time, per system, and collect their results.
  result_set = defaultdict(lambda: [])  # type: Dict[str, List[TestComparison]]
  for i in range(0, runs):
    print("Running Test set {}".format(i))
    for test_target in test_input:
      print("Running Target {}".format(test_target.Name))
      result = RunTest(test_target)
      result_set[result.suite_name].append(result)
      print("Finished {}".format(test_target.Name))

  print("Tests Completed")
  for _, results in result_set.items():
    for i, result in enumerate(results):
      with open("{}.{}.csv".format(result.suite_name, i), "w") as stat_file:
        stat_file.write("target,test,description,linux,"
                        "fuchsia,fuchsia/linux,units\n")
        stat_file.write(result.MakeCsvFormat())

def main() -> int:
  GenerateTestData(1)
  return 0


if __name__ == "__main__":
  sys.exit(main())
