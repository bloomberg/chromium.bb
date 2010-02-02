#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# valgrind_test.py

'''Runs an exe through Valgrind and puts the intermediate files in a
directory.
'''

import datetime
import glob
import logging
import optparse
import os
import re
import shutil
import stat
import sys
import tempfile

import common

import memcheck_analyze
import tsan_analyze

import google.logging_utils

class ValgrindTool(object):

  """Abstract class for running Valgrind.

  Always subclass this and implement ValgrindCommand() with platform specific
  stuff.
  """

  TMP_DIR = "valgrind.tmp"

  def __init__(self):
    # If we have a valgrind.tmp directory, we failed to cleanup last time.
    if os.path.exists(self.TMP_DIR):
      shutil.rmtree(self.TMP_DIR)
    os.mkdir(self.TMP_DIR)

  def UseXML(self):
    # Override if tool prefers nonxml output
    return True

  def ToolName(self):
    raise RuntimeError, "This method should be implemented " \
                        "in the tool-specific subclass"

  def CreateOptionParser(self):
    self._parser = optparse.OptionParser("usage: %prog [options] <program to "
                                         "test>")
    self._parser.add_option("-t", "--timeout",
                      dest="timeout", metavar="TIMEOUT", default=10000,
                      help="timeout in seconds for the run (default 10000)")
    self._parser.add_option("", "--source_dir",
                            help="path to top of source tree for this build"
                                 "(used to normalize source paths in baseline)")
    self._parser.add_option("", "--gtest_filter", default="",
                            help="which test case to run")
    self._parser.add_option("", "--gtest_repeat",
                            help="how many times to run each test")
    self._parser.add_option("", "--gtest_print_time", action="store_true",
                            default=False,
                            help="show how long each test takes")
    self._parser.add_option("", "--indirect", action="store_true",
                            default=False,
                            help="set BROWSER_WRAPPER rather than "
                                 "running valgrind directly")
    self._parser.add_option("-v", "--verbose", action="store_true",
                            default=False,
                            help="verbose output - enable debug log messages")
    self._parser.add_option("", "--trace_children", action="store_true",
                            default=False,
                            help="also trace child processes")
    self._parser.add_option("", "--num-callers",
                            dest="num_callers", default=30,
                            help="number of callers to show in stack traces")
    self._parser.add_option("", "--nocleanup_on_exit", action="store_true",
                            default=False,
                            help="don't delete directory with logs on exit")
    self.ExtendOptionParser(self._parser)
    self._parser.description = __doc__

  def ExtendOptionParser(self, parser):
    parser.add_option("", "--generate_dsym", action="store_true",
                          default=False,
                          help="Generate .dSYM file on Mac if needed. Slow!")

  def ParseArgv(self, args):
    self.CreateOptionParser()

    # self._tool_flags will store those tool flags which we don't parse
    # manually in this script.
    self._tool_flags = []
    known_args = []

    """ We assume that the first argument not starting with "-" is a program name
    and all the following flags should be passed to the program.
    TODO(timurrrr): customize optparse instead
    """
    while len(args) > 0 and args[0][:1] == "-":
      arg = args[0]
      if (arg == "--"):
        break
      if self._parser.has_option(arg.split("=")[0]):
        known_args += [arg]
      else:
        self._tool_flags += [arg]
      args = args[1:]

    if len(args) > 0:
      known_args += args

    self._options, self._args = self._parser.parse_args(known_args)

    self._timeout = int(self._options.timeout)
    self._num_callers = int(self._options.num_callers)
    self._suppressions = self._options.suppressions
    self._source_dir = self._options.source_dir
    self._nocleanup_on_exit = self._options.nocleanup_on_exit
    if self._options.gtest_filter != "":
      self._args.append("--gtest_filter=%s" % self._options.gtest_filter)
    if self._options.gtest_repeat:
      self._args.append("--gtest_repeat=%s" % self._options.gtest_repeat)
    if self._options.gtest_print_time:
      self._args.append("--gtest_print_time");

    return True

  def Setup(self, args):
    return self.ParseArgv(args)

  def PrepareForTest(self):
    """Runs dsymutil if needed.

    Valgrind for Mac OS X requires that debugging information be in a .dSYM
    bundle generated by dsymutil.  It is not currently able to chase DWARF
    data into .o files like gdb does, so executables without .dSYM bundles or
    with the Chromium-specific "fake_dsym" bundles generated by
    build/mac/strip_save_dsym won't give source file and line number
    information in valgrind.

    This function will run dsymutil if the .dSYM bundle is missing or if
    it looks like a fake_dsym.  A non-fake dsym that already exists is assumed
    to be up-to-date.
    """
    if not common.IsMac():
      return

    test_command = self._args[0]
    dsym_bundle = self._args[0] + '.dSYM'
    dsym_file = os.path.join(dsym_bundle, 'Contents', 'Resources', 'DWARF',
                             os.path.basename(test_command))
    dsym_info_plist = os.path.join(dsym_bundle, 'Contents', 'Info.plist')

    needs_dsymutil = True
    saved_test_command = None

    if os.path.exists(dsym_file) and os.path.exists(dsym_info_plist):
      # Look for the special fake_dsym tag in dsym_info_plist.
      dsym_info_plist_contents = open(dsym_info_plist).read()

      if not re.search('^\s*<key>fake_dsym</key>$', dsym_info_plist_contents,
                       re.MULTILINE):
        # fake_dsym is not set, this is a real .dSYM bundle produced by
        # dsymutil.  dsymutil does not need to be run again.
        needs_dsymutil = False
      else:
        # fake_dsym is set.  dsym_file is a copy of the original test_command
        # before it was stripped.  Copy it back to test_command so that
        # dsymutil has unstripped input to work with.  Move the stripped
        # test_command out of the way, it will be restored when this is
        # done.
        saved_test_command = test_command + '.stripped'
        os.rename(test_command, saved_test_command)
        shutil.copyfile(dsym_file, test_command)
        shutil.copymode(saved_test_command, test_command)

    if needs_dsymutil:
      if self._options.generate_dsym:
        # Remove the .dSYM bundle if it exists.
        shutil.rmtree(dsym_bundle, True)

        dsymutil_command = ['dsymutil', test_command]

        # dsymutil is crazy slow.  Ideally we'd have a timeout here,
        # but common.RunSubprocess' timeout is only checked
        # after each line of output; dsymutil is silent
        # until the end, and is then killed, which is silly.
        common.RunSubprocess(dsymutil_command)

        if saved_test_command:
          os.rename(saved_test_command, test_command)
      else:
        logging.info("No real .dSYM for test_command.  Line numbers will "
                     "not be shown.  Either tell xcode to generate .dSYM "
                     "file, or use --generate_dsym option to this tool.")

  def ValgrindCommand(self):
    """Get the valgrind command to run."""
    # Note that self._args begins with the exe to be run.
    tool_name = self.ToolName()

    # Construct the valgrind command.
    proc = ["valgrind",
            "--tool=%s" % tool_name,
            "--num-callers=%i" % self._num_callers]

    if self._options.trace_children:
      proc += ["--trace-children=yes"];

    proc += self.ToolSpecificFlags()
    proc += self._tool_flags

    suppression_count = 0
    for suppression_file in self._suppressions:
      if os.path.exists(suppression_file):
        suppression_count += 1
        proc += ["--suppressions=%s" % suppression_file]

    if not suppression_count:
      logging.warning("WARNING: NOT USING SUPPRESSIONS!")

    logfilename = self.TMP_DIR + ("/%s." % tool_name) + "%p"
    if self.UseXML():
      if os.system("valgrind --help | grep -q xml-file") == 0:
        proc += ["--xml=yes", "--xml-file=" + logfilename]
      else:
        # TODO(dank): remove once valgrind-3.5 is deployed everywhere
        proc += ["--xml=yes", "--log-file=" + logfilename]
    else:
      proc += ["--log-file=" + logfilename]

    # The Valgrind command is constructed.

    if self._options.indirect:
      self.CreateBrowserWrapper(" ".join(proc))
      proc = []
    proc += self._args
    return proc

  def ToolSpecificFlags(self):
    return []

  def Execute(self):
    ''' Execute the app to be tested after successful instrumentation.
    Full execution command-line provided by subclassers via proc.'''
    logging.info("starting execution...")

    proc = self.ValgrindCommand()
    os.putenv("G_SLICE", "always-malloc")
    logging.info("export G_SLICE=always-malloc");
    os.putenv("NSS_DISABLE_ARENA_FREE_LIST", "1")
    logging.info("export NSS_DISABLE_ARENA_FREE_LIST=1");
    os.putenv("GTEST_DEATH_TEST_USE_FORK", "1")
    logging.info("export GTEST_DEATH_TEST_USE_FORK=1");

    return common.RunSubprocess(proc, self._timeout)

  def Analyze(self):
    raise RuntimeError, "This method should be implemented " \
                        "in the tool-specific subclass"

  def Cleanup(self):
    # Right now, we can cleanup by deleting our temporary directory. Other
    # cleanup is still a TODO?
    if not self._nocleanup_on_exit:
      shutil.rmtree(self.TMP_DIR, ignore_errors=True)
    return True

  def RunTestsAndAnalyze(self):
    self.PrepareForTest()
    exec_retcode = self.Execute()
    analyze_retcode = self.Analyze()

    if analyze_retcode:
      logging.error("Analyze failed.")
      return analyze_retcode

    if exec_retcode:
      logging.error("Test Execution failed.")
      return exec_retcode

    logging.info("Execution and analysis completed successfully.")
    return 0

  def CreateBrowserWrapper(self, command):
    """The program being run invokes Python or something else
    that can't stand to be valgrinded, and also invokes
    the Chrome browser.  Set an environment variable to
    tell the program to prefix the Chrome commandline
    with a magic wrapper.  Build the magic wrapper here.
    """
    (fd, indirect_fname) = tempfile.mkstemp(dir=self.TMP_DIR, prefix="browser_wrapper.", text=True)
    f = os.fdopen(fd, "w");
    f.write("#!/bin/sh\n")
    f.write(command)
    f.write(' "$@"\n')
    f.close()
    os.chmod(indirect_fname, stat.S_IRUSR|stat.S_IXUSR)
    os.putenv("BROWSER_WRAPPER", indirect_fname)
    logging.info('export BROWSER_WRAPPER=' + indirect_fname);

  def Main(self, args):
    '''Call this to run through the whole process: Setup, Execute, Analyze'''
    start = datetime.datetime.now()
    retcode = -1
    if self.Setup(args):
      retcode = self.RunTestsAndAnalyze()
      self.Cleanup()
    else:
      logging.error("Setup failed")
    end = datetime.datetime.now()
    seconds = (end - start).seconds
    hours = seconds / 3600
    seconds = seconds % 3600
    minutes = seconds / 60
    seconds = seconds % 60
    logging.info("elapsed time: %02d:%02d:%02d" % (hours, minutes, seconds))
    return retcode

# TODO(timurrrr): Split into a separate file.
class Memcheck(ValgrindTool):
  """Memcheck"""

  def __init__(self):
    ValgrindTool.__init__(self)

  def ToolName(self):
    return "memcheck"

  def ExtendOptionParser(self, parser):
    ValgrindTool.ExtendOptionParser(self, parser)
    parser.add_option("", "--suppressions", default=[],
                      action="append",
                      help="path to a valgrind suppression file")
    parser.add_option("", "--show_all_leaks", action="store_true",
                      default=False,
                      help="also show less blatant leaks")
    parser.add_option("", "--track_origins", action="store_true",
                      default=False,
                      help="Show whence uninitialized bytes came. 30% slower.")

  def ToolSpecificFlags(self):
    ret = ["--leak-check=full", "--gen-suppressions=all", "--demangle=no"]

    if self._options.show_all_leaks:
      ret += ["--show-reachable=yes"];
    else:
      ret += ["--show-possible=no"];

    if self._options.track_origins:
      ret += ["--track-origins=yes"];

    return ret

  def Analyze(self):
    # Glob all the files in the "valgrind.tmp" directory
    filenames = glob.glob(self.TMP_DIR + "/memcheck.*")

    use_gdb = common.IsMac()
    analyzer = memcheck_analyze.MemcheckAnalyze(self._source_dir, filenames,
                                                self._options.show_all_leaks,
                                                use_gdb=use_gdb)
    ret = analyzer.Report()
    if ret != 0:
      logging.info("Please see http://dev.chromium.org/developers/how-tos/"
                   "using-valgrind for the info on Memcheck/Valgrind")
    return ret

class ThreadSanitizer(ValgrindTool):
  """ThreadSanitizer"""

  def __init__(self):
    ValgrindTool.__init__(self)

  def ToolName(self):
    return "tsan"

  def UseXML(self):
    return False

  def ExtendOptionParser(self, parser):
    ValgrindTool.ExtendOptionParser(self, parser)
    parser.add_option("", "--suppressions", default=[],
                      action="append",
                      help="path to a valgrind suppression file")
    parser.add_option("", "--pure-happens-before", default="yes",
                      dest="pure_happens_before",
                      help="Less false reports, more missed races")
    parser.add_option("", "--ignore-in-dtor", default="no",
                      dest="ignore_in_dtor",
                      help="Ignore data races inside destructors")
    parser.add_option("", "--announce-threads", default="yes",
                      dest="announce_threads",
                      help="Show the the stack traces of thread creation")

  def EvalBoolFlag(self, flag_value):
    if (flag_value in ["1", "true", "yes"]):
      return True
    elif (flag_value in ["0", "false", "no"]):
      return False
    raise RuntimeError, "Can't parse flag value (%s)" % flag_value

  def ToolSpecificFlags(self):
    ret = []

    ignore_files = ["ignores.txt"]
    platform_suffix = common.PlatformName()
    ignore_files.append("ignores_%s.txt" % platform_suffix)
    for ignore_file in ignore_files:
      fullname =  os.path.join(self._source_dir,
          "tools", "valgrind", "tsan", ignore_file)
      if os.path.exists(fullname):
        ret += ["--ignore=%s" % fullname]

    # The -v flag is needed for printing the list of used suppressions.
    ret += ["-v"]

    ret += ["--file-prefix-to-cut=%s/" % self._source_dir]

    if self.EvalBoolFlag(self._options.pure_happens_before):
      ret += ["--pure-happens-before=yes"] # "no" is the default value for TSAN

    if not self.EvalBoolFlag(self._options.ignore_in_dtor):
      ret += ["--ignore-in-dtor=no"] # "yes" is the default value for TSAN

    if self.EvalBoolFlag(self._options.announce_threads):
      ret += ["--announce-threads"]

    # --show-pc flag is needed for parsing the error logs on Darwin.
    if platform_suffix == 'mac':
      ret += ["--show-pc=yes"]

    return ret

  def Analyze(self):
    filenames = glob.glob(self.TMP_DIR + "/tsan.*")
    use_gdb = common.IsMac()
    analyzer = tsan_analyze.TsanAnalyze(self._source_dir, filenames,
                                        use_gdb=use_gdb)
    ret = analyzer.Report()
    if ret != 0:
      logging.info("Please see http://dev.chromium.org/developers/how-tos/"
                   "using-valgrind/threadsanitizer for the info on "
                   "ThreadSanitizer")
    return ret


class ToolFactory:
  def Create(self, tool_name):
    if tool_name == "memcheck":
      return Memcheck()
    if tool_name == "memcheck_wine":
      return Memcheck()
    if tool_name == "tsan":
      if not IsLinux():
        logging.info("WARNING: ThreadSanitizer may be unstable on Mac.")
        logging.info("See http://code.google.com/p/data-race-test/wiki/"
                     "ThreadSanitizerOnMacOsx for the details")
      return ThreadSanitizer()
    try:
      platform_name = common.PlatformName()
    except common.NotImplementedError:
      platform_name = sys.platform + "(Unknown)"
    raise RuntimeError, "Unknown tool (tool=%s, platform=%s)" % (tool_name,
                                                                 platform_name)

def RunTool(argv):
  # TODO(timurrrr): customize optparse instead
  tool_name = "memcheck"
  args = argv[1:]
  for arg in args:
    if arg.startswith("--tool="):
      tool_name = arg[7:]
      args.remove(arg)
      break

  tool = ToolFactory().Create(tool_name)
  return tool.Main(args)

if __name__ == "__main__":
  if sys.argv.count("-v") > 0 or sys.argv.count("--verbose") > 0:
    google.logging_utils.config_root(logging.DEBUG)
  else:
    google.logging_utils.config_root()
  # TODO(timurrrr): valgrind tools may use -v/--verbose as well

  ret = RunTool(sys.argv)
  sys.exit(ret)
