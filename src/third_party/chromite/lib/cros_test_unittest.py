# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for CrOSTest."""

from __future__ import print_function

import os

import mock

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.lib import cros_test
from chromite.utils import outcap


# pylint: disable=protected-access
class CrOSTesterBase(cros_test_lib.RunCommandTempDirTestCase):
  """Base class for setup and creating a temp file path."""

  def setUp(self):
    """Common set up method for all tests."""
    opts = cros_test.ParseCommandLine([])
    self._tester = cros_test.CrOSTest(opts)
    self._tester._device.board = 'amd64-generic'
    self._tester._device.image_path = self.TempFilePath(
        'chromiumos_qemu_image.bin')
    osutils.Touch(self._tester._device.image_path)
    version_str = ('QEMU emulator version 2.6.0, Copyright (c) '
                   '2003-2008 Fabrice Bellard')
    self.rc.AddCmdResult(partial_mock.In('--version'), output=version_str)
    self.ssh_port = self._tester._device.ssh_port

  def TempFilePath(self, file_path):
    """Creates a temporary file path lasting for the duration of a test."""
    return os.path.join(self.tempdir, file_path)


class CrOSTester(CrOSTesterBase):
  """Tests miscellaneous utility methods"""

  def testStartVM(self):
    """Verify that a new VM is started before running tests."""
    self._tester.start_vm = True
    self._tester.Run()
    # Check if new VM got launched.
    self.assertCommandContains([self._tester._device.qemu_path, '-enable-kvm'])
    # Check if new VM is responsive.
    self.assertCommandContains(
        ['ssh', '-p', '9222', 'root@localhost', '--', 'true'])

  def testDeployChrome(self):
    """Tests basic deploy chrome command."""
    self._tester.deploy = True
    self._tester.build_dir = self.TempFilePath('out_amd64-generic/Release')
    self._tester.Run()
    self.assertCommandContains(['deploy_chrome', '--force', '--build-dir',
                                self._tester.build_dir, '--process-timeout',
                                '180', '--to', self._tester._device.device,
                                '--port', '9222', '--board', 'amd64-generic',
                                '--cache-dir', self._tester.cache_dir])

  def testDeployChromeWithArgs(self):
    """Tests deploy chrome command with additional arguments."""
    self._tester.deploy = True
    self._tester.build_dir = self.TempFilePath('out_amd64-generic/Release')
    self._tester.nostrip = True
    self._tester.mount = True
    self._tester.Run()
    self.assertCommandContains(['--nostrip', '--mount'])

  def testFetchResults(self):
    """Verify that results files/directories are copied from the DUT."""
    self._tester.results_src = ['/tmp/results/cmd_results',
                                '/tmp/results/filename.txt',
                                '/tmp/results/test_results']
    self._tester.results_dest_dir = self.TempFilePath('results_dir')
    osutils.SafeMakedirs(self._tester.results_dest_dir)
    self._tester.Run()
    for filename in self._tester.results_src:
      self.assertCommandContains(['scp', 'root@localhost:%s' % filename,
                                  self._tester.results_dest_dir])

  def testFileList(self):
    """Verify that FileList returns the correct files."""
    # Ensure FileList returns files when files_from is None.
    files = ['/tmp/filename1', '/tmp/filename2']
    self.assertEqual(files, cros_test.FileList(files, None))

    # Ensure FileList returns files when files_from does not exist.
    files_from = self.TempFilePath('file_list')
    self.assertEqual(files, cros_test.FileList(files, files_from))

    # Ensure FileList uses 'files_from' and ignores 'files'.
    file_list = ['/tmp/file1', '/tmp/file2', '/tmp/file3']
    osutils.WriteFile(files_from, '\n'.join(file_list))
    self.assertEqual(file_list, cros_test.FileList(files, files_from))


class CrOSTesterMiscTests(CrOSTesterBase):
  """Tests miscellaneous test cases."""

  @mock.patch('chromite.lib.vm.VM.IsRunning', return_value=True)
  def testBasic(self, isrunning_mock):
    """Tests basic functionality."""
    self._tester.Run()
    isrunning_mock.assert_called()
    # Run vm_sanity.
    self.assertCommandContains([
        'ssh', '-p', '9222', 'root@localhost', '--',
        '/usr/local/autotest/bin/vm_sanity.py'
    ])

  def testCatapult(self):
    """Verify catapult test command."""
    self._tester.catapult_tests = ['testAddResults']
    self._tester.Run()
    self.assertCommandContains([
        'python', '/usr/local/telemetry/src/third_party/catapult/'
        'telemetry/bin/run_tests', '--browser=system', 'testAddResults'
    ])

  def testCatapultAsGuest(self):
    """Verify that we use the correct browser in guest mode."""
    self._tester.catapult_tests = ['testAddResults']
    self._tester.guest = True
    self._tester.Run()
    self.assertCommandContains([
        'python', '/usr/local/telemetry/src/third_party/catapult/'
        'telemetry/bin/run_tests', '--browser=system-guest', 'testAddResults'
    ])

  def testRunDeviceCmd(self):
    """Verify a run device cmd call."""
    self._tester.remote_cmd = True
    self._tester.files = [self.TempFilePath('crypto_unittests')]
    osutils.Touch(self._tester.files[0], mode=0o700)
    self._tester.as_chronos = True
    self._tester.args = ['crypto_unittests',
                         '--test-launcher-print-test-stdio=always']

    self._tester.Run()

    # Ensure target directory is created on the DUT.
    self.assertCommandContains(['mkdir', '-p', '/usr/local/cros_test'])
    # Ensure test ssh keys are authorized with chronos.
    self.assertCommandContains(['cp', '-r', '/root/.ssh/',
                                '/home/chronos/user/'])
    # Ensure chronos has ownership of the directory.
    self.assertCommandContains(['chown', '-R', 'chronos:',
                                '/usr/local/cros_test'])
    # Ensure command runs in the target directory.
    self.assertCommandContains('"cd /usr/local/cros_test && crypto_unittests '
                               '--test-launcher-print-test-stdio=always"')
    # Ensure target directory is removed at the end of the test.
    self.assertCommandContains(['rm', '-rf', '/usr/local/cros_test'])

  def testRunDeviceCmdWithSetCwd(self):
    """Verify a run device command call when giving a cwd."""
    self._tester.remote_cmd = True
    self._tester.cwd = '/usr/local/autotest'
    self._tester.args = ['./bin/vm_sanity.py']

    self._tester.Run()

    # Ensure command runs in the autotest directory.
    self.assertCommandContains('"cd /usr/local/autotest && ./bin/vm_sanity.py"')

  def testRunDeviceCmdWithoutSrcFiles(self):
    """Verify running a remote command when src files are not specified.

    The remote command should not change the working directory or create a temp
    directory on the target.
    """
    self._tester.remote_cmd = True
    self._tester.args = ['/usr/local/autotest/bin/vm_sanity.py']
    self._tester.Run()
    self.assertCommandContains(['ssh', '-p', '9222',
                                '/usr/local/autotest/bin/vm_sanity.py'])
    self.assertCommandContains(['mkdir', '-p'], expected=False)
    self.assertCommandContains(['"cd %s && /usr/local/autotest/bin/'
                                'vm_sanity.py"' % self._tester.cwd],
                               expected=False)
    self.assertCommandContains(['rm', '-rf'], expected=False)

  def testHostCmd(self):
    """Verify running a host command."""
    self._tester.host_cmd = True
    self._tester.build_dir = '/some/chromium/dir'
    self._tester.args = ['tast', 'run', 'localhost:9222', 'ui.ChromeLogin']
    self._tester.Run()
    # Ensure command is run with an env var for the build dir, and ensure an
    # exception is not raised if it fails.
    self.assertCommandCalled(
        ['tast', 'run', 'localhost:9222', 'ui.ChromeLogin'],
        error_code_ok=True,
        extra_env={'CHROMIUM_OUTPUT_DIR': '/some/chromium/dir'})
    # Ensure that --host-cmd does not invoke ssh since it runs on the host.
    self.assertCommandContains(['ssh', 'tast'], expected=False)


class CrOSTesterAutotest(CrOSTesterBase):
  """Tests autotest test cases."""

  def testBasicAutotest(self):
    """Tests a simple autotest call."""
    self._tester.autotest = ['accessibility_Sanity']
    self._tester.Run()

    # Check VM got launched.
    self.assertCommandContains([self._tester._device.qemu_path, '-enable-kvm'])

    # Checks that autotest is running.
    self.assertCommandContains([
        'test_that', '--no-quickmerge', '--ssh_options',
        '-F /dev/null -i /dev/null',
        'localhost:9222', 'accessibility_Sanity'])

  def testAutotestWithArgs(self):
    """Tests an autotest call with attributes."""
    self._tester.autotest = ['accessibility_Sanity']
    self._tester.results_dir = 'test_results'
    self._tester._device.private_key = '.ssh/testing_rsa'
    self._tester._device.log_level = 'debug'
    self._tester._device.ssh_port = None
    self._tester._device.device = '100.90.29.199'
    self._tester.test_that_args = ['--test_that-args',
                                   '--whitelist-chrome-crashes']

    cwd = os.path.join('/mnt/host/source',
                       os.path.relpath(os.getcwd(), constants.SOURCE_ROOT))
    test_results_dir = os.path.join(cwd, 'test_results')
    testing_rsa_dir = os.path.join(cwd, '.ssh/testing_rsa')

    self._tester._RunAutotest()

    self.assertCommandCalled(
        ['test_that', '--board', 'amd64-generic', '--results_dir',
         test_results_dir, '--ssh_private_key', testing_rsa_dir, '--debug',
         '--whitelist-chrome-crashes', '--no-quickmerge', '--ssh_options',
         '-F /dev/null -i /dev/null', '100.90.29.199', 'accessibility_Sanity'],
        enter_chroot=not cros_build_lib.IsInsideChroot())

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=True)
  def testInsideChrootAutotest(self, check_inside_chroot_mock):
    """Tests running an autotest from within the chroot."""
    # Checks that mock version has been called.
    check_inside_chroot_mock.assert_called()

    self._tester.autotest = ['accessibility_Sanity']
    self._tester.results_dir = '/mnt/host/source/test_results'
    self._tester._device.private_key = '/mnt/host/source/.ssh/testing_rsa'

    self._tester._RunAutotest()

    self.assertCommandContains([
        '--results_dir', '/mnt/host/source/test_results',
        '--ssh_private_key', '/mnt/host/source/.ssh/testing_rsa'])

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot', return_value=False)
  def testOutsideChrootAutotest(self, check_inside_chroot_mock):
    """Tests running an autotest from outside the chroot."""
    # Checks that mock version has been called.
    check_inside_chroot_mock.assert_called()

    self._tester.autotest = ['accessibility_Sanity']
    # Capture the run command. This is necessary beacuse the mock doesn't
    # capture the cros_sdk wrapper.
    with outcap.OutputCapturer() as output:
      self._tester._RunAutotest()
    # Check that we enter the chroot before running test_that.
    self.assertIn(
        'cros_sdk -- test_that --board amd64-generic --no-quickmerge'
        " --ssh_options '-F /dev/null -i /dev/null' localhost:9222"
        ' accessibility_Sanity', output.GetStderr())


class CrOSTesterTast(CrOSTesterBase):
  """Tests tast test cases."""

  def testSingleBaseTastTest(self):
    """Verify running a single tast test."""
    self._tester.tast = ['ui.ChromeLogin']
    self._tester.Run()
    self.assertCommandContains(['tast', 'run', '-build=false',
                                '-waituntilready', '-extrauseflags=tast_vm',
                                'localhost:9222', 'ui.ChromeLogin'])

  def testExpressionBaseTastTest(self):
    """Verify running a set of tast tests with an expression."""
    self._tester.tast = [
        '(("dep:chrome" || "dep:android") && !flaky && !disabled)'
    ]
    self._tester.Run()
    self.assertCommandContains([
        'tast', 'run', '-build=false', '-waituntilready',
        '-extrauseflags=tast_vm', 'localhost:9222',
        '(("dep:chrome" || "dep:android") && !flaky && !disabled)'
    ])

  @mock.patch('chromite.lib.cros_build_lib.IsInsideChroot')
  def testTastTestWithOtherArgs(self, check_inside_chroot_mock):
    """Verify running a single tast test with various arguments."""
    self._tester.tast = ['ui.ChromeLogin']
    self._tester.test_timeout = 100
    self._tester._device.log_level = 'debug'
    self._tester._device.ssh_port = None
    self._tester._device.device = '100.90.29.199'
    self._tester.results_dir = '/tmp/results'
    self._tester.Run()
    check_inside_chroot_mock.assert_called()
    self.assertCommandContains(['tast', '-verbose', 'run', '-build=false',
                                '-waituntilready', '-timeout=100',
                                '-resultsdir', '/tmp/results', '100.90.29.199',
                                'ui.ChromeLogin'])

  def testTastTestSDK(self):
    """Verify running tast tests from the SimpleChrome SDK."""
    self._tester.tast = ['ui.ChromeLogin']
    self._tester._device.private_key = '/tmp/.ssh/testing_rsa'
    tast_cache_dir = cros_test_lib.FakeSDKCache(
        self._tester.cache_dir).CreateCacheReference(
            self._tester._device.board, 'chromeos-base')
    tast_bin_dir = os.path.join(tast_cache_dir, 'tast-cmd/usr/bin')
    osutils.SafeMakedirs(tast_bin_dir)
    self._tester.Run()
    self.assertCommandContains([
        os.path.join(tast_bin_dir, 'tast'), 'run', '-build=false',
        '-waituntilready', '-remoterunner=%s'
        % os.path.join(tast_bin_dir, 'remote_test_runner'),
        '-remotebundledir=%s' % os.path.join(tast_cache_dir,
                                             'tast-remote-tests-cros/usr',
                                             'libexec/tast/bundles/remote'),
        '-remotedatadir=%s' % os.path.join(tast_cache_dir,
                                           'tast-remote-tests-cros/usr',
                                           'share/tast/data'),
        '-ephemeraldevserver=false', '-keyfile', '/tmp/.ssh/testing_rsa',
        '-extrauseflags=tast_vm', 'localhost:9222', 'ui.ChromeLogin'
    ])


class CrOSTesterChromeTest(CrOSTesterBase):
  """Tests chrome test test cases."""

  def SetUpChromeTest(self, test_exe, test_label, test_args=None):
    """Sets configurations necessary for running a chrome test.

    Args:
      test_exe: The name of the chrome test.
      test_label: The label of the chrome test.
      test_args: A list of arguments of the particular chrome test.
    """
    self._tester.args = [test_exe] + test_args if test_args else [test_exe]
    self._tester.chrome_test = True
    self._tester.build_dir = self.TempFilePath('out_amd64-generic/Release')
    osutils.SafeMakedirs(self._tester.build_dir)
    isolate_map = self.TempFilePath('testing/buildbot/gn_isolate_map.pyl')
    # Add info about the specified chrome test to the isolate map.
    osutils.WriteFile(isolate_map,
                      """{
                        "%s": {
                          "label": "%s",
                          "type": "console_test_launcher",
                        }
                      }""" % (test_exe, test_label), makedirs=True)

    self._tester.build = True
    self._tester.deploy = True

    self._tester.chrome_test_target = test_exe
    self._tester.chrome_test_deploy_target_dir = '/usr/local/chrome_test'

    # test_label looks like //crypto:crypto_unittests.
    # label_root extracts 'crypto' from the test_label in this instance.
    label_root = test_label.split(':')[0].lstrip('/')
    # A few files used by the chrome test.
    runtime_deps = [
        './%s' % test_exe,
        'gen.runtime/%s/%s/%s.runtime_deps' % (label_root, test_exe, test_exe),
        '../../third_party/chromite']
    # Creates the test_exe to be an executable.
    osutils.Touch(os.path.join(self._tester.build_dir, runtime_deps[0]),
                  mode=0o700)
    for dep in runtime_deps[1:]:
      osutils.Touch(os.path.join(self._tester.build_dir, dep), makedirs=True)
    # Mocks the output by providing necessary runtime files.
    self.rc.AddCmdResult(
        partial_mock.InOrder(['gn', 'desc', test_label]),
        output='\n'.join(runtime_deps))

  def CheckChromeTestCommands(self, test_exe, test_label, build_dir,
                              test_args=None):
    """Checks to see that chrome test commands ran properly.

    Args:
      test_exe: The name of the chrome test.
      test_label: The label of the chrome test.
      build_dir: The directory where chrome is built.
      test_args: Chrome test arguments.
    """
    # Ensure chrome is being built.
    self.assertCommandContains(['autoninja', '-C', build_dir, test_exe])
    # Ensure that the runtime dependencies are checked for.
    self.assertCommandContains(['gn', 'desc', build_dir, test_label,
                                'runtime_deps'])
    # Ensure UI is stopped so the test can grab the GPU if needed.
    self.assertCommandContains(['ssh', '-p', '9222', 'root@localhost', '--',
                                'stop ui'])
    # Ensure a user activity ping is sent to the device.
    self.assertCommandContains(['ssh', '-p', '9222', 'root@localhost', '--',
                                'dbus-send', '--system', '--type=method_call',
                                '--dest=org.chromium.PowerManager',
                                '/org/chromium/PowerManager',
                                'org.chromium.PowerManager.HandleUserActivity',
                                'int32:0'])
    args = ' '.join(test_args) if test_args else ''
    # Ensure the chrome test is run.
    self.assertCommandContains(['ssh', '-p', '9222', 'root@localhost', '--',
                                'cd /usr/local/chrome_test && su chronos -c -- '
                                '"out_amd64-generic/Release/%s %s"'
                                % (test_exe, args)])

  def testChromeTestRsync(self):
    """Verify build/deploy and chrome test commands using rsync to copy."""
    test_exe = 'crypto_unittests'
    test_label = '//crypto:' + test_exe
    self.SetUpChromeTest(test_exe, test_label)
    self._tester.Run()
    self.CheckChromeTestCommands(test_exe, test_label, self._tester.build_dir)

    # Ensure files are being copied over to the device using rsync.
    self.assertCommandContains(['rsync', '%s/' % self._tester.staging_dir,
                                '[root@localhost]:/usr/local/chrome_test'])

  @mock.patch('chromite.lib.remote_access.RemoteDevice.HasRsync',
              return_value=False)
  def testChromeTestSCP(self, rsync_mock):
    """Verify build/deploy and chrome test commands using scp to copy."""
    test_exe = 'crypto_unittests'
    test_label = '//crypto:' + test_exe
    self.SetUpChromeTest(test_exe, test_label)
    self._tester.Run()
    self.CheckChromeTestCommands(test_exe, test_label, self._tester.build_dir)

    # Ensure files are being copied over to the device using scp.
    self.assertCommandContains(['scp', '%s/' % self._tester.staging_dir,
                                'root@localhost:/usr/local/chrome_test'])
    rsync_mock.assert_called()

  def testChromeTestExeArg(self):
    """Verify build/deploy and chrome test commands when a test arg is given."""
    test_exe = 'crypto_unittests'
    test_label = '//crypto:' + test_exe
    test_args = ['--test-launcher-print-test-stdio=auto']
    self.SetUpChromeTest(test_exe, test_label, test_args)
    self._tester.Run()
    self.CheckChromeTestCommands(test_exe, test_label, self._tester.build_dir,
                                 test_args)


class CrOSTesterParser(CrOSTesterBase):
  """Tests parser test cases."""

  def CheckParserError(self, args, error_msg):
    """Checks that parser error is raised.

    Args:
      args: List of commandline arguments.
      error_msg: Error message to check for.
    """
    # Recreate args as a list if it is given as a string.
    if isinstance(args, str):
      args = [args]
    # Putting outcap.OutputCapturer() before assertRaises(SystemExit)
    # swallows SystemExit exception check.
    with self.assertRaises(SystemExit):
      with outcap.OutputCapturer() as output:
        cros_test.ParseCommandLine(args)
    self.assertIn(error_msg, output.GetStderr())

  def testParserErrorChromeTest(self):
    """Verify we get a parser error for --chrome-test when no args are given."""
    self.CheckParserError('--chrome-test', '--chrome-test')

  def testParserSetsBuildDir(self):
    """Verify that the build directory is set when not specified."""
    test_dir = self.TempFilePath('out_amd64-generic/Release/crypto_unittests')
    # Retrieves the build directory from the parsed options.
    build_dir = cros_test.ParseCommandLine(
        ['--chrome-test', '--', test_dir]).build_dir
    self.assertEqual(build_dir, os.path.dirname(test_dir))

  def testParserErrorBuild(self):
    """Verify parser errors for building/deploying Chrome."""
    # Parser error if no build directory is specified.
    self.CheckParserError('--build', '--build-dir')
    # Parser error if build directory is not an existing directory.
    self.CheckParserError(['--deploy', '--build-dir', '/not/a/directory'],
                          'not a directory')

  def testParserErrorResultsSrc(self):
    """Verify parser errors for results src/dest directories."""
    # Parser error if --results-src is not absolute.
    self.CheckParserError(['--results-src', 'tmp/results'], 'absolute')
    # Parser error if no results destination dir is given.
    self.CheckParserError(['--results-src', '/tmp/results'], 'with results-src')
    # Parser error if no results source is given.
    self.CheckParserError(['--results-dest-dir', '/tmp/dest_dir'],
                          'with results-dest-dir')
    # Parser error if results destination dir is a file.
    filename = '/tmp/dest_dir_file'
    osutils.Touch(filename)
    self.CheckParserError(['--results-src', '/tmp/results',
                           '--results-dest-dir', filename], 'existing file')

  def testParserErrorCommands(self):
    """Verify we get parser errors when using certain commands."""
    # Parser error if no test command is provided.
    self.CheckParserError('--remote-cmd', 'specify test command')
    # Parser error if using chronos without a test command.
    self.CheckParserError('--as-chronos', 'as-chronos')
    # Parser error if there are args, but no command.
    self.CheckParserError('--some_test some_command',
                          '--remote-cmd or --host-cmd or --chrome-test')
    # Parser error when additional args don't start with --.
    self.CheckParserError(['--host-cmd', 'tast', 'run'], 'must start with')

  def testParserErrorCWD(self):
    """Verify we get parser errors when specifying the cwd."""
    # Parser error if the cwd refers to a parent path.
    self.CheckParserError(['--cwd', '../new_cwd'], 'cwd cannot start with ..')

    # Parser error if the cwd is not an absolute path.
    self.CheckParserError(['--cwd', 'tmp/cwd'], 'cwd must be an absolute path')

  def testParserErrorFiles(self):
    """Verify we get parser errors with --files."""
    # Parser error when both --files and --files-from are specified.
    self.CheckParserError(['--files', 'file_list', '--files-from', 'file'],
                          '--files and --files-from')

    # Parser error when --files-from does not exist.
    self.CheckParserError(['--files-from', '/fake/file'], 'is not a file')

    # Parser error when a file in --files has an absolute path.
    self.CheckParserError(['--files', '/etc/lsb-release'],
                          'should be a relative path')

    # Parser error when a file has a bad path.
    self.CheckParserError(['--files', '../some_file'], 'cannot start with ..')

    # Parser error when a non-existent file is passed to --files.
    self.CheckParserError(['--files', 'fake/file'], 'does not exist')
