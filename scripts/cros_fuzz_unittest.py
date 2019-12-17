# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_fuzz."""

from __future__ import print_function

import os

from chromite.lib import cros_logging as logging
from chromite.lib import cros_test_lib
from chromite.scripts import cros_fuzz

DEFAULT_MAX_TOTAL_TIME_OPTION = cros_fuzz.GetLibFuzzerOption(
    cros_fuzz.MAX_TOTAL_TIME_OPTION_NAME,
    cros_fuzz.MAX_TOTAL_TIME_DEFAULT_VALUE)

FUZZ_TARGET = 'fuzzer'
FUZZER_COVERAGE_PATH = (
    '/build/amd64-generic/tmp/fuzz/coverage-report/%s' % FUZZ_TARGET)

BOARD = 'amd64-generic'


class SysrootPathTest(cros_test_lib.TestCase):
  """Tests the SysrootPath class."""

  def setUp(self):
    self.path_to_sysroot = _SetPathToSysroot()
    self.sysroot_relative_path = '/dir'
    self.basename = os.path.basename(self.sysroot_relative_path)
    # Chroot relative path of a path that is in the sysroot.
    self.path_in_sysroot = os.path.join(self.path_to_sysroot, self.basename)

  def testSysroot(self):
    """Tests that SysrootPath.sysroot returns expected result."""
    sysroot_path = cros_fuzz.SysrootPath(self.sysroot_relative_path)
    self.assertEqual(self.sysroot_relative_path, sysroot_path.sysroot)

  def testChroot(self):
    """Tests that SysrootPath.chroot returns expected result."""
    sysroot_path = cros_fuzz.SysrootPath(self.sysroot_relative_path)
    expected = os.path.join(self.path_to_sysroot, self.basename)
    self.assertEqual(expected, sysroot_path.chroot)

  def testIsSysrootPath(self):
    """Tests that the IsSysrootPath can tell what is in the sysroot."""
    self.assertTrue(cros_fuzz.SysrootPath.IsPathInSysroot(self.path_to_sysroot))
    self.assertTrue(cros_fuzz.SysrootPath.IsPathInSysroot(self.path_in_sysroot))
    path_not_in_sysroot_1 = os.path.join(
        os.path.dirname(self.path_to_sysroot), self.basename)
    self.assertFalse(
        cros_fuzz.SysrootPath.IsPathInSysroot(path_not_in_sysroot_1))
    path_not_in_sysroot_2 = os.path.join('/dir/build/amd64-generic')
    self.assertFalse(
        cros_fuzz.SysrootPath.IsPathInSysroot(path_not_in_sysroot_2))

  def testFromChrootPathInSysroot(self):
    """Tests that FromChrootPathInSysroot converts paths properly."""
    # Test that it raises an assertion error when the path is not in the
    # sysroot.
    path_not_in_sysroot_1 = os.path.join(
        os.path.dirname(self.path_to_sysroot), 'dir')
    with self.assertRaises(AssertionError):
      cros_fuzz.SysrootPath.FromChrootPathInSysroot(path_not_in_sysroot_1)

    sysroot_path = cros_fuzz.SysrootPath.FromChrootPathInSysroot(
        self.path_in_sysroot)
    self.assertEqual(self.sysroot_relative_path, sysroot_path)


class GetPathForCopyTest(cros_test_lib.TestCase):
  """Tests GetPathForCopy."""

  def testGetPathForCopy(self):
    """Test that GetPathForCopy gives us the correct sysroot directory."""
    _SetPathToSysroot()
    directory = '/path/to/directory'
    parent = 'parent'
    child = os.path.basename(directory)
    path_to_sysroot = cros_fuzz.SysrootPath.path_to_sysroot
    storage_directory = cros_fuzz.SCRIPT_STORAGE_DIRECTORY

    sysroot_path = cros_fuzz.GetPathForCopy(parent, child)

    expected_chroot_path = os.path.join(path_to_sysroot, 'tmp',
                                        storage_directory, parent, child)
    self.assertEqual(expected_chroot_path, sysroot_path.chroot)

    expected_sysroot_path = os.path.join('/', 'tmp', storage_directory, parent,
                                         child)
    self.assertEqual(expected_sysroot_path, sysroot_path.sysroot)


class GetLibFuzzerOptionTest(cros_test_lib.TestCase):
  """Tests GetLibFuzzerOption."""

  def testGetLibFuzzerOption(self):
    """Tests that GetLibFuzzerOption returns a correct libFuzzer option."""
    expected = '-max_total_time=60'
    self.assertEqual(expected, cros_fuzz.GetLibFuzzerOption(
        'max_total_time', 60))


class LimitFuzzingTest(cros_test_lib.TestCase):
  """Tests LimitFuzzing."""

  def setUp(self):
    self.fuzz_command = ['./fuzzer', '-rss_limit_mb=4096']
    self.corpus = None

  def _Helper(self, expected_command=None):
    """Calls LimitFuzzing and asserts fuzz_command equals |expected_command|.

    If |expected| is None, then it is set to self.fuzz_command before calling
    LimitFuzzing.
    """
    if expected_command is None:
      expected_command = self.fuzz_command[:]
    cros_fuzz.LimitFuzzing(self.fuzz_command, self.corpus)
    self.assertEqual(expected_command, self.fuzz_command)

  def testCommandHasMaxTotalTime(self):
    """Tests that no limit is added when user specifies -max_total_time."""
    self.fuzz_command.append('-max_total_time=60')
    self._Helper()

  def testCommandHasRuns(self):
    """Tests that no limit is added when user specifies -runs"""
    self.fuzz_command.append('-runs=1')
    self._Helper()

  def testCommandHasCorpus(self):
    """Tests that a limit is added when user specifies a corpus."""
    self.corpus = 'corpus'
    expected = self.fuzz_command + ['-runs=0']
    self._Helper(expected)

  def testNoLimitOrCorpus(self):
    """Tests that a limit is added when user specifies no corpus or limit."""
    expected = self.fuzz_command + [DEFAULT_MAX_TOTAL_TIME_OPTION]
    self._Helper(expected)


class RunSysrootCommandMockTestCase(cros_test_lib.MockTestCase):
  """Class for TestCases that call RunSysrootCommand."""

  def setUp(self):
    _SetPathToSysroot()
    self.expected_command = None
    self.expected_extra_env = None
    self.PatchObject(
        cros_fuzz,
        'RunSysrootCommand',
        side_effect=self.MockedRunSysrootCommand)


  def MockedRunSysrootCommand(self, command, extra_env=None,
                              **kwargs):    # pylint: disable=unused-argument
    """The mocked version of RunSysrootCommand.

    Asserts |command| and |extra_env| are what is expected.
    """
    self.assertEqual(self.expected_command, command)
    self.assertEqual(self.expected_extra_env, extra_env)


class RunFuzzerTest(RunSysrootCommandMockTestCase):
  """Tests RunFuzzer."""

  def setUp(self):
    self.corpus_path = None
    self.fuzz_args = ''
    self.testcase_path = None
    self.expected_command = [
        cros_fuzz.GetFuzzerSysrootPath(FUZZ_TARGET).sysroot,
    ]
    self.expected_extra_env = {
        'ASAN_OPTIONS': 'log_path=stderr',
        'MSAN_OPTIONS': 'log_path=stderr',
        'UBSAN_OPTIONS': 'log_path=stderr',
    }

  def _Helper(self):
    """Calls RunFuzzer."""
    cros_fuzz.RunFuzzer(FUZZ_TARGET, self.corpus_path, self.fuzz_args,
                        self.testcase_path)

  def testNoOptional(self):
    """Tests correct command and env used when not specifying optional."""
    self.expected_command.append(DEFAULT_MAX_TOTAL_TIME_OPTION)
    self._Helper()

  def testFuzzArgs(self):
    """Tests that the correct command is used when fuzz_args is specified."""
    fuzz_args = [DEFAULT_MAX_TOTAL_TIME_OPTION, '-fake_arg=fake_value']
    self.expected_command.extend(fuzz_args)
    self.fuzz_args = ' '.join(fuzz_args)
    self._Helper()

  def testTestCase(self):
    """Tests a testcase is used when specified."""
    self.testcase_path = '/path/to/testcase'
    self.expected_command.append(self.testcase_path)
    self._Helper()


class MergeProfrawTest(RunSysrootCommandMockTestCase):
  """Tests MergeProfraw."""

  def testMergeProfraw(self):
    """Tests that MergeProfraw works as expected."""
    # Parent class will assert that these commands are used.
    profdata_path = cros_fuzz.GetProfdataPath(FUZZ_TARGET)
    self.expected_command = [
        'llvm-profdata',
        'merge',
        '-sparse',
        cros_fuzz.DEFAULT_PROFRAW_PATH,
        '-o',
        profdata_path.sysroot,
    ]
    cros_fuzz.MergeProfraw(FUZZ_TARGET)


class GenerateCoverageReportTest(cros_test_lib.RunCommandTestCase):
  """Tests GenerateCoverageReport."""

  def setUp(self):
    _SetPathToSysroot()
    self.fuzzer_path = cros_fuzz.GetFuzzerSysrootPath(FUZZ_TARGET).chroot
    self.profdata_path = cros_fuzz.GetProfdataPath(FUZZ_TARGET)

  def testWithSharedLibraries(self):
    """Tests that right command is used when specifying shared libraries."""
    shared_libraries = ['shared_lib.so']
    cros_fuzz.GenerateCoverageReport(FUZZ_TARGET, shared_libraries)
    instr_profile_option = '-instr-profile=%s' % self.profdata_path.chroot
    output_dir_option = '-output-dir=%s' % FUZZER_COVERAGE_PATH
    expected_command = [
        'llvm-cov',
        'show',
        '-object',
        self.fuzzer_path,
        '-object',
        'shared_lib.so',
        '-format=html',
        instr_profile_option,
        output_dir_option,
    ]
    self.assertCommandCalled(
        expected_command, stderr=True, debug_level=logging.DEBUG)

  def testNoSharedLibraries(self):
    """Tests the right coverage command is used without shared libraries."""
    shared_libraries = []
    cros_fuzz.GenerateCoverageReport(FUZZ_TARGET, shared_libraries)
    instr_profile_option = '-instr-profile=%s' % self.profdata_path.chroot
    output_dir_option = '-output-dir=%s' % FUZZER_COVERAGE_PATH
    expected_command = [
        'llvm-cov', 'show', '-object', self.fuzzer_path, '-format=html',
        instr_profile_option, output_dir_option
    ]
    self.assertCommandCalled(
        expected_command, stderr=True, debug_level=logging.DEBUG)


class RunSysrootCommandTest(cros_test_lib.RunCommandTestCase):
  """Tests RunSysrootCommand."""

  def testRunSysrootCommand(self):
    """Tests RunSysrootCommand creates a proper command to run in sysroot."""
    command = ['./fuzz', '-rss_limit_mb=4096']
    cros_fuzz.RunSysrootCommand(command)
    sysroot_path = _SetPathToSysroot()
    expected_command = ['sudo', '--', 'chroot', sysroot_path]
    expected_command.extend(command)
    self.assertCommandCalled(expected_command, debug_level=logging.DEBUG)


class GetBuildExtraEnvTest(cros_test_lib.TestCase):
  """Tests GetBuildExtraEnv."""

  TEST_ENV_VAR = 'TEST_VAR'

  def testUseAndFeaturesNotClobbered(self):
    """Tests that values of certain environment variables are appended to."""
    vars_and_values = {'FEATURES': 'foo', 'USE': 'bar'}
    for var, value in vars_and_values.items():
      os.environ[var] = value
    extra_env = cros_fuzz.GetBuildExtraEnv(cros_fuzz.BuildType.COVERAGE)
    for var, value in vars_and_values.items():
      self.assertIn(value, extra_env[var])

  def testCoverageBuild(self):
    """Tests that a proper environment is returned for a coverage build."""
    extra_env = cros_fuzz.GetBuildExtraEnv(cros_fuzz.BuildType.COVERAGE)
    for expected_flag in ['fuzzer', 'coverage', 'asan']:
      self.assertIn(expected_flag, extra_env['USE'])
    self.assertIn('noclean', extra_env['FEATURES'])


def _SetPathToSysroot():
  """Calls SysrootPath.SetPathToSysroot and returns result."""
  return cros_fuzz.SysrootPath.SetPathToSysroot(BOARD)
