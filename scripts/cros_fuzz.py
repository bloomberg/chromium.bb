# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for performing tasks that are useful for fuzzer development.

Run "cros_fuzz" in the chroot for a list of command or "cros_fuzz $COMMAND
--help" for their full details. Below is a summary of commands that the script
can perform:

coverage: Generate a coverage report for a given fuzzer (specified by "--fuzzer"
  option). You almost certainly want to specify the package to build (using
  the "--package" option) so that a coverage build is done, since a coverage
  build is needed to generate a report. If your fuzz target is running on
  ClusterFuzz already, you can use the "--download" option to download the
  corpus from ClusterFuzz. Otherwise, you can use the "--corpus" option to
  specify the path of the corpus to run the fuzzer on and generate a report.
  The corpus will be copied to the sysroot so that the fuzzer can use it.
  Note that "--download" and "--corpus" are mutually exclusive.

reproduce: Runs the fuzzer specified by the "--fuzzer" option on a testcase
  (path specified by the "--testcase" argument). Optionally does a build when
  the "--package" option is used. The type of build can be specified using the
  "--build_type" argument.

download: Downloads the corpus from ClusterFuzz of the fuzzer specified by the
  "--fuzzer" option. The path of the directory the corpus directory is
  downloaded to can be specified using the "--directory" option.

shell: Sets up the sysroot for fuzzing and then chroots into the sysroot giving
  you a shell that is ready to fuzz.

setup: Sets up the sysroot for fuzzing (done prior to doing "reproduce", "shell"
  and "coverage" commands).

cleanup: Undoes "setup".

Note that cros_fuzz will print every shell command it runs if you set the
log-level to debug ("--log-level debug"). Otherwise it will print commands that
fail.
"""

from __future__ import print_function

import os
import shutil

from elftools.elf.elffile import ELFFile
import lddtree

from chromite.lib import commandline
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import portage_util

# Directory in sysroot's /tmp directory that this script will use for files it
# needs to write. We need a directory to write files to because this script uses
# external programs that must write and read to/from files and because these
# must be run inside the sysroot and thus are usually unable to read or write
# from directories in the chroot environment this script is executed in.
SCRIPT_STORAGE_DIRECTORY = 'fuzz'
SCRIPT_STORAGE_PATH = os.path.join('/', 'tmp', SCRIPT_STORAGE_DIRECTORY)

# Names of subdirectories in "fuzz" directory used by this script to store
# things.
CORPUS_DIRECTORY_NAME = 'corpus'
TESTCASE_DIRECTORY_NAME = 'testcase'
COVERAGE_REPORT_DIRECTORY_NAME = 'coverage-report'

# Constants for names of libFuzzer command line options.
RUNS_OPTION_NAME = 'runs'
MAX_TOTAL_TIME_OPTION_NAME = 'max_total_time'

# The default path a profraw file written by a clang coverage instrumented
# binary when run by this script (default is current working directory).
DEFAULT_PROFRAW_PATH = '/default.profraw'

# Constants for libFuzzer command line values.
# 0 runs means execute everything in the corpus and do no mutations.
RUNS_DEFAULT_VALUE = 0
# An arbitrary but short amount of time to run a fuzzer to get some coverage
# data (when a corpus hasn't been provided and we aren't told to download one.
MAX_TOTAL_TIME_DEFAULT_VALUE = 30


class BuildType(object):
  """Class to hold the different kinds of build types."""

  ASAN = 'asan'
  MSAN = 'msan'
  UBSAN = 'ubsan'
  COVERAGE = 'coverage'
  STANDARD = ''

  # Build types that users can specify.
  CHOICES = (ASAN, MSAN, UBSAN, COVERAGE)


class SysrootPath(object):
  """Class for representing a path that is in the sysroot.

  Useful for dealing with paths that we must interact with when chrooted into
  the sysroot and outside of it.

  For example, if we need to interact with the "/tmp" directory of the sysroot,
  SysrootPath('/tmp').sysroot returns the path of the directory if we are in
  chrooted into the sysroot, i.e. "/tmp".

  SysrootPath('/tmp').chroot returns the path of the directory when in the
  cros_sdk i.e. SYSROOT_DIRECTORY + "/tmp" (this will probably be
  "/build/amd64-generic/tmp" in most cases).
  """

  # The actual path to the sysroot (from within the chroot).
  path_to_sysroot = None

  def __init__(self, path):
    """Constructor.

    Args:
      path: An absolute path representing something in the sysroot.
    """

    assert path.startswith('/')
    if self.IsPathInSysroot(path):
      path = self.FromChrootPathInSysroot(os.path.abspath(path))
    self.path_list = path.split(os.sep)[1:]

  @classmethod
  def SetPathToSysroot(cls, board):
    """Sets path_to_sysroot

    Args:
      board: The board we will use for our sysroot.

    Returns:
      The path to the sysroot (the value of path_to_sysroot).
    """
    cls.path_to_sysroot = cros_build_lib.GetSysroot(board)
    return cls.path_to_sysroot

  @property
  def chroot(self):
    """Get the path of the object in the Chrome OS SDK chroot.

    Returns:
      The path this object represents when chrooted into the sysroot.
    """
    assert self.path_to_sysroot is not None, 'set SysrootPath.path_to_sysroot'
    return os.path.join(self.path_to_sysroot, *self.path_list)

  @property
  def sysroot(self):
    """Get the path of the object when in the sysroot.

    Returns:
      The path this object represents when in the Chrome OS SDK .
    """
    return os.path.join('/', *self.path_list)

  @classmethod
  def IsPathInSysroot(cls, path):
    """Is a path in the sysroot.

    Args:
      path: The path we are checking is in the sysroot.

    Returns:
      True if path is within the sysroot's path in the chroot.
    """
    assert cls.path_to_sysroot
    return path.startswith(cls.path_to_sysroot)

  @classmethod
  def FromChrootPathInSysroot(cls, path):
    """Converts a chroot-relative path that is in sysroot into sysroot-relative.

    Args:
      path: The chroot-relative path we are converting to sysroot relative.

    Returns:
      The sysroot relative version of |path|.
    """
    assert cls.IsPathInSysroot(path)
    common_prefix = os.path.commonprefix([cls.path_to_sysroot, path])
    return path[len(common_prefix):]


def GetScriptStoragePath(relative_path):
  """Get the SysrootPath representing a script storage path.

  Get a path of a directory this script will store things in.

  Args:
    relative_path: The path relative to the root of the script storage
      directory.

  Returns:
    The SysrootPath representing absolute path of |relative_path| in the script
    storage directory.
  """
  path = os.path.join(SCRIPT_STORAGE_PATH, relative_path)
  return SysrootPath(path)


def GetSysrootPath(path):
  """Get the chroot-relative path of a path in the sysroot.

  Args:
    path: An absolute path in the sysroot that we will get the path in the
      chroot for.

  Returns:
    The chroot-relative path of |path| in the sysroot.
  """
  return SysrootPath(path).chroot


def GetCoverageDirectory(fuzzer):
  """Get a coverage report directory for a fuzzer

  Args:
    fuzzer: The fuzzer to get the coverage report directory for.

  Returns:
    The location of the coverage report directory for the |fuzzer|.
  """
  relative_path = os.path.join(COVERAGE_REPORT_DIRECTORY_NAME, fuzzer)
  return GetScriptStoragePath(relative_path)


def GetFuzzerSysrootPath(fuzzer):
  """Get the path in the sysroot of a fuzzer.

  Args:
    fuzzer: The fuzzer to get the path of.

  Returns:
    The path of |fuzzer| in the sysroot.
  """
  return SysrootPath(os.path.join('/', 'usr', 'libexec', 'fuzzers', fuzzer))


def GetProfdataPath(fuzzer):
  """Get the profdata file of a fuzzer.

  Args:
    fuzzer: The fuzzer to get the profdata file of.

  Returns:
    The path of the profdata file that should be used by |fuzzer|.
  """
  return GetScriptStoragePath('%s.profdata' % fuzzer)


def GetPathForCopy(parent_directory, chroot_path):
  """Returns a path in the script storage directory to copy chroot_path.

  Returns a SysrootPath representing the location where |chroot_path| should
  copied. This path will be in the parent_directory which will be in the script
  storage directory.
  """
  basename = os.path.basename(chroot_path)
  return GetScriptStoragePath(os.path.join(parent_directory, basename))


def CopyCorpusToSysroot(src_corpus_path):
  """Copies corpus into the sysroot.

  Copies corpus into the sysroot. Doesn't copy if corpus is already in sysroot.

  Args:
    src_corpus_path: A path (in the chroot) to a corpus that will be copied into
      sysroot.

  Returns:
    The path in the sysroot that the corpus was copied to.
  """
  if src_corpus_path is None:
    return None

  if SysrootPath.IsPathInSysroot(src_corpus_path):
    # Don't copy if |src_testcase_path| is already in sysroot. Just return it in
    # the format expected by the caller.
    return SysrootPath(src_corpus_path)

  dest_corpus_path = GetPathForCopy(CORPUS_DIRECTORY_NAME, src_corpus_path)
  osutils.RmDir(dest_corpus_path.chroot, ignore_missing=True)
  shutil.copytree(src_corpus_path, dest_corpus_path.chroot)
  return dest_corpus_path


def CopyTestcaseToSysroot(src_testcase_path):
  """Copies a testcase into the sysroot.

  Copies a testcase into the sysroot. Doesn't copy if testcase is already in
  sysroot.

  Args:
    src_testcase_path: A path (in the chroot) to a testcase that will be copied
      into sysroot.

  Returns:
    The path in the sysroot that the testcase was copied to.
  """
  if SysrootPath.IsPathInSysroot(src_testcase_path):
    # Don't copy if |src_testcase_path| is already in sysroot. Just return it in
    # the format expected by the caller.
    return SysrootPath(src_testcase_path)

  dest_testcase_path = GetPathForCopy(TESTCASE_DIRECTORY_NAME,
                                      src_testcase_path)
  osutils.SafeMakedirsNonRoot(os.path.dirname(dest_testcase_path.chroot))
  osutils.SafeUnlink(dest_testcase_path.chroot)

  shutil.copy(src_testcase_path, dest_testcase_path.chroot)
  return dest_testcase_path


def sudo_run(*args, **kwargs):
  """Wrapper around cros_build_lib.sudo_run.

  Wrapper that calls cros_build_lib.sudo_run but sets debug_level by
  default.

  Args:
    *args: Positional arguments to pass to cros_build_lib.sudo_run.
    *kwargs: Keyword arguments to pass to cros_build_lib.sudo_run.

  Returns:
    The value returned by calling cros_build_lib.sudo_run.
  """
  kwargs.setdefault('debug_level', logging.DEBUG)
  return cros_build_lib.sudo_run(*args, **kwargs)


def GetLibFuzzerOption(option_name, option_value):
  """Gets the libFuzzer command line option with the specified name and value.

  Args:
    option_name: The name of the libFuzzer option.
    option_value: The value of the libFuzzer option.

  Returns:
    The libFuzzer option composed of |option_name| and |option_value|.
  """
  return '-%s=%s' % (option_name, option_value)


def IsOptionLimit(option):
  """Determines if fuzzer option limits fuzzing time."""
  for limit_name in [MAX_TOTAL_TIME_OPTION_NAME, RUNS_OPTION_NAME]:
    if option.startswith('-%s' % limit_name):
      return True

  return False


def LimitFuzzing(fuzz_command, corpus):
  """Limits how long fuzzing will go if unspecified.

  Adds a reasonable limit on how much fuzzing will be done unless there already
  is some kind of limit. Mutates fuzz_command.

  Args:
    fuzz_command: A command to run a fuzzer. Used to determine if a limit needs
      to be set. Mutated if it is needed to specify a limit.
    corpus: The corpus that will be passed to the fuzzer. If not None then
      fuzzing is limited by running everything in the corpus once.
  """
  if any(IsOptionLimit(x) for x in fuzz_command[1:]):
    # Don't do anything if there is already a limit.
    return

  if corpus:
    # If there is a corpus, just run everything in the corpus once.
    fuzz_command.append(
        GetLibFuzzerOption(RUNS_OPTION_NAME, RUNS_DEFAULT_VALUE))
    return

  # Since there is no corpus, just fuzz for 30 seconds.
  logging.info('Limiting fuzzing to %s seconds.', MAX_TOTAL_TIME_DEFAULT_VALUE)
  max_total_time_option = GetLibFuzzerOption(MAX_TOTAL_TIME_OPTION_NAME,
                                             MAX_TOTAL_TIME_DEFAULT_VALUE)
  fuzz_command.append(max_total_time_option)


def GetFuzzExtraEnv(extra_options=None):
  """Gets extra_env for fuzzing.

  Gets environment varaibles and values for running libFuzzer. Sets defaults and
  allows user to specify extra sanitizer options.

  Args:
    extra_options: A dict containing sanitizer options to set in addition to the
      defaults.

  Returns:
    A dict containing environment variables and their values that can be used in
    the environment libFuzzer runs in.
  """
  if extra_options is None:
    extra_options = {}

  # log_path must be set because Chrome OS's patched compiler changes it.
  options_dict = {'log_path': 'stderr'}
  options_dict.update(extra_options)
  sanitizer_options = ':'.join('%s=%s' % x for x in options_dict.items())
  sanitizers = ('ASAN', 'MSAN', 'UBSAN')
  return {x + '_OPTIONS': sanitizer_options for x in sanitizers}


def RunFuzzer(fuzzer,
              corpus_path=None,
              fuzz_args='',
              testcase_path=None,
              crash_expected=False):
  """Runs the fuzzer while chrooted into the sysroot.

  Args:
    fuzzer: The fuzzer to run.
    corpus_path: A path to a corpus (not necessarily in the sysroot) to run the
      fuzzer on.
    fuzz_args: Additional arguments to pass to the fuzzer when running it.
    testcase_path: A path to a testcase (not necessarily in the sysroot) to run
      the fuzzer on.
    crash_expected: Is it normal for the fuzzer to crash on this run?
  """
  logging.info('Running fuzzer: %s', fuzzer)
  fuzzer_sysroot_path = GetFuzzerSysrootPath(fuzzer)
  fuzz_command = [fuzzer_sysroot_path.sysroot]
  fuzz_command += fuzz_args.split()

  if testcase_path:
    fuzz_command.append(testcase_path)
  else:
    LimitFuzzing(fuzz_command, corpus_path)

  if corpus_path:
    fuzz_command.append(corpus_path)

  if crash_expected:
    # Don't return nonzero when fuzzer OOMs, leaks, or timesout, since we don't
    # want an exception in those cases. The user may be trying to reproduce
    # those issues.
    fuzz_command += ['-error_exitcode=0', '-timeout_exitcode=0']

    # We must set exitcode=0 or else the fuzzer will return nonzero on
    # successful reproduction.
    sanitizer_options = {'exitcode': '0'}
  else:
    sanitizer_options = {}

  extra_env = GetFuzzExtraEnv(sanitizer_options)
  RunSysrootCommand(fuzz_command, extra_env=extra_env, debug_level=logging.INFO)


def MergeProfraw(fuzzer):
  """Merges profraw file from a fuzzer and creates a profdata file.

  Args:
    fuzzer: The fuzzer to merge the profraw file from.
  """
  profdata_path = GetProfdataPath(fuzzer)
  command = [
      'llvm-profdata',
      'merge',
      '-sparse',
      DEFAULT_PROFRAW_PATH,
      '-o',
      profdata_path.sysroot,
  ]

  RunSysrootCommand(command)
  return profdata_path


def GenerateCoverageReport(fuzzer, shared_libraries):
  """Generates an HTML coverage report from a fuzzer run.

  Args:
    fuzzer: The fuzzer to generate the coverage report for.
    shared_libraries: Libraries loaded dynamically by |fuzzer|.

  Returns:
    The path of the coverage report.
  """
  fuzzer_path = GetFuzzerSysrootPath(fuzzer).chroot
  command = ['llvm-cov', 'show', '-object', fuzzer_path]
  for library in shared_libraries:
    command += ['-object', library]

  coverage_directory = GetCoverageDirectory(fuzzer)
  command += [
      '-format=html',
      '-instr-profile=%s' % GetProfdataPath(fuzzer).chroot,
      '-output-dir=%s' % coverage_directory.chroot,
  ]

  # TODO(metzman): Investigate error messages printed by this command.
  cros_build_lib.run(command, redirect_stderr=True, debug_level=logging.DEBUG)
  return coverage_directory


def GetSharedLibraries(binary_path):
  """Gets the shared libraries used by a binary.

  Gets the shared libraries used by the binary. Based on GetSharedLibraries from
  src/tools/code_coverage/coverage_utils.py in Chromium.

  Args:
    binary_path: The path to the binary we want to find the shared libraries of.

  Returns:
    The shared libraries used by |binary_path|.
  """
  logging.info('Finding shared libraries for targets (if any).')
  shared_libraries = []
  elf_dict = lddtree.ParseELF(
      binary_path.chroot, root=SysrootPath.path_to_sysroot)
  for shared_library in elf_dict['libs'].values():
    shared_library_path = shared_library['path']

    if shared_library_path in shared_libraries:
      continue

    assert os.path.exists(shared_library_path), ('Shared library "%s" used by '
                                                 'the given target(s) does not '
                                                 'exist.' % shared_library_path)

    if IsInstrumentedWithClangCoverage(shared_library_path):
      # Do not add non-instrumented libraries. Otherwise, llvm-cov errors out.
      shared_libraries.append(shared_library_path)

  logging.debug('Found shared libraries (%d): %s.', len(shared_libraries),
                shared_libraries)
  logging.info('Finished finding shared libraries for targets.')
  return shared_libraries


def IsInstrumentedWithClangCoverage(binary_path):
  """Determines if a binary is instrumented with clang source based coverage.

  Args:
    binary_path: The path of the binary (executable or library) we are checking
      is instrumented with clang source based coverage.

  Returns:
    True if the binary is instrumented with clang source based coverage.
  """
  with open(binary_path, 'rb') as file_handle:
    elf_file = ELFFile(file_handle)
    return elf_file.get_section_by_name('__llvm_covmap') is not None


def RunFuzzerAndGenerateCoverageReport(fuzzer, corpus, fuzz_args):
  """Runs a fuzzer generates a coverage report and returns the report's path.

  Gets a coverage report for a fuzzer.

  Args:
    fuzzer: The fuzzer to run and generate the coverage report for.
    corpus: The path to a corpus to run the fuzzer on.
    fuzz_args: Additional arguments to pass to the fuzzer.

  Returns:
    The path to the coverage report.
  """
  corpus_path = CopyCorpusToSysroot(corpus)
  if corpus_path:
    corpus_path = corpus_path.sysroot

  RunFuzzer(fuzzer, corpus_path=corpus_path, fuzz_args=fuzz_args)
  MergeProfraw(fuzzer)
  fuzzer_sysroot_path = GetFuzzerSysrootPath(fuzzer)
  shared_libraries = GetSharedLibraries(fuzzer_sysroot_path)
  return GenerateCoverageReport(fuzzer, shared_libraries)


def RunSysrootCommand(command, **kwargs):
  """Runs command while chrooted into sysroot and returns the output.

  Args:
    command: A command to run in the sysroot.
    kwargs: Extra arguments to pass to cros_build_lib.sudo_run.

  Returns:
    The result of a call to cros_build_lib.sudo_run.
  """
  command = ['chroot', SysrootPath.path_to_sysroot] + command
  return sudo_run(command, **kwargs)


def GetBuildExtraEnv(build_type):
  """Gets the extra_env for building a package.

  Args:
    build_type: The type of build we want to do.

  Returns:
    The extra_env to use when building.
  """
  if build_type is None:
    build_type = BuildType.ASAN

  use_flags = os.environ.get('USE', '').split()
  # Check that the user hasn't already set USE flags that we can set.
  # No good way to iterate over an enum in python2.
  for use_flag in BuildType.CHOICES:
    if use_flag in use_flags:
      logging.warn('%s in USE flags. Please use --build_type instead.',
                   use_flag)

  # Set USE flags.
  fuzzer_build_type = 'fuzzer'
  use_flags += [fuzzer_build_type, build_type]
  features_flags = os.environ.get('FEATURES', '').split()
  if build_type == BuildType.COVERAGE:
    # We must use ASan when doing coverage builds.
    use_flags.append(BuildType.ASAN)
    # Use noclean so that a coverage report can be generated based on the source
    # code.
    features_flags.append('noclean')

  return {
      'FEATURES': ' '.join(features_flags),
      'USE': ' '.join(use_flags),
  }


def BuildPackage(package, board, build_type):
  """Builds a package on a specified board.

  Args:
    package: The package to build. Nothing is built if None.
    board: The board to build the package on.
    build_type: The type of the build to do (e.g. asan, msan, ubsan, coverage).
  """
  if package is None:
    return

  logging.info('Building %s using %s.', package, build_type)
  extra_env = GetBuildExtraEnv(build_type)
  build_packages_path = os.path.join(constants.SOURCE_ROOT, 'src', 'scripts',
                                     'build_packages')
  command = [
      build_packages_path,
      '--board',
      board,
      '--skip_chroot_upgrade',
      package,
  ]
  # For msan builds, always use "--nousepkg" since all package needs to be
  # instrumented with msan.
  if build_type == BuildType.MSAN:
    command += ['--nousepkg']

  # Print the output of the build command. Do this because it is familiar to
  # devs and we don't want to leave them not knowing about the build's progress
  # for a long time.
  cros_build_lib.run(command, extra_env=extra_env)


def DownloadFuzzerCorpus(fuzzer, dest_directory=None):
  """Downloads a corpus and returns its path.

  Downloads a corpus to a subdirectory of dest_directory if specified and
  returns path on the filesystem of the corpus. Asks users to authenticate
  if permission to read from bucket is denied.

  Args:
    fuzzer: The name of the fuzzer who's corpus we want to download.
    dest_directory: The directory to download the corpus to.

  Returns:
    The path to the downloaded corpus.

  Raises:
    gs.NoSuchKey: A corpus for the fuzzer doesn't exist.
    gs.GSCommandError: The corpus failed to download for another reason.
  """
  if not fuzzer.startswith('chromeos_'):
    # ClusterFuzz internally appends "chromeos_" to chromeos targets' names.
    # Therefore we must do so in order to find the corpus.
    fuzzer = 'chromeos_%s' % fuzzer

  if dest_directory is None:
    dest_directory = GetScriptStoragePath(CORPUS_DIRECTORY_NAME).chroot
  osutils.SafeMakedirsNonRoot(dest_directory)

  clusterfuzz_gcs_corpus_bucket = 'chromeos-corpus'
  suburl = 'libfuzzer/%s' % fuzzer
  gcs_path = gs.GetGsURL(
      clusterfuzz_gcs_corpus_bucket,
      for_gsutil=True,
      public=False,
      suburl=suburl)

  dest_path = os.path.join(dest_directory, fuzzer)

  try:
    logging.info('Downloading corpus to %s.', dest_path)
    ctx = gs.GSContext()
    ctx.Copy(
        gcs_path,
        dest_directory,
        recursive=True,
        parallel=True,
        debug_level=logging.DEBUG)
    logging.info('Finished downloading corpus.')
  except gs.GSNoSuchKey as exception:
    logging.error('Corpus for fuzzer: %s does not exist.', fuzzer)
    raise exception
  # Try to authenticate if we were denied permission to access the corpus.
  except gs.GSCommandError as exception:
    logging.error(
        'gsutil failed to download the corpus. You may need to log in. See:\n'
        'https://chromium.googlesource.com/chromiumos/docs/+/master/gsutil.md'
        '#setup\n'
        'for instructions on doing this.')
    raise exception

  return dest_path


def Reproduce(fuzzer, testcase_path):
  """Runs a fuzzer in the sysroot on a testcase.

  Args:
    fuzzer: The fuzzer to run.
    testcase_path: The path (not necessarily in the sysroot) of the testcase to
      run the fuzzer on.
  """
  testcase_sysroot_path = CopyTestcaseToSysroot(testcase_path).sysroot
  RunFuzzer(fuzzer, testcase_path=testcase_sysroot_path, crash_expected=True)


def SetUpSysrootForFuzzing():
  """Sets up the the sysroot for fuzzing

  Prepares the sysroot for fuzzing. Idempotent.
  """
  logging.info('Setting up sysroot for fuzzing.')
  # TODO(metzman): Don't create devices or mount /proc, use platform2_test.py
  # instead.
  # Mount /proc in sysroot and setup dev there because they are needed by
  # sanitizers.
  proc_manager = ProcManager()
  proc_manager.Mount()

  # Setup devices in /dev that are needed by libFuzzer.
  device_manager = DeviceManager()
  device_manager.SetUp()

  # Set up asan_symbolize.py, llvm-symbolizer, and llvm-profdata in the
  # sysroot so that fuzzer output (including stack traces) can be symbolized
  # and so that coverage reports can be generated.
  tool_manager = ToolManager()
  tool_manager.Install()

  osutils.SafeMakedirsNonRoot(GetSysrootPath(SCRIPT_STORAGE_PATH))


def CleanUpSysroot():
  """Cleans up the the sysroot from SetUpSysrootForFuzzing.

  Undoes SetUpSysrootForFuzzing. Idempotent.
  """
  logging.info('Cleaning up the sysroot.')
  proc_manager = ProcManager()
  proc_manager.Unmount()

  device_manager = DeviceManager()
  device_manager.CleanUp()

  tool_manager = ToolManager()
  tool_manager.Uninstall()
  osutils.RmDir(GetSysrootPath(SCRIPT_STORAGE_PATH), ignore_missing=True)


class ToolManager(object):
  """Class that installs or uninstalls fuzzing tools to/from the sysroot.

  Install and Uninstall methods are idempotent. Both are safe to call at any
  point.
  """

  # Path to asan_symbolize.py.
  ASAN_SYMBOLIZE_PATH = os.path.join('/', 'usr', 'bin', 'asan_symbolize.py')

  # List of LLVM binaries we must install in sysroot.
  LLVM_BINARY_NAMES = ['gdbserver', 'llvm-symbolizer', 'llvm-profdata']

  def __init__(self):
    self.asan_symbolize_sysroot_path = GetSysrootPath(self.ASAN_SYMBOLIZE_PATH)

  def Install(self):
    """Installs tools to the sysroot."""
    # Install asan_symbolize.py.
    sudo_run(['cp', self.ASAN_SYMBOLIZE_PATH, self.asan_symbolize_sysroot_path])
    # Install the LLVM binaries.
    # TODO(metzman): Build these tools so that we don't mess up when board is
    # for a different ISA.
    for llvm_binary in self._GetLLVMBinaries():
      llvm_binary.Install()

  def Uninstall(self):
    """Uninstalls tools from the sysroot. Undoes Install."""
    # Uninstall asan_symbolize.py.
    osutils.SafeUnlink(self.asan_symbolize_sysroot_path, sudo=True)
    # Uninstall the LLVM binaries.
    for llvm_binary in self._GetLLVMBinaries():
      llvm_binary.Uninstall()

  def _GetLLVMBinaries(self):
    """Creates LllvmBinary objects for each binary name in LLVM_BINARY_NAMES."""
    return [LlvmBinary(x) for x in self.LLVM_BINARY_NAMES]


class LlvmBinary(object):
  """Class for representing installing/uninstalling an LLVM binary in sysroot.

  Install and Uninstall methods are idempotent. Both are safe to call at any
  time.
  """

  # Path to the lddtree chromite script.
  LDDTREE_SCRIPT_PATH = os.path.join(constants.CHROMITE_BIN_DIR, 'lddtree')

  def __init__(self, binary):
    self.binary = binary
    self.install_dir = GetSysrootPath(
        os.path.join('/', 'usr', 'libexec', binary))
    self.binary_dir_path = GetSysrootPath(os.path.join('/', 'usr', 'bin'))
    self.binary_chroot_dest_path = os.path.join(self.binary_dir_path, binary)

  def Uninstall(self):
    """Removes an LLVM binary from sysroot. Undoes Install."""
    osutils.RmDir(self.install_dir, ignore_missing=True, sudo=True)
    osutils.SafeUnlink(self.binary_chroot_dest_path, sudo=True)

  def Install(self):
    """Installs (sets up) an LLVM binary in the sysroot.

    Sets up an llvm binary in the sysroot so that it can be run there.
    """
    # Create a directory for installing |binary| and all of its dependencies in
    # the sysroot.
    binary_rel_path = ['usr', 'bin', self.binary]
    binary_chroot_path = os.path.join('/', *binary_rel_path)
    if not os.path.exists(binary_chroot_path):
      logging.warning('Cannot copy %s, file does not exist in chroot.',
                      binary_chroot_path)
      logging.warning('Functionality provided by %s will be missing.',
                      binary_chroot_path)
      return

    osutils.SafeMakedirsNonRoot(self.install_dir)

    # Copy the binary and everything needed to run it into the sysroot.
    cmd = [
        self.LDDTREE_SCRIPT_PATH,
        '-v',
        '--generate-wrappers',
        '--root',
        '/',
        '--copy-to-tree',
        self.install_dir,
        binary_chroot_path,
    ]
    sudo_run(cmd)

    # Create a symlink to the copy of the binary (we can't do lddtree in
    # self.binary_dir_path). Note that symlink should be relative so that it
    # will be valid when chrooted into the sysroot.
    rel_path = os.path.relpath(self.install_dir, self.binary_dir_path)
    link_path = os.path.join(rel_path, *binary_rel_path)
    osutils.SafeSymlink(link_path, self.binary_chroot_dest_path, sudo=True)


class DeviceManager(object):
  """Class that creates or removes devices from /dev in sysroot.

  SetUp and CleanUp methods are idempotent. Both are safe to call at any point.
  """

  DEVICE_MKNOD_PARAMS = {
      'null': (666, 3),
      'random': (444, 8),
      'urandom': (444, 9),
  }

  MKNOD_MAJOR = '1'

  def __init__(self):
    self.dev_path_chroot = GetSysrootPath('/dev')

  def _GetDevicePath(self, device_name):
    """Returns the path of |device_name| in sysroot's /dev."""
    return os.path.join(self.dev_path_chroot, device_name)

  def SetUp(self):
    """Sets up devices in the sysroot's /dev.

    Creates /dev/null, /dev/random, and /dev/urandom. If they already exist then
    recreates them.
    """
    self.CleanUp()
    osutils.SafeMakedirsNonRoot(self.dev_path_chroot)
    for device, mknod_params in self.DEVICE_MKNOD_PARAMS.items():
      device_path = self._GetDevicePath(device)
      self._MakeCharDevice(device_path, *mknod_params)

  def CleanUp(self):
    """Cleans up devices in the sysroot's /dev. Undoes SetUp.

    Removes /dev/null, /dev/random, and /dev/urandom if they exist.
    """
    for device in self.DEVICE_MKNOD_PARAMS:
      device_path = self._GetDevicePath(device)
      if os.path.exists(device_path):
        # Use -r since dev/null is sometimes a directory.
        sudo_run(['rm', '-r', device_path])

  def _MakeCharDevice(self, path, mode, minor):
    """Make a character device."""
    mode = str(mode)
    minor = str(minor)
    command = ['mknod', '-m', mode, path, 'c', self.MKNOD_MAJOR, minor]
    sudo_run(command)


class ProcManager(object):
  """Class that mounts or unmounts /proc in sysroot.

  Mount and Unmount are idempotent. Both are safe to call at any point.
  """

  PROC_PATH = '/proc'

  def __init__(self):
    self.proc_path_chroot = GetSysrootPath(self.PROC_PATH)
    self.is_mounted = osutils.IsMounted(self.proc_path_chroot)

  def Unmount(self):
    """Unmounts /proc in chroot. Undoes Mount."""
    if not self.is_mounted:
      return
    osutils.UmountDir(self.proc_path_chroot, cleanup=False)

  def Mount(self):
    """Mounts /proc in chroot. Remounts it if already mounted."""
    self.Unmount()
    osutils.MountDir(
        self.PROC_PATH,
        self.proc_path_chroot,
        'proc',
        debug_level=logging.DEBUG)


def EnterSysrootShell():
  """Spawns and gives user access to a bash shell in the sysroot."""
  command = ['/bin/bash', '-i']
  return RunSysrootCommand(
      command,
      extra_env=GetFuzzExtraEnv(),
      debug_level=logging.INFO,
      error_code_ok=True).returncode


def StripFuzzerPrefixes(fuzzer_name):
  """Strip the prefix ClusterFuzz uses in case they are specified.

  Strip the prefixes used by ClusterFuzz if the users has included them by
  accident.

  Args:
    fuzzer_name: The fuzzer who's name may contain prefixes.

  Returns:
    The name of the fuzz target without prefixes.
  """
  initial_name = fuzzer_name

  def StripPrefix(prefix):
    if fuzzer_name.startswith(prefix):
      return fuzzer_name[len(prefix):]
    return fuzzer_name

  clusterfuzz_prefixes = ['libFuzzer_', 'chromeos_']

  for prefix in clusterfuzz_prefixes:
    fuzzer_name = StripPrefix(prefix)

  if initial_name != fuzzer_name:
    logging.warn(
        '%s contains a prefix from ClusterFuzz (one or more of %s) that is not '
        "part of the fuzzer's name. Interpreting --fuzzer as %s.",
        initial_name, clusterfuzz_prefixes, fuzzer_name)

  return fuzzer_name


def ExecuteShellCommand():
  """Executes the "shell" command.

  Sets up the sysroot for fuzzing and gives user access to a bash shell it
  spawns in the sysroot.

  Returns:
    The exit code of the shell command.
  """
  SetUpSysrootForFuzzing()
  return EnterSysrootShell()


def ExecuteSetupCommand():
  """Executes the "setup" command. Wrapper for SetUpSysrootForFuzzing.

  Sets up the sysroot for fuzzing.
  """
  SetUpSysrootForFuzzing()


def ExecuteCleanupCommand():
  """Executes the "cleanup" command. Wrapper for CleanUpSysroot.

  Undoes pre-fuzzing setup.
  """
  CleanUpSysroot()


def ExecuteCoverageCommand(options):
  """Executes the "coverage" command.

  Executes the "coverage" command by optionally doing a coverage build of a
  package, optionally downloading the fuzzer's corpus, optionally copying it
  into the sysroot, running the fuzzer and then generating a coverage report
  for the user to view. Causes program to exit if fuzzer is not instrumented
  with source based coverage.

  Args:
    options: The parsed arguments passed to this program.
  """
  BuildPackage(options.package, options.board, BuildType.COVERAGE)

  fuzzer = StripFuzzerPrefixes(options.fuzzer)
  fuzzer_sysroot_path = GetFuzzerSysrootPath(fuzzer)
  if not IsInstrumentedWithClangCoverage(fuzzer_sysroot_path.chroot):
    # Don't run the fuzzer if it isn't instrumented with source based coverage.
    # Quit and let the user know how to build the fuzzer properly.
    cros_build_lib.Die(
        '%s is not instrumented with source based coverage.\nSpecify --package '
        'to do a coverage build or build with USE flag: "coverage".', fuzzer)

  corpus = options.corpus
  if options.download:
    corpus = DownloadFuzzerCorpus(options.fuzzer)

  # Set up sysroot for fuzzing.
  SetUpSysrootForFuzzing()

  coverage_report_path = RunFuzzerAndGenerateCoverageReport(
      fuzzer, corpus, options.fuzz_args)

  # Get path on host so user can access it with their browser.
  # TODO(metzman): Add the ability to convert to host paths to path_util.
  external_trunk_path = os.getenv('EXTERNAL_TRUNK_PATH')
  coverage_report_host_path = os.path.join(external_trunk_path, 'chroot',
                                           coverage_report_path.chroot[1:])
  print('Coverage report written to file://%s/index.html' %
        coverage_report_host_path)


def ExecuteDownloadCommand(options):
  """Executes the "download" command. Wrapper around DownloadFuzzerCorpus."""
  DownloadFuzzerCorpus(StripFuzzerPrefixes(options.fuzzer), options.directory)


def ExecuteReproduceCommand(options):
  """Executes the "reproduce" command.

  Executes the "reproduce" command by Running a fuzzer on a testcase.
  May build the fuzzer before running.

  Args:
    options: The parsed arguments passed to this program.
  """
  if options.build_type and not options.package:
    raise Exception('Cannot specify --build_type without specifying --package.')

  # Verify that "msan-fuzzer" profile is being used with msan.
  # Check presence of "-fsanitize=memory" in CFLAGS.
  if options.build_type == BuildType.MSAN:
    cmd = ['portageq-%s' % options.board, 'envvar', 'CFLAGS']
    cflags = cros_build_lib.run(cmd, capture_output=True).output.splitlines()
    check_string = '-fsanitize=memory'
    if not any(check_string in s for s in cflags):
      logging.error(
          '-fsanitize=memory not found in CFLAGS. '
          'Use "setup_board --board=amd64-generic --profile=msan-fuzzer" '
          'for MSan Fuzzing Builds.')
      raise Exception('Incompatible profile used for msan fuzzing.')

  BuildPackage(options.package, options.board, options.build_type)
  SetUpSysrootForFuzzing()
  Reproduce(StripFuzzerPrefixes(options.fuzzer), options.testcase)


def InstallBaseDependencies(options):
  """ Installs the base packages needed to chroot in board sysroot.

  Args:
    options: The parsed arguments passed to this program.
  """
  package = 'virtual/implicit-system'
  if not portage_util.IsPackageInstalled(
      package, sysroot=SysrootPath.path_to_sysroot):
    build_type = getattr(options, 'build_type', None)
    BuildPackage(package, options.board, build_type)


def ParseArgs(argv):
  """Parses program arguments.

  Args:
    argv: The program arguments we want to parse.

  Returns:
    An options object which will tell us which command to run and which options
    to use for that command.
  """
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument(
      '--board',
      default=cros_build_lib.GetDefaultBoard(),
      help='Board on which to run test.')

  subparsers = parser.add_subparsers(dest='command')

  subparsers.add_parser('cleanup', help='Undo setup command.')
  coverage_parser = subparsers.add_parser(
      'coverage', help='Get a coverage report for a fuzzer.')

  coverage_parser.add_argument('--package', help='Package to build.')

  corpus_parser = coverage_parser.add_mutually_exclusive_group()
  corpus_parser.add_argument('--corpus', help='Corpus to run fuzzer on.')

  corpus_parser.add_argument(
      '--download',
      action='store_true',
      help='Generate coverage report based on corpus from ClusterFuzz.')

  coverage_parser.add_argument(
      '--fuzzer',
      required=True,
      help='The fuzz target to generate a coverage report for.')

  coverage_parser.add_argument(
      '--fuzz-args',
      default='',
      help='Arguments to pass libFuzzer. '
      'Please use an equals sign or parsing will fail '
      '(i.e. --fuzzer_args="-rss_limit_mb=2048 -print_funcs=1").')

  download_parser = subparsers.add_parser('download', help='Download a corpus.')

  download_parser.add_argument(
      '--directory', help='Path to directory to download the corpus to.')

  download_parser.add_argument(
      '--fuzzer', required=True, help='Fuzzer to download the corpus for.')

  reproduce_parser = subparsers.add_parser(
      'reproduce', help='Run a fuzzer on a testcase.')

  reproduce_parser.add_argument(
      '--testcase', required=True, help='Path of testcase to run fuzzer on.')

  reproduce_parser.add_argument(
      '--fuzzer', required=True, help='Fuzzer to reproduce the crash on.')

  reproduce_parser.add_argument('--package', help='Package to build.')

  reproduce_parser.add_argument(
      '--build-type',
      choices=BuildType.CHOICES,
      help='Type of build.',
      type=str.lower)  # Ignore sanitizer case.

  subparsers.add_parser('setup', help='Set up the sysroot to test fuzzing.')

  subparsers.add_parser(
      'shell',
      help='Set up sysroot for fuzzing and get a shell in the sysroot.')

  opts = parser.parse_args(argv)
  opts.Freeze()
  return opts


def main(argv):
  """Parses arguments and executes a command.

  Args:
    argv: The prorgram arguments.

  Returns:
    0 on success. Non-zero on failure.
  """
  cros_build_lib.AssertInsideChroot()
  options = ParseArgs(argv)
  if options.board is None:
    logging.error('Please specify "--board" or set ".default_board".')
    return 1

  SysrootPath.SetPathToSysroot(options.board)

  InstallBaseDependencies(options)

  if options.command == 'cleanup':
    ExecuteCleanupCommand()
  elif options.command == 'coverage':
    ExecuteCoverageCommand(options)
  elif options.command == 'setup':
    ExecuteSetupCommand()
  elif options.command == 'download':
    ExecuteDownloadCommand(options)
  elif options.command == 'reproduce':
    ExecuteReproduceCommand(options)
  elif options.command == 'shell':
    return ExecuteShellCommand()

  return 0
