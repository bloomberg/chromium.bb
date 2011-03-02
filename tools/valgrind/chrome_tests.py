#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# chrome_tests.py

''' Runs various chrome tests through valgrind_test.py.'''

import glob
import logging
import optparse
import os
import stat
import sys

import logging_utils
import path_utils

import common
import valgrind_test

class TestNotFound(Exception): pass

class MultipleGTestFiltersSpecified(Exception): pass

def Dir2IsNewer(dir1, dir2):
  if dir2 == None or not os.path.isdir(dir2):
    return False
  if dir1 == None or not os.path.isdir(dir1):
    return True
  return (os.stat(dir2)[stat.ST_MTIME] - os.stat(dir1)[stat.ST_MTIME]) > 0

def FindNewestDir(dirs):
  newest_dir = None
  for dir in dirs:
    if Dir2IsNewer(newest_dir, dir):
      newest_dir = dir
  return newest_dir

def File2IsNewer(file1, file2):
  if file2 == None or not os.path.isfile(file2):
    return False
  if file1 == None or not os.path.isfile(file1):
    return True
  return (os.stat(file2)[stat.ST_MTIME] - os.stat(file1)[stat.ST_MTIME]) > 0

def FindDirContainingNewestFile(dirs, file):
  newest_dir = None
  newest_file = None
  for dir in dirs:
    the_file = os.path.join(dir, file)
    if File2IsNewer(newest_file, the_file):
      newest_dir = dir
      newest_file = the_file
  if newest_dir == None:
    logging.error("cannot find file %s anywhere, have you built it?" % file)
    sys.exit(-1)
  return newest_dir

class ChromeTests:
  def __init__(self, options, args, test):
    if ':' in test:
      (self._test, self._gtest_filter) = test.split(':', 1)
    else:
      self._test = test
      self._gtest_filter = options.gtest_filter

    if self._test not in self._test_list:
      raise TestNotFound("Unknown test: %s" % test)

    if options.gtest_filter and options.gtest_filter != self._gtest_filter:
      raise MultipleGTestFiltersSpecified("Can not specify both --gtest_filter "
                                          "and --test %s" % test)

    self._options = options
    self._args = args

    script_dir = path_utils.ScriptDir()
    # Compute the top of the tree (the "source dir") from the script dir (where
    # this script lives).  We assume that the script dir is in tools/valgrind/
    # relative to the top of the tree.
    self._source_dir = os.path.dirname(os.path.dirname(script_dir))
    # since this path is used for string matching, make sure it's always
    # an absolute Unix-style path
    self._source_dir = os.path.abspath(self._source_dir).replace('\\', '/')
    valgrind_test_script = os.path.join(script_dir, "valgrind_test.py")
    self._command_preamble = ["--source_dir=%s" % (self._source_dir)]

  def _DefaultCommand(self, tool, module, exe=None, valgrind_test_args=None):
    '''Generates the default command array that most tests will use.'''
    if exe and common.IsWindows():
      exe = exe + '.exe'

    if not self._options.build_dir:
      dirs = [
        os.path.join(self._source_dir, "xcodebuild", "Debug"),
        os.path.join(self._source_dir, "out", "Debug"),
        os.path.join(self._source_dir, "build", "Debug"),
      ]
      if exe:
        self._options.build_dir = FindDirContainingNewestFile(dirs, exe)
      else:
        self._options.build_dir = FindNewestDir(dirs)

    cmd = list(self._command_preamble)

    # Find all suppressions matching the following pattern:
    # tools/valgrind/TOOL/suppressions[_PLATFORM].txt
    # and list them with --suppressions= prefix.
    script_dir = path_utils.ScriptDir()
    tool_name = tool.ToolName();
    suppression_file = os.path.join(script_dir, tool_name, "suppressions.txt")
    if os.path.exists(suppression_file):
      cmd.append("--suppressions=%s" % suppression_file)
    # Platform-specific suppression
    for platform in common.PlatformNames():
      platform_suppression_file = \
          os.path.join(script_dir, tool_name, 'suppressions_%s.txt' % platform)
      if os.path.exists(platform_suppression_file):
        cmd.append("--suppressions=%s" % platform_suppression_file)

    if self._options.valgrind_tool_flags:
      cmd += self._options.valgrind_tool_flags.split(" ")
    if valgrind_test_args != None:
      for arg in valgrind_test_args:
        cmd.append(arg)
    if exe:
      cmd.append(os.path.join(self._options.build_dir, exe))
      # Valgrind runs tests slowly, so slow tests hurt more; show elapased time
      # so we can find the slowpokes.
      cmd.append("--gtest_print_time")
    if self._options.gtest_repeat:
      cmd.append("--gtest_repeat=%s" % self._options.gtest_repeat)
    return cmd

  def Run(self):
    ''' Runs the test specified by command-line argument --test '''
    logging.info("running test %s" % (self._test))
    return self._test_list[self._test](self)

  def _ReadGtestFilterFile(self, tool, name, cmd):
    '''Read a file which is a list of tests to filter out with --gtest_filter
    and append the command-line option to cmd.
    '''
    filters = []
    gtest_files_dir = os.path.join(path_utils.ScriptDir(), "gtest_exclude")

    gtest_filter_files = [
        os.path.join(gtest_files_dir, name + ".gtest.txt"),
        os.path.join(gtest_files_dir, name + ".gtest-%s.txt" % tool.ToolName())]
    for platform_suffix in common.PlatformNames():
      gtest_filter_files += [
        os.path.join(gtest_files_dir, name + ".gtest_%s.txt" % platform_suffix),
        os.path.join(gtest_files_dir, name + ".gtest-%s_%s.txt" % \
            (tool.ToolName(), platform_suffix))]
    logging.info("Reading gtest exclude filter files:")
    for filename in gtest_filter_files:
      readable_filename = filename.replace(self._source_dir + "/", "")
      if not os.path.exists(filename):
        logging.info("  \"%s\" - not found" % readable_filename)
        continue
      logging.info("  \"%s\" - OK" % readable_filename)
      f = open(filename, 'r')
      for line in f.readlines():
        if line.startswith("#") or line.startswith("//") or line.isspace():
          continue
        line = line.rstrip()
        test_prefixes = ["FLAKY", "FAILS"]
        for p in test_prefixes:
          # Strip prefixes from the test names.
          line = line.replace(".%s_" % p, ".")
        # Exclude the original test name.
        filters.append(line)
        if line[-2:] != ".*":
          # List all possible prefixes if line doesn't end with ".*".
          for p in test_prefixes:
            filters.append(line.replace(".", ".%s_" % p))
    # Get rid of duplicates.
    filters = set(filters)
    gtest_filter = self._gtest_filter
    if len(filters):
      if gtest_filter:
        gtest_filter += ":"
        if gtest_filter.find("-") < 0:
          gtest_filter += "-"
      else:
        gtest_filter = "-"
      gtest_filter += ":".join(filters)
    if gtest_filter:
      cmd.append("--gtest_filter=%s" % gtest_filter)

  def SimpleTest(self, module, name, valgrind_test_args=None, cmd_args=None):
    tool = valgrind_test.CreateTool(self._options.valgrind_tool)
    cmd = self._DefaultCommand(tool, module, name, valgrind_test_args)
    self._ReadGtestFilterFile(tool, name, cmd)
    if cmd_args:
      cmd.extend(["--"])
      cmd.extend(cmd_args)

    # Sets LD_LIBRARY_PATH to the build folder so external libraries can be
    # loaded.
    if (os.getenv("LD_LIBRARY_PATH")):
      os.putenv("LD_LIBRARY_PATH", "%s:%s" % (os.getenv("LD_LIBRARY_PATH"),
                                              self._options.build_dir))
    else:
      os.putenv("LD_LIBRARY_PATH", self._options.build_dir)
    return tool.Run(cmd, module)

  def TestBase(self):
    return self.SimpleTest("base", "base_unittests")

  def TestBrowser(self):
    return self.SimpleTest("chrome", "browser_tests")

  def TestGURL(self):
    return self.SimpleTest("chrome", "googleurl_unittests")

  def TestCourgette(self):
    return self.SimpleTest("courgette", "courgette_unittests")

  def TestMedia(self):
    return self.SimpleTest("chrome", "media_unittests")

  def TestNotifier(self):
    return self.SimpleTest("chrome", "notifier_unit_tests")

  def TestPrinting(self):
    return self.SimpleTest("chrome", "printing_unittests")

  def TestRemoting(self):
    return self.SimpleTest("chrome", "remoting_unittests",
                           cmd_args=[
                               "--ui-test-timeout=240000",
                               "--ui-test-action-timeout=120000",
                               "--ui-test-action-max-timeout=280000"])

  def TestIpc(self):
    return self.SimpleTest("ipc", "ipc_tests",
                           valgrind_test_args=["--trace_children"])

  def TestNet(self):
    return self.SimpleTest("net", "net_unittests")

  def TestStartup(self):
    # We don't need the performance results, we're just looking for pointer
    # errors, so set number of iterations down to the minimum.
    os.putenv("STARTUP_TESTS_NUMCYCLES", "1")
    logging.info("export STARTUP_TESTS_NUMCYCLES=1");
    return self.SimpleTest("chrome", "startup_tests",
                           valgrind_test_args=[
                            "--trace_children",
                            "--indirect"])

  def TestTestShell(self):
    return self.SimpleTest("webkit", "test_shell_tests")

  def TestUnit(self):
    return self.SimpleTest("chrome", "unit_tests")

  def TestApp(self):
    return self.SimpleTest("chrome", "app_unittests")

  def TestGfx(self):
    return self.SimpleTest("chrome", "gfx_unittests")

  # Valgrind timeouts are in seconds.
  UI_VALGRIND_ARGS = ["--timeout=7200", "--trace_children", "--indirect"]
  # UI test timeouts are in milliseconds.
  UI_TEST_ARGS = ["--ui-test-timeout=240000",
                  "--ui-test-action-timeout=120000",
                  "--ui-test-action-max-timeout=280000",
                  "--ui-test-sleep-timeout=120000",
                  "--ui-test-terminate-timeout=120000"]
  def TestUI(self):
    return self.SimpleTest("chrome", "ui_tests",
                           valgrind_test_args=self.UI_VALGRIND_ARGS,
                           cmd_args=self.UI_TEST_ARGS)

  def TestAutomatedUI(self):
    return self.SimpleTest("chrome", "automated_ui_tests",
                           valgrind_test_args=self.UI_VALGRIND_ARGS,
                           cmd_args=self.UI_TEST_ARGS)

  def TestInteractiveUI(self):
    return self.SimpleTest("chrome", "interactive_ui_tests",
                           valgrind_test_args=self.UI_VALGRIND_ARGS,
                           cmd_args=(self.UI_TEST_ARGS +
                                     ["--test-terminate-timeout=300000"]))

  def TestReliability(self):
    script_dir = path_utils.ScriptDir()
    url_list_file = os.path.join(script_dir, "reliability", "url_list.txt")
    return self.SimpleTest("chrome", "reliability_tests",
                           valgrind_test_args=self.UI_VALGRIND_ARGS,
                           cmd_args=(self.UI_TEST_ARGS +
                                     ["--list=%s" % url_list_file]))

  def TestSafeBrowsing(self):
    return self.SimpleTest("chrome", "safe_browsing_tests",
                           valgrind_test_args=self.UI_VALGRIND_ARGS,
                           cmd_args=(["--test-terminate-timeout=900000"]))

  def TestSync(self):
    return self.SimpleTest("chrome", "sync_unit_tests")

  def TestSyncIntegration(self):
    return self.SimpleTest("chrome", "sync_integration_tests",
                           valgrind_test_args=self.UI_VALGRIND_ARGS,
                           cmd_args=(["--test-terminate-timeout=900000"]))

  def TestLayoutChunk(self, chunk_num, chunk_size):
    # Run tests [chunk_num*chunk_size .. (chunk_num+1)*chunk_size) from the
    # list of tests.  Wrap around to beginning of list at end.
    # If chunk_size is zero, run all tests in the list once.
    # If a text file is given as argument, it is used as the list of tests.
    #
    # Build the ginormous commandline in 'cmd'.
    # It's going to be roughly
    #  python valgrind_test.py ... python run_webkit_tests.py ...
    # but we'll use the --indirect flag to valgrind_test.py
    # to avoid valgrinding python.
    # Start by building the valgrind_test.py commandline.
    tool = valgrind_test.CreateTool(self._options.valgrind_tool)
    cmd = self._DefaultCommand(tool, "webkit")
    cmd.append("--trace_children")
    cmd.append("--indirect")
    cmd.append("--ignore_exit_code")
    # Now build script_cmd, the run_webkits_tests.py commandline
    # Store each chunk in its own directory so that we can find the data later
    chunk_dir = os.path.join("layout", "chunk_%05d" % chunk_num)
    test_shell = os.path.join(self._options.build_dir, "test_shell")
    out_dir = os.path.join(path_utils.ScriptDir(), "latest")
    out_dir = os.path.join(out_dir, chunk_dir)
    if os.path.exists(out_dir):
      old_files = glob.glob(os.path.join(out_dir, "*.txt"))
      for f in old_files:
        os.remove(f)
    else:
      os.makedirs(out_dir)
    script = os.path.join(self._source_dir, "webkit", "tools", "layout_tests",
                          "run_webkit_tests.py")
    script_cmd = ["python", script, "--run-singly", "-v",
                  "--noshow-results", "--time-out-ms=200000",
                  "--nocheck-sys-deps"]
    # Pass build mode to run_webkit_tests.py.  We aren't passed it directly,
    # so parse it out of build_dir.  run_webkit_tests.py can only handle
    # the two values "Release" and "Debug".
    # TODO(Hercules): unify how all our scripts pass around build mode
    # (--mode / --target / --build_dir / --debug)
    if self._options.build_dir.endswith("Debug"):
      script_cmd.append("--debug");
    if (chunk_size > 0):
      script_cmd.append("--run-chunk=%d:%d" % (chunk_num, chunk_size))
    if len(self._args):
      # if the arg is a txt file, then treat it as a list of tests
      if os.path.isfile(self._args[0]) and self._args[0][-4:] == ".txt":
        script_cmd.append("--test-list=%s" % self._args[0])
      else:
        script_cmd.extend(self._args)
    self._ReadGtestFilterFile(tool, "layout", script_cmd)
    # Now run script_cmd with the wrapper in cmd
    cmd.extend(["--"])
    cmd.extend(script_cmd)
    return tool.Run(cmd, "layout")

  def TestLayout(self):
    # A "chunk file" is maintained in the local directory so that each test
    # runs a slice of the layout tests of size chunk_size that increments with
    # each run.  Since tests can be added and removed from the layout tests at
    # any time, this is not going to give exact coverage, but it will allow us
    # to continuously run small slices of the layout tests under valgrind rather
    # than having to run all of them in one shot.
    chunk_size = self._options.num_tests
    if (chunk_size == 0):
      return self.TestLayoutChunk(0, 0)
    chunk_num = 0
    chunk_file = os.path.join("valgrind_layout_chunk.txt")
    logging.info("Reading state from " + chunk_file)
    try:
      f = open(chunk_file)
      if f:
        str = f.read()
        if len(str):
          chunk_num = int(str)
        # This should be enough so that we have a couple of complete runs
        # of test data stored in the archive (although note that when we loop
        # that we almost guaranteed won't be at the end of the test list)
        if chunk_num > 10000:
          chunk_num = 0
        f.close()
    except IOError, (errno, strerror):
      logging.error("error reading from file %s (%d, %s)" % (chunk_file,
                    errno, strerror))
    ret = self.TestLayoutChunk(chunk_num, chunk_size)
    # Wait until after the test runs to completion to write out the new chunk
    # number.  This way, if the bot is killed, we'll start running again from
    # the current chunk rather than skipping it.
    logging.info("Saving state to " + chunk_file)
    try:
      f = open(chunk_file, "w")
      chunk_num += 1
      f.write("%d" % chunk_num)
      f.close()
    except IOError, (errno, strerror):
      logging.error("error writing to file %s (%d, %s)" % (chunk_file, errno,
                    strerror))
    # Since we're running small chunks of the layout tests, it's important to
    # mark the ones that have errors in them.  These won't be visible in the
    # summary list for long, but will be useful for someone reviewing this bot.
    return ret

  # The known list of tests.
  # Recognise the original abbreviations as well as full executable names.
  _test_list = {
    "automated_ui" : TestAutomatedUI,
    "base": TestBase,            "base_unittests": TestBase,
    "browser": TestBrowser,      "browser_tests": TestBrowser,
    "googleurl": TestGURL,       "googleurl_unittests": TestGURL,
    "courgette": TestCourgette,  "courgette_unittests": TestCourgette,
    "ipc": TestIpc,              "ipc_tests": TestIpc,
    "interactive_ui": TestInteractiveUI,
    "layout": TestLayout,        "layout_tests": TestLayout,
    "media": TestMedia,          "media_unittests": TestMedia,
    "net": TestNet,              "net_unittests": TestNet,
    "notifier": TestNotifier,    "notifier_unittests": TestNotifier,
    "printing": TestPrinting,    "printing_unittests": TestPrinting,
    "reliability": TestReliability, "reliability_tests": TestReliability,
    "remoting": TestRemoting,    "remoting_unittests": TestRemoting,
    "safe_browsing": TestSafeBrowsing, "safe_browsing_tests": TestSafeBrowsing,
    "startup": TestStartup,      "startup_tests": TestStartup,
    "sync": TestSync,            "sync_unit_tests": TestSync,
    "sync_integration_tests": TestSyncIntegration,
    "sync_integration": TestSyncIntegration,
    "test_shell": TestTestShell, "test_shell_tests": TestTestShell,
    "ui": TestUI,                "ui_tests": TestUI,
    "unit": TestUnit,            "unit_tests": TestUnit,
    "app": TestApp,              "app_unittests": TestApp,
    "gfx": TestGfx,              "gfx_unittests": TestGfx,
  }

def _main(_):
  parser = optparse.OptionParser("usage: %prog -b <dir> -t <test> "
                                 "[-t <test> ...]")
  parser.disable_interspersed_args()
  parser.add_option("-b", "--build_dir",
                    help="the location of the compiler output")
  parser.add_option("-t", "--test", action="append", default=[],
                    help="which test to run, supports test:gtest_filter format "
                         "as well.")
  parser.add_option("", "--baseline", action="store_true", default=False,
                    help="generate baseline data instead of validating")
  parser.add_option("", "--gtest_filter",
                    help="additional arguments to --gtest_filter")
  parser.add_option("", "--gtest_repeat",
                    help="argument for --gtest_repeat")
  parser.add_option("-v", "--verbose", action="store_true", default=False,
                    help="verbose output - enable debug log messages")
  parser.add_option("", "--tool", dest="valgrind_tool", default="memcheck",
                    help="specify a valgrind tool to run the tests under")
  parser.add_option("", "--tool_flags", dest="valgrind_tool_flags", default="",
                    help="specify custom flags for the selected valgrind tool")
  # My machine can do about 120 layout tests/hour in release mode.
  # Let's do 30 minutes worth per run.
  # The CPU is mostly idle, so perhaps we can raise this when
  # we figure out how to run them more efficiently.
  parser.add_option("-n", "--num_tests", default=60, type="int",
                    help="for layout tests: # of subtests per run.  0 for all.")

  options, args = parser.parse_args()

  if options.verbose:
    logging_utils.config_root(logging.DEBUG)
  else:
    logging_utils.config_root()

  if not options.test:
    parser.error("--test not specified")

  if len(options.test) != 1 and options.gtest_filter:
    parser.error("--gtest_filter and multiple tests don't make sense together")

  for t in options.test:
    tests = ChromeTests(options, args, t)
    ret = tests.Run()
    if ret: return ret
  return 0


if __name__ == "__main__":
  ret = _main(sys.argv)
  sys.exit(ret)
