# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for commands."""

from __future__ import print_function

import base64
import collections
import datetime as dt
import json
import hashlib
import os
import struct
import subprocess
import sys

import mock

from chromite.cbuildbot import commands
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import failures_lib
from chromite.cbuildbot import swarming_lib
from chromite.cbuildbot import topology
from chromite.lib import chroot_lib
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import gob_util
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.lib import path_util
from chromite.lib import portage_util
from chromite.lib import sysroot_lib
from chromite.scripts import pushimage

from chromite.service import artifacts as artifacts_service


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class RunBuildScriptTest(cros_test_lib.RunCommandTempDirTestCase):
  """Test RunBuildScript in a variety of cases."""

  def _assertRunBuildScript(self, in_chroot=False, error=None, raises=None,
                            **kwargs):
    """Test the RunBuildScript function.

    Args:
      in_chroot: Whether to enter the chroot or not.
      error: error result message to simulate.
      raises: If the command should fail, the exception to be raised.
      kwargs: Extra kwargs passed to RunBuildScript.
    """

    # Write specified error message to status file.
    def WriteError(_cmd, extra_env=None, **_kwargs):
      if extra_env is not None and error is not None:
        status_file = extra_env[constants.PARALLEL_EMERGE_STATUS_FILE_ENVVAR]
        osutils.WriteFile(status_file, error)

    buildroot = self.tempdir
    osutils.SafeMakedirs(os.path.join(buildroot, '.repo'))
    if error is not None:
      osutils.SafeMakedirs(os.path.join(buildroot, 'chroot', 'tmp'))

    # Run the command, throwing an exception if it fails.
    cmd = ['example', 'command']
    sudo_cmd = ['sudo', '--'] + cmd
    returncode = 1 if raises else 0
    self.rc.AddCmdResult(cmd, returncode=returncode,
                         side_effect=WriteError)
    self.rc.AddCmdResult(sudo_cmd, returncode=returncode,
                         side_effect=WriteError)

    self.PatchObject(path_util, 'ToChrootPath', side_effect=lambda x: x)

    with cros_test_lib.LoggingCapturer():
      # If the script failed, the exception should be raised and printed.
      if raises:
        self.assertRaises(raises, commands.RunBuildScript, buildroot,
                          cmd, enter_chroot=in_chroot, **kwargs)
      else:
        commands.RunBuildScript(buildroot, cmd, enter_chroot=in_chroot,
                                **kwargs)

  def testSuccessOutsideChroot(self):
    """Test executing a command outside the chroot."""
    self._assertRunBuildScript()

  def testSuccessInsideChrootWithoutTempdir(self):
    """Test executing a command inside a chroot without a tmp dir."""
    self._assertRunBuildScript(in_chroot=True)

  def testSuccessInsideChrootWithTempdir(self):
    """Test executing a command inside a chroot with a tmp dir."""
    self._assertRunBuildScript(in_chroot=True, error='')

  def testFailureOutsideChroot(self):
    """Test a command failure outside the chroot."""
    self._assertRunBuildScript(raises=failures_lib.BuildScriptFailure)

  def testFailureInsideChrootWithoutTempdir(self):
    """Test a command failure inside the chroot without a temp directory."""
    self._assertRunBuildScript(in_chroot=True,
                               raises=failures_lib.BuildScriptFailure)

  def testFailureInsideChrootWithTempdir(self):
    """Test a command failure inside the chroot with a temp directory."""
    self._assertRunBuildScript(in_chroot=True, error='',
                               raises=failures_lib.BuildScriptFailure)

  def testPackageBuildFailure(self):
    """Test detecting a package build failure."""
    self._assertRunBuildScript(in_chroot=True, error=constants.CHROME_CP,
                               raises=failures_lib.PackageBuildFailure)

  def testSuccessWithSudo(self):
    """Test a command run with sudo."""
    self._assertRunBuildScript(in_chroot=False, sudo=True)
    self._assertRunBuildScript(in_chroot=True, sudo=True)


class ChromeSDKTest(cros_test_lib.RunCommandTempDirTestCase):
  """Basic tests for ChromeSDK commands with run mocked out."""
  BOARD = 'daisy_foo'
  EXTRA_ARGS = ('--monkey', 'banana')
  EXTRA_ARGS2 = ('--donkey', 'kong')
  CHROME_SRC = 'chrome_src'
  CMD = ['bar', 'baz']
  CWD = 'fooey'

  def setUp(self):
    self.inst = commands.ChromeSDK(self.CWD, self.BOARD)

  def testRunCommand(self):
    """Test that running a command is possible."""
    self.inst.Run(self.CMD)
    self.assertCommandContains([self.BOARD] + self.CMD, cwd=self.CWD)

  def testRunCommandWithRunArgs(self):
    """Test run_args optional argument for run kwargs."""
    self.inst.Run(self.CMD, run_args={'log_output': True})
    self.assertCommandContains([self.BOARD] + self.CMD, cwd=self.CWD,
                               log_output=True)

  def testRunCommandKwargs(self):
    """Exercise optional arguments."""
    custom_inst = commands.ChromeSDK(
        self.CWD, self.BOARD, extra_args=list(self.EXTRA_ARGS),
        chrome_src=self.CHROME_SRC, debug_log=True)
    custom_inst.Run(self.CMD, list(self.EXTRA_ARGS2))
    self.assertCommandContains(['debug', self.BOARD] + list(self.EXTRA_ARGS) +
                               list(self.EXTRA_ARGS2) + self.CMD, cwd=self.CWD)

  def MockGetDefaultTarget(self):
    self.rc.AddCmdResult(partial_mock.In('qlist-%s' % self.BOARD),
                         output='%s' % constants.CHROME_CP)

  def testNinjaWithRunArgs(self):
    """Test that running ninja with run_args.

    run_args is an optional argument for run kwargs.
    """
    self.MockGetDefaultTarget()
    self.inst.Ninja(run_args={'log_output': True})
    self.assertCommandContains(
        ['autoninja', '-C', 'out_%s/Release' % self.BOARD,
         'chromiumos_preflight'], cwd=self.CWD, log_output=True,
    )

  def testNinjaOptions(self):
    """Test that running ninja with non-default options."""
    self.MockGetDefaultTarget()
    custom_inst = commands.ChromeSDK(self.CWD, self.BOARD, goma=True)
    custom_inst.Ninja(debug=True)
    self.assertCommandContains(['autoninja', '-j', '80', '-C',
                                'out_%s/Debug' % self.BOARD,
                                'chromiumos_preflight'])


class SkylabHWLabCommandsTest(cros_test_lib.RunCommandTestCase):
  """Test commands that launch Skylab suites."""

  _SKYLAB_TOOL = '/opt/skylab'

  def setUp(self):
    self.PatchObject(commands, '_InstallSkylabTool',
                     return_value=self._SKYLAB_TOOL)

  @staticmethod
  def _fakeWaitJson(state, failure, stdout=''):
    """Return a fake json serialized suite report, in wait-task format."""
    report = {
        'task-result': {
            'state': state,
            'success': not failure,
        },
        'stdout': stdout,
    }
    return json.dumps(report)

  @staticmethod
  def _fakeCreateJson(task_id, task_url):
    """Return a fake json serialized suite report, in create-suite format."""
    report = {
        'task_id': task_id,
        'task_url': task_url,
    }
    return json.dumps(report)

  def testCreateSuiteNoPoolRaisesError(self):
    """Attempting to create a suite without specifying a pool should raise."""
    build = 'foo-bar/R1234'
    suite = 'foo-suite'
    board = 'foo-board'
    with self.assertRaises(ValueError):
      commands.RunSkylabHWTestSuite(build, suite, board)

  def testCreateSuite(self):
    """Test that function call args are mapped correctly to commandline args."""
    build = 'foo-bar/R1234'
    suite = 'foo-suite'
    board = 'foo-board'
    pool = 'foo-pool'
    # An OrderedDict is used to make the keyval order on the command line
    # deterministic for testing purposes.
    keyvals = collections.OrderedDict([('k1', 'v1'), ('k2', 'v2')])
    priority = constants.HWTEST_DEFAULT_PRIORITY
    quota_account = 'foo-account'
    max_retries = 10
    timeout_mins = 10

    task_id = 'foo-task_id'

    create_cmd = [
        self._SKYLAB_TOOL, 'create-suite',
        '-image', build,
        '-board', board,
        '-pool', pool,
        '-priority', '140',
        '-timeout-mins', str(timeout_mins),
        '-max-retries', str(max_retries),
        '-keyval', 'k1:v1',
        '-keyval', 'k2:v2',
        '-qs-account', quota_account,
        '-json',
        '-service-account-json', constants.CHROMEOS_SERVICE_ACCOUNT,
        suite
    ]

    self.rc.AddCmdResult(
        create_cmd, output=self._fakeCreateJson(task_id, 'foo://foo'))

    result = commands.RunSkylabHWTestSuite(
        build, suite, board, pool=pool, job_keyvals=keyvals, priority=priority,
        quota_account=quota_account, max_retries=max_retries,
        timeout_mins=timeout_mins)

    self.assertTrue(isinstance(result, commands.HWTestSuiteResult))
    self.assertEqual(result.to_raise, None)
    self.assertEqual(result.json_dump_result, None)

    self.rc.assertCommandCalled(create_cmd, stdout=True)
    self.assertEqual(self.rc.call_count, 1)

  def testCreateSuiteAndWait(self):
    """Test that suite can be created and then waited for."""
    build = 'foo-bar/R1234'
    suite = 'foo-suite'
    board = 'foo-board'
    pool = 'foo-pool'

    task_id = 'foo-task_id'

    create_cmd = [
        self._SKYLAB_TOOL, 'create-suite',
        '-image', build,
        '-board', board,
        '-pool', pool,
        '-json',
        '-service-account-json', constants.CHROMEOS_SERVICE_ACCOUNT,
        suite,
    ]
    wait_cmd = [
        self._SKYLAB_TOOL, 'wait-task',
        '-service-account-json', constants.CHROMEOS_SERVICE_ACCOUNT,
        task_id
    ]
    self.rc.AddCmdResult(
        create_cmd, output=self._fakeCreateJson(task_id, 'foo://foo'))
    self.rc.AddCmdResult(
        wait_cmd, output=self._fakeWaitJson('COMPLETED', False))

    result = commands.RunSkylabHWTestSuite(
        build, suite, board, pool=pool, timeout_mins=None,
        wait_for_results=True)
    self.assertEqual(result.to_raise, None)
    self.assertEqual(result.json_dump_result, None)

    self.rc.assertCommandCalled(create_cmd, stdout=True)
    self.rc.assertCommandCalled(wait_cmd, stdout=True)
    self.assertEqual(self.rc.call_count, 2)

  def testSuiteFailure(self):
    """Test that nonzero return code is reported as suite failure."""
    build = 'foo-bar/R1234'
    suite = 'foo-suite'
    board = 'foo-board'
    pool = 'foo-pool'

    task_id = 'foo-task_id'

    create_cmd = [
        self._SKYLAB_TOOL, 'create-suite',
        '-image', build,
        '-board', board,
        '-pool', pool,
        '-json',
        '-service-account-json', constants.CHROMEOS_SERVICE_ACCOUNT,
        suite,
    ]
    wait_cmd = [
        self._SKYLAB_TOOL, 'wait-task',
        '-service-account-json', constants.CHROMEOS_SERVICE_ACCOUNT,
        task_id,
    ]
    self.rc.AddCmdResult(
        create_cmd, output=self._fakeCreateJson(task_id, 'foo://foo'))
    self.rc.AddCmdResult(wait_cmd, output=self._fakeWaitJson('COMPLETED', True))

    result = commands.RunSkylabHWTestSuite(
        build, suite, board, pool=pool, timeout_mins=None,
        wait_for_results=True)
    error = result.to_raise
    self.assertTrue(isinstance(error, failures_lib.TestFailure))
    self.assertIn('Suite failed', str(error))

  def testCreateTest(self):
    """Test that function call args are mapped correctly to commandline args."""
    test_plan = '{}'
    build = 'foo-bar/R1234'
    board = 'foo-board'
    model = 'foo-model'
    pool = 'foo-pool'
    suite = 'foo-suite'
    # An OrderedDict is used to make the keyval order on the command line
    # deterministic for testing purposes.
    keyvals = {'key': 'value'}
    timeout_mins = 10

    task_id = 'foo-task_id'

    create_cmd = [
        self._SKYLAB_TOOL, 'create-testplan',
        '-image', build,
        '-legacy-suite', suite,
        '-pool', pool,
        '-board', board,
        '-model', model,
        '-timeout-mins', str(timeout_mins),
        '-keyval', 'key:value',
        '-service-account-json', constants.CHROMEOS_SERVICE_ACCOUNT,
        '-plan-file', '/dev/stdin'
    ]

    self.rc.AddCmdResult(
        create_cmd, output=self._fakeCreateJson(task_id, 'foo://foo'))

    result = commands.RunSkylabHWTestPlan(
        test_plan=test_plan, build=build, pool=pool, board=board, model=model,
        timeout_mins=timeout_mins, keyvals=keyvals, legacy_suite=suite)
    self.assertTrue(isinstance(result, commands.HWTestSuiteResult))
    self.assertEqual(result.to_raise, None)
    self.assertEqual(result.json_dump_result, None)

    self.rc.assertCommandCalled(
        create_cmd, stdout=True, input=test_plan)
    self.assertEqual(self.rc.call_count, 1)



class HWLabCommandsTest(cros_test_lib.RunCommandTestCase,
                        cros_test_lib.OutputTestCase,
                        cros_test_lib.MockTempDirTestCase):
  """Test commands related to HWLab tests that are runing via swarming proxy."""

  # pylint: disable=protected-access
  JOB_ID_OUTPUT = """
Autotest instance: cautotest
02-23-2015 [06:26:51] Submitted create_suite_job rpc
02-23-2015 [06:26:53] Created suite job: http://cautotest.corp.google.com/afe/#tab_id=view_job&object_id=26960110
Created task id: 26960110
@@@STEP_LINK@Suite created@http://cautotest.corp.google.com/afe/#tab_id=view_job&object_id=26960110@@@
"""

  WAIT_RETRY_OUTPUT = """
ERROR: Encountered swarming internal error
"""

  WAIT_OUTPUT = """
The suite job has another 3:09:50.012887 till timeout.
The suite job has another 2:39:39.789250 till timeout.
"""
  JSON_DICT = """
{"tests": {"test_1":{"status":"GOOD", "attributes": ["suite:test-suite"]},
           "test_2":{"status":"other", "attributes": ["suite:test-suite"]}
}}
"""
  JSON_OUTPUT = ('%s%s%s' % (commands.JSON_DICT_START, JSON_DICT,
                             commands.JSON_DICT_END))
  SWARMING_TIMEOUT_DEFAULT = str(
      commands._DEFAULT_HWTEST_TIMEOUT_MINS * 60 +
      commands._SWARMING_ADDITIONAL_TIMEOUT)
  SWARMING_EXPIRATION = str(commands._SWARMING_EXPIRATION)


  def setUp(self):
    self._build = 'test-build'
    self._board = 'test-board'
    self._model = 'test-model'
    self._suite = 'test-suite'
    self._pool = 'test-pool'
    self._num = 42
    self._file_bugs = True
    self._wait_for_results = True
    self._priority = 'test-priority'
    self._timeout_mins = 2880
    self._max_runtime_mins = None
    self._retry = False
    self._max_retries = 3
    self._minimum_duts = 2
    self._suite_min_duts = 2
    self.create_cmd = None
    self.wait_cmd = None
    self.json_dump_cmd = None
    self.temp_json_path = os.path.join(self.tempdir, 'temp_summary.json')
    # Bot died
    self.retriable_swarming_code = 80
    self.internal_failure_exit_code = 1
    # A random code that's not retriable.
    self.swarming_code = 10
    topology.FetchTopology()

  def RunHWTestSuite(self, *args, **kwargs):
    """Run the hardware test suite, printing logs to stdout."""
    kwargs.setdefault('debug', False)
    with cros_test_lib.LoggingCapturer() as logs:
      try:
        cmd_result = commands.RunHWTestSuite(self._build, self._suite,
                                             self._board, *args, **kwargs)
        return cmd_result
      finally:
        print(logs.messages)

  def SetCmdResults(self, create_return_code=0, wait_return_code=0,
                    dump_json_return_code=0, wait_retry=False, args=(),
                    swarming_timeout_secs=SWARMING_TIMEOUT_DEFAULT,
                    swarming_io_timeout_secs=SWARMING_TIMEOUT_DEFAULT,
                    swarming_hard_timeout_secs=SWARMING_TIMEOUT_DEFAULT,
                    swarming_expiration_secs=SWARMING_EXPIRATION):
    """Set the expected results from the specified commands.

    Args:
      create_return_code: Return code from create command.
      wait_return_code: Return code from wait command.
      dump_json_return_code: Return code from json_dump command.
      wait_retry: Boolean, if wait command should be retried.
      args: Additional args to pass to create and wait commands.
      swarming_timeout_secs: swarming client timeout.
      swarming_io_timeout_secs: swarming client io timeout.
      swarming_hard_timeout_secs: swarming client hard timeout.
      swarming_expiration_secs: swarming task expiration.
    """
    # Pull out the test priority for the swarming tag.
    priority = None
    priority_flag = '--priority'
    if priority_flag in args:
      priority = args[args.index(priority_flag) + 1]

    base_cmd = [swarming_lib._SWARMING_PROXY_CLIENT, 'run',
                '--swarming',
                topology.topology.get(topology.SWARMING_PROXY_HOST_KEY),
                '--task-summary-json', self.temp_json_path,
                '--print-status-updates',
                '--timeout', swarming_timeout_secs,
                '--raw-cmd',
                '--task-name', 'test-build-test-suite',
                '--dimension', 'os', 'Ubuntu-14.04',
                '--dimension', 'pool', commands.SUITE_BOT_POOL,
                '--io-timeout', swarming_io_timeout_secs,
                '--hard-timeout', swarming_hard_timeout_secs,
                '--expiration', swarming_expiration_secs,
                '--tags=board:test-board',
                '--tags=build:test-build',
                '--tags=priority:%s' % priority,
                '--tags=suite:test-suite',
                '--tags=task_name:test-build-test-suite',
                '--auth-service-account-json',
                constants.CHROMEOS_SERVICE_ACCOUNT,
                '--', commands.RUN_SUITE_PATH,
                '--build', 'test-build', '--board', 'test-board']
    args = list(args)
    base_cmd = base_cmd + ['--suite_name', 'test-suite'] + args

    self.create_cmd = base_cmd + ['-c']
    self.wait_cmd = base_cmd + ['-m', '26960110']
    self.json_dump_cmd = base_cmd + ['--json_dump', '-m', '26960110']
    create_results = iter([
        cros_build_lib.CommandResult(returncode=create_return_code,
                                     output=self.JOB_ID_OUTPUT,
                                     error=''),
    ])
    self.rc.AddCmdResult(
        self.create_cmd,
        side_effect=lambda *args, **kwargs: next(create_results),
    )
    wait_results_list = []
    if wait_retry:
      r = cros_build_lib.CommandResult(
          returncode=self.internal_failure_exit_code,
          output=self.WAIT_RETRY_OUTPUT,
          error='')
      wait_results_list.append(r)

    wait_results_list.append(
        cros_build_lib.CommandResult(
            returncode=wait_return_code, output=self.WAIT_OUTPUT,
            error='')
    )
    wait_results = iter(wait_results_list)

    self.rc.AddCmdResult(
        self.wait_cmd,
        side_effect=lambda *args, **kwargs: next(wait_results),
    )

    # Json dump will only run when wait_cmd fails
    if wait_return_code != 0:
      dump_json_results = iter([
          cros_build_lib.CommandResult(returncode=dump_json_return_code,
                                       output=self.JSON_OUTPUT,
                                       error=''),
      ])
      self.rc.AddCmdResult(
          self.json_dump_cmd,
          side_effect=lambda *args, **kwargs: next(dump_json_results),
      )

  def PatchJson(self, task_outputs):
    """Mock out the code that loads from json.

    Args:
      task_outputs: A list of tuple, the first element is the value of 'outputs'
                    field in the json dictionary, the second is a boolean
                    indicating whether there is an internal failure,
                    the third is a state code for the internal failure.
                    e.g.
                    ('some output', True, 80)
                    ('some output', False, None)
    """
    orig_func = commands._CreateSwarmingArgs

    def replacement(*args, **kargs):
      swarming_args = orig_func(*args, **kargs)
      swarming_args['temp_json_path'] = self.temp_json_path
      return swarming_args

    self.PatchObject(commands, '_CreateSwarmingArgs', side_effect=replacement)

    if task_outputs:
      return_values = []
      for s in task_outputs:
        j = {'shards': [{'name': 'fake_name', 'bot_id': 'chromeos-server990',
                         'created_ts': '2015-06-12 12:00:00',
                         'internal_failure': s[1],
                         'state': s[2],
                         'outputs': [s[0]]}]}
        return_values.append(j)
      return_values_iter = iter(return_values)
      self.PatchObject(swarming_lib.SwarmingCommandResult, 'LoadJsonSummary',
                       side_effect=lambda json_file: next(return_values_iter))
    else:
      self.PatchObject(swarming_lib.SwarmingCommandResult, 'LoadJsonSummary',
                       return_value=None)

  def testRunHWTestSuiteMinimal(self):
    """Test RunHWTestSuite without optional arguments."""
    self.SetCmdResults()
    # When run without optional arguments, wait and dump_json cmd will not run.
    self.PatchJson([(self.JOB_ID_OUTPUT, False, None)])

    with self.OutputCapturer() as output:
      cmd_result = self.RunHWTestSuite()
    self.assertEqual(cmd_result, (None, None))
    self.assertCommandCalled(self.create_cmd, capture_output=True,
                             encoding='utf-8', stderr=subprocess.STDOUT,
                             env=mock.ANY)
    self.assertIn(self.JOB_ID_OUTPUT, '\n'.join(output.GetStdoutLines()))

  def testRunHWTestSuiteMaximal(self):
    """Test RunHWTestSuite with all arguments."""
    swarming_timeout = str(self._timeout_mins * 60 +
                           commands._SWARMING_ADDITIONAL_TIMEOUT)
    self.SetCmdResults(
        args=[
            '--pool', 'test-pool',
            '--file_bugs', 'True',
            '--priority', 'test-priority', '--timeout_mins', '2880',
            '--max_runtime_mins', '2880',
            '--retry', 'False', '--max_retries', '3', '--minimum_duts', '2',
            '--suite_min_duts', '2'
        ],
        swarming_timeout_secs=swarming_timeout,
        swarming_io_timeout_secs=swarming_timeout,
        swarming_hard_timeout_secs=swarming_timeout)

    self.PatchJson([(self.JOB_ID_OUTPUT, False, None),
                    (self.WAIT_OUTPUT, False, None)])
    with self.OutputCapturer() as output:
      cmd_result = self.RunHWTestSuite(pool=self._pool,
                                       file_bugs=self._file_bugs,
                                       wait_for_results=self._wait_for_results,
                                       priority=self._priority,
                                       timeout_mins=self._timeout_mins,
                                       max_runtime_mins=self._max_runtime_mins,
                                       retry=self._retry,
                                       max_retries=self._max_retries,
                                       minimum_duts=self._minimum_duts,
                                       suite_min_duts=self._suite_min_duts)
    self.assertEqual(cmd_result, (None, None))
    self.assertCommandCalled(self.create_cmd, capture_output=True,
                             stderr=subprocess.STDOUT, env=mock.ANY,
                             encoding='utf-8')
    self.assertCommandCalled(self.wait_cmd, capture_output=True,
                             stderr=subprocess.STDOUT, env=mock.ANY,
                             encoding='utf-8')
    self.assertIn(self.WAIT_OUTPUT, '\n'.join(output.GetStdoutLines()))
    self.assertIn(self.JOB_ID_OUTPUT, '\n'.join(output.GetStdoutLines()))

  def testRunHWTestSuiteFailure(self):
    """Test RunHWTestSuite when ERROR is returned."""
    self.PatchJson([(self.JOB_ID_OUTPUT, False, None)])
    self.rc.SetDefaultCmdResult(returncode=1, output=self.JOB_ID_OUTPUT)
    with self.OutputCapturer():
      cmd_result = self.RunHWTestSuite()
      self.assertIsInstance(cmd_result.to_raise, failures_lib.TestFailure)

  def testRunHWTestSuiteTimedOut(self):
    """Test RunHWTestSuite when SUITE_TIMEOUT is returned."""
    self.PatchJson([(self.JOB_ID_OUTPUT, False, None)])
    self.rc.SetDefaultCmdResult(returncode=4, output=self.JOB_ID_OUTPUT)
    with self.OutputCapturer():
      cmd_result = self.RunHWTestSuite()
      self.assertIsInstance(cmd_result.to_raise, failures_lib.SuiteTimedOut)

  def testRunHWTestSuiteInfraFail(self):
    """Test RunHWTestSuite when INFRA_FAILURE is returned."""
    self.PatchJson([(self.JOB_ID_OUTPUT, False, None)])
    self.rc.SetDefaultCmdResult(returncode=3, output=self.JOB_ID_OUTPUT)
    with self.OutputCapturer():
      cmd_result = self.RunHWTestSuite()
      self.assertIsInstance(cmd_result.to_raise, failures_lib.TestLabFailure)

  def testRunHWTestSuiteCommandErrorJSONDumpMissing(self):
    """Test RunHWTestSuite when the JSON output is missing on error."""
    self.SetCmdResults()
    self.PatchJson([
        (self.JOB_ID_OUTPUT, False, None),
        ('', False, None),
        ('', False, None),
    ])

    def fail_swarming_cmd(cmd, *_args, **_kwargs):
      result = swarming_lib.SwarmingCommandResult(None, cmd=cmd,
                                                  error='injected error',
                                                  output='', returncode=3)
      raise cros_build_lib.RunCommandError('injected swarming failure',
                                           result, None)

    self.rc.AddCmdResult(self.wait_cmd, side_effect=fail_swarming_cmd)
    self.rc.AddCmdResult(self.json_dump_cmd, side_effect=fail_swarming_cmd)

    with self.OutputCapturer():
      cmd_result = self.RunHWTestSuite(wait_for_results=True)
      self.assertIsInstance(cmd_result.to_raise, failures_lib.TestLabFailure)
      self.assertCommandCalled(self.json_dump_cmd, capture_output=True,
                               stderr=subprocess.STDOUT, env=mock.ANY,
                               encoding='utf-8')

  def testRunHWTestBoardNotAvailable(self):
    """Test RunHWTestSuite when BOARD_NOT_AVAILABLE is returned."""
    self.PatchJson([(self.JOB_ID_OUTPUT, False, None)])
    self.rc.SetDefaultCmdResult(returncode=5, output=self.JOB_ID_OUTPUT)
    with self.OutputCapturer():
      cmd_result = self.RunHWTestSuite()
      self.assertIsInstance(cmd_result.to_raise, failures_lib.BoardNotAvailable)

  def testRunHWTestTestWarning(self):
    """Test RunHWTestSuite when WARNING is returned."""
    self.PatchJson([(self.JOB_ID_OUTPUT, False, None)])
    self.rc.SetDefaultCmdResult(returncode=2, output=self.JOB_ID_OUTPUT)
    with self.OutputCapturer():
      cmd_result = self.RunHWTestSuite()
      self.assertIsInstance(cmd_result.to_raise, failures_lib.TestWarning)

  def testRunHWTestTestSwarmingClientNoSummaryFile(self):
    """Test RunHWTestSuite when no summary file is generated."""
    unknown_failure = 'Unknown failure'
    self.PatchJson(task_outputs=[])
    self.rc.SetDefaultCmdResult(returncode=1, output=unknown_failure)
    with self.OutputCapturer() as output:
      cmd_result = self.RunHWTestSuite()
      self.assertIsInstance(cmd_result.to_raise,
                            failures_lib.SwarmingProxyFailure)
      self.assertIn(unknown_failure, '\n'.join(output.GetStdoutLines()))

  def testRunHWTestTestSwarmingClientInternalFailure(self):
    """Test RunHWTestSuite when swarming encounters internal failure."""
    unknown_failure = 'Unknown failure'
    self.PatchJson(
        task_outputs=[(self.JOB_ID_OUTPUT, True, self.swarming_code)])
    self.rc.SetDefaultCmdResult(returncode=1, output=unknown_failure)
    with self.OutputCapturer() as output:
      cmd_result = self.RunHWTestSuite()
      self.assertIsInstance(cmd_result.to_raise,
                            failures_lib.SwarmingProxyFailure)
      self.assertIn(unknown_failure, '\n'.join(output.GetStdoutLines()))
      self.assertIn('summary json content', '\n'.join(output.GetStdoutLines()))

  def testRunHWTestTestSwarmingClientWithRetires(self):
    """Test RunHWTestSuite with retries."""
    self.SetCmdResults(wait_retry=True)
    self.PatchJson([
        (self.JOB_ID_OUTPUT, False, None),
        (self.WAIT_RETRY_OUTPUT, True, self.retriable_swarming_code),
        (self.WAIT_OUTPUT, False, None),
        (self.JSON_OUTPUT, False, None),
    ])
    with self.OutputCapturer() as output:
      self.RunHWTestSuite(wait_for_results=self._wait_for_results)
      self.assertCommandCalled(self.create_cmd, capture_output=True,
                               stderr=subprocess.STDOUT, env=mock.ANY,
                               encoding='utf-8')
      self.assertCommandCalled(self.wait_cmd, capture_output=True,
                               stderr=subprocess.STDOUT, env=mock.ANY,
                               encoding='utf-8')
      self.assertIn(self.WAIT_RETRY_OUTPUT.strip(),
                    '\n'.join(output.GetStdoutLines()))
      self.assertIn(self.WAIT_OUTPUT, '\n'.join(output.GetStdoutLines()))
      self.assertIn(self.JOB_ID_OUTPUT, '\n'.join(output.GetStdoutLines()))

  def testRunHWTestSuiteJsonDumpWhenWaitCmdFail(self):
    """Test RunHWTestSuite run json dump cmd when wait_cmd fail."""
    self.SetCmdResults(wait_return_code=1, wait_retry=True)
    self.PatchJson([
        (self.JOB_ID_OUTPUT, False, None),
        (self.JSON_OUTPUT, False, None),
    ])
    with mock.patch.object(commands, '_HWTestWait', return_value=False):
      with self.OutputCapturer() as output:
        self.RunHWTestSuite(wait_for_results=self._wait_for_results)
        self.assertCommandCalled(self.create_cmd, capture_output=True,
                                 stderr=subprocess.STDOUT, env=mock.ANY,
                                 encoding='utf-8')
        self.assertCommandCalled(self.json_dump_cmd, capture_output=True,
                                 stderr=subprocess.STDOUT, env=mock.ANY,
                                 encoding='utf-8')
        self.assertIn(self.JOB_ID_OUTPUT, '\n'.join(output.GetStdoutLines()))
        self.assertIn(self.JSON_OUTPUT, '\n'.join(output.GetStdoutLines()))


class CBuildBotTest(cros_test_lib.RunCommandTempDirTestCase):
  """Test general cbuildbot command methods."""

  def setUp(self):
    self._board = 'test-board'
    self._buildroot = self.tempdir
    self._chroot = os.path.join(self._buildroot, 'chroot')
    os.makedirs(os.path.join(self._buildroot, '.repo'))

    self._dropfile = os.path.join(
        self._buildroot, 'src', 'scripts', 'cbuildbot_package.list')

    self.target_image = os.path.join(
        self.tempdir,
        'link/R37-5952.0.2014_06_12_2302-a1/chromiumos_test_image.bin')


  def testGenerateStackTraces(self):
    """Test if we can generate stack traces for minidumps."""
    os.makedirs(os.path.join(self._chroot, 'tmp'))
    dump_file = os.path.join(self._chroot, 'tmp', 'test.dmp')
    dump_file_dir, dump_file_name = os.path.split(dump_file)
    ret = [(dump_file_dir, [''], [dump_file_name])]
    with mock.patch('os.walk', return_value=ret):
      test_results_dir = os.path.join(self.tempdir, 'test_results')
      commands.GenerateStackTraces(self._buildroot, self._board,
                                   test_results_dir, self.tempdir, True)
      self.assertCommandContains(['minidump_stackwalk'])

  def testUprevPackagesMin(self):
    """See if we can generate the minimal cros_mark_as_stable commandline."""
    commands.UprevPackages(self._buildroot, [self._board],
                           constants.PUBLIC_OVERLAYS)
    self.assertCommandContains([
        'commit', '--all',
        '--boards=%s' % self._board,
        '--drop_file=%s' % self._dropfile,
        '--buildroot', self._buildroot,
        '--overlay-type', 'public',
    ])

  def testUprevPackagesMax(self):
    """See if we can generate the max cros_mark_as_stable commandline."""
    commands.UprevPackages(self._buildroot, [self._board],
                           overlay_type=constants.PUBLIC_OVERLAYS,
                           workspace='/workspace')
    self.assertCommandContains([
        'commit', '--all',
        '--boards=%s' % self._board,
        '--drop_file=%s' % self._dropfile,
        '--buildroot', '/workspace',
        '--overlay-type', 'public',
    ])

  def testUprevPushMin(self):
    """See if we can generate the minimal cros_mark_as_stable commandline."""
    commands.UprevPush(self._buildroot, overlay_type=constants.PUBLIC_OVERLAYS)
    self.assertCommandContains([
        'push',
        '--buildroot', self._buildroot,
        '--overlay-type', 'public',
        '--dryrun',
    ])

  def testUprevPushMax(self):
    """See if we can generate the max cros_mark_as_stable commandline."""
    commands.UprevPush(self._buildroot,
                       dryrun=False,
                       overlay_type=constants.PUBLIC_OVERLAYS,
                       workspace='/workspace')
    self.assertCommandContains([
        'push',
        '--buildroot', '/workspace',
        '--overlay-type', 'public',
    ])

  def testVerifyBinpkgMissing(self):
    """Test case where binpkg is missing."""
    self.rc.AddCmdResult(
        partial_mock.ListRegex(r'emerge'),
        output='\n[ebuild] %s' % constants.CHROME_CP)
    self.assertRaises(
        commands.MissingBinpkg, commands.VerifyBinpkg,
        self._buildroot, self._board, constants.CHROME_CP, packages=())

  def testVerifyBinpkgPresent(self):
    """Test case where binpkg is present."""
    self.rc.AddCmdResult(
        partial_mock.ListRegex(r'emerge'),
        output='\n[binary] %s' % constants.CHROME_CP)
    commands.VerifyBinpkg(self._buildroot, self._board, constants.CHROME_CP,
                          packages=())

  def testVerifyChromeNotInstalled(self):
    """Test case where Chrome is not installed at all."""
    commands.VerifyBinpkg(self._buildroot, self._board, constants.CHROME_CP,
                          packages=())

  def testBuild(self, default=False, **kwargs):
    """Base case where Build is called with minimal options."""
    kwargs.setdefault('build_autotest', default)
    kwargs.setdefault('usepkg', default)
    kwargs.setdefault('skip_chroot_upgrade', default)

    commands.Build(buildroot=self._buildroot, board='amd64-generic', **kwargs)
    self.assertCommandContains(['./build_packages'])

  def testGetFirmwareVersions(self):
    self.rc.SetDefaultCmdResult(output="""

flashrom(8): a8f99c2e61e7dc09c4b25ef5a76ef692 */build/kevin/usr/sbin/flashrom
             ELF 32-bit LSB executable, ARM, EABI5 version 1 (SYSV), statically linked, for GNU/Linux 2.d
             0.9.4  : 860875a : Apr 10 2017 23:54:29 UTC

BIOS image:   6b5b855a0b8fd1657546d1402c15b206 *chromeos-firmware-kevin-0.0.1/.dist/kevin_fw_8785.178.0.n
BIOS version: Google_Kevin.8785.178.0
EC image:     1ebfa9518e6cac0558a80b7ab2f5b489 *chromeos-firmware-kevin-0.0.1/.dist/kevin_ec_8785.178.0.n
EC version:kevin_v1.10.184-459421c

Package Content:
a8f99c2e61e7dc09c4b25ef5a76ef692 *./flashrom
3c3a99346d1ca1273cbcd86c104851ff *./shflags
457a8dc8546764affc9700f8da328d23 *./dump_fmap
c392980ddb542639edf44a965a59361a *./updater5.sh
490c95d6123c208d20d84d7c16857c7c *./crosfw.sh
6b5b855a0b8fd1657546d1402c15b206 *./bios.bin
7b5bef0d2da90c23ff2e157250edf0fa *./crosutil.sh
d78722e4f1a0dc2d8c3d6b0bc7010ae3 *./crossystem
457a8dc8546764affc9700f8da328d23 *./gbb_utility
1ebfa9518e6cac0558a80b7ab2f5b489 *./ec.bin
c98ca54db130886142ad582a58e90ddc *./common.sh
5ba978bdec0f696f47f0f0de90936880 *./mosys
312e8ee6122057f2a246d7bcf1572f49 *./vpd
""")
    build_sbin = os.path.join(self._buildroot, constants.DEFAULT_CHROOT_DIR,
                              'build', self._board, 'usr', 'sbin')
    osutils.Touch(os.path.join(build_sbin, 'chromeos-firmwareupdate'),
                  makedirs=True)
    result = commands.GetFirmwareVersions(self._buildroot, self._board)
    versions = commands.FirmwareVersions(
        None, 'Google_Kevin.8785.178.0', None, 'kevin_v1.10.184-459421c', None)
    self.assertEqual(result, versions)

  def testGetFirmwareVersionsMixedImage(self):
    """Verify that can extract the right version from a mixed RO+RW bundle."""
    self.rc.SetDefaultCmdResult(output="""

flashrom(8): 29c9ec509aaa9c1f575cca883d90980c */build/caroline/usr/sbin/flashrom
             ELF 64-bit LSB executable, x86-64, version 1 (SYSV), statically linked, for GNU/Linux 2.6.32, BuildID[sha1]=eb6af9bb9e14e380676ad9607760c54addec4a3a, stripped
             0.9.4  : 1bb61e1 : Feb 07 2017 18:29:17 UTC

BIOS image:   9f78f612c24ee7ec4ca4d2747b01d8b9 *chromeos-firmware-caroline-0.0.1/.dist/Caroline.7820.263.0.tbz2/image.bin
BIOS version: Google_Caroline.7820.263.0
BIOS (RW) image:   2cb5021b986fe024f20d242e1885e1e7 *chromeos-firmware-caroline-0.0.1/.dist/Caroline.7820.286.0.tbz2/image.bin
BIOS (RW) version: Google_Caroline.7820.286.0
EC image:     18569de94ea66ba0cad360c3b7d8e205 *chromeos-firmware-caroline-0.0.1/.dist/Caroline_EC.7820.263.0.tbz2/ec.bin
EC version:   caroline_v1.9.357-ac5c7b4
EC (RW) version:   caroline_v1.9.370-e8b9bd2
PD image:     0ba8d6a0fa82c42fa42a98096e2b1480 *chromeos-firmware-caroline-0.0.1/.dist/Caroline_PD.7820.263.0.tbz2/ec.bin
PD version:   caroline_pd_v1.9.357-ac5c7b4
PD (RW) version:   caroline_pd_v1.9.370-e8b9bd2
Extra files from folder: /mnt/host/source/src/private-overlays/overlay-caroline-private/chromeos-base/chromeos-firmware-caroline/files/extra
Extra file: /build/caroline//bin/xxd

Package Content:
dc9b08c5b17a7d51f9acdf5d3e12ebb7 *./updater4.sh
29c9ec509aaa9c1f575cca883d90980c *./flashrom
3c3a99346d1ca1273cbcd86c104851ff *./shflags
d962372228f82700d179d53a509f9735 *./dump_fmap
490c95d6123c208d20d84d7c16857c7c *./crosfw.sh
deb421e949ffaa23102ef3cee640be2d *./bios.bin
b0ca480cb2981b346f493ebc93a52e8a *./crosutil.sh
fba6434300d36f7b013883b6a3d04b57 *./pd.bin
03496184aef3ec6d5954528a5f15d8af *./crossystem
d962372228f82700d179d53a509f9735 *./gbb_utility
6ddd288ce20e28b90ef0b21613637b60 *./ec.bin
7ca17c9b563383296ee9e2c353fdb766 *./updater_custom.sh
c2728ed24809ec845c53398a15255f49 *./xxd
c98ca54db130886142ad582a58e90ddc *./common.sh
a3326e34e8c9f221cc2dcd2489284e30 *./mosys
ae8cf9fca3165a1c1f12decfd910c4fe *./vpd
""")
    build_sbin = os.path.join(self._buildroot, constants.DEFAULT_CHROOT_DIR,
                              'build', self._board, 'usr', 'sbin')
    osutils.Touch(os.path.join(build_sbin, 'chromeos-firmwareupdate'),
                  makedirs=True)
    result = commands.GetFirmwareVersions(self._buildroot, self._board)
    versions = commands.FirmwareVersions(
        None,
        'Google_Caroline.7820.263.0',
        'Google_Caroline.7820.286.0',
        'caroline_v1.9.357-ac5c7b4',
        'caroline_v1.9.370-e8b9bd2')
    self.assertEqual(result, versions)

  def testGetAllFirmwareVersions(self):
    """Verify that all model firmware versions can be extracted"""
    self.rc.SetDefaultCmdResult(output="""

flashrom(8): 68935ee2fcfcffa47af81b966269cd2b */build/reef/usr/sbin/flashrom
             ELF 64-bit LSB executable, x86-64, version 1 (SYSV), statically linked, for GNU/Linux 2.6.32, BuildID[sha1]=e102cc98d45300b50088999d53775acbeff407dc, stripped
             0.9.9  : bbb2d6a : Jul 28 2017 15:12:34 UTC

Model:        reef
BIOS image:   1b535280fe688ac284d95276492b06f6 */build/reef/tmp/portage/chromeos-base/chromeos-firmware-reef-0.0.1-r79/temp/tmp7rHApL.pack_firmware-99001/models/reef/image.bin
BIOS version: Google_Reef.9042.87.1
BIOS (RW) image:   0ef265eb8f2d228c09f75b011adbdcbb */build/reef/tmp/portage/chromeos-base/chromeos-firmware-reef-0.0.1-r79/temp/tmp7rHApL.pack_firmware-99001/models/reef/image.binrw
BIOS (RW) version: Google_Reef.9042.110.0
EC image:     2e8b4b5fa73cc5dbca4496de97a917a9 */build/reef/tmp/portage/chromeos-base/chromeos-firmware-reef-0.0.1-r79/temp/tmp7rHApL.pack_firmware-99001/models/reef/ec.bin
EC version:   reef_v1.1.5900-ab1ee51
EC (RW) version: reef_v1.1.5909-bd1f0c9

Model:        pyro
BIOS image:   9e62447ebf22a724a4a835018ab6234e */build/reef/tmp/portage/chromeos-base/chromeos-firmware-reef-0.0.1-r79/temp/tmp7rHApL.pack_firmware-99001/models/pyro/image.bin
BIOS version: Google_Pyro.9042.87.1
BIOS (RW) image:   1897457303c85de99f3e98b2eaa0eccc */build/reef/tmp/portage/chromeos-base/chromeos-firmware-reef-0.0.1-r79/temp/tmp7rHApL.pack_firmware-99001/models/pyro/image.binrw
BIOS (RW) version: Google_Pyro.9042.110.0
EC image:     44b93ed591733519e752e05aa0529eb5 */build/reef/tmp/portage/chromeos-base/chromeos-firmware-reef-0.0.1-r79/temp/tmp7rHApL.pack_firmware-99001/models/pyro/ec.bin
EC version:   pyro_v1.1.5900-ab1ee51
EC (RW) version: pyro_v1.1.5909-bd1f0c9

Model:        snappy
BIOS image:   3ab63ff080596bd7de4e7619f003bb64 */build/reef/tmp/portage/chromeos-base/chromeos-firmware-reef-0.0.1-r79/temp/tmp7rHApL.pack_firmware-99001/models/snappy/image.bin
BIOS version: Google_Snappy.9042.110.0
EC image:     c4db159e84428391d2ee25368c5fe5b6 */build/reef/tmp/portage/chromeos-base/chromeos-firmware-reef-0.0.1-r79/temp/tmp7rHApL.pack_firmware-99001/models/snappy/ec.bin
EC version:   snappy_v1.1.5909-bd1f0c9

Model:        sand
BIOS image:   387da034a4f0a3f53e278ebfdcc2a412 */build/reef/tmp/portage/chromeos-base/chromeos-firmware-reef-0.0.1-r79/temp/tmp7rHApL.pack_firmware-99001/models/sand/image.bin
BIOS version: Google_Sand.9042.110.0
EC image:     411562e0589dacec131f5fdfbe95a561 */build/reef/tmp/portage/chromeos-base/chromeos-firmware-reef-0.0.1-r79/temp/tmp7rHApL.pack_firmware-99001/models/sand/ec.bin
EC version:   sand_v1.1.5909-bd1f0c9

Model:        electro
BIOS image:   1b535280fe688ac284d95276492b06f6 */build/reef/tmp/portage/chromeos-base/chromeos-firmware-reef-0.0.1-r79/temp/tmp7rHApL.pack_firmware-99001/models/reef/image.bin
BIOS version: Google_Reef.9042.87.1
BIOS (RW) image:   0ef265eb8f2d228c09f75b011adbdcbb */build/reef/tmp/portage/chromeos-base/chromeos-firmware-reef-0.0.1-r79/temp/tmp7rHApL.pack_firmware-99001/models/reef/image.binrw
BIOS (RW) version: Google_Reef.9042.110.0
EC image:     2e8b4b5fa73cc5dbca4496de97a917a9 */build/reef/tmp/portage/chromeos-base/chromeos-firmware-reef-0.0.1-r79/temp/tmp7rHApL.pack_firmware-99001/models/reef/ec.bin
EC version:   reef_v1.1.5900-ab1ee51
EC (RW) version: reef_v1.1.5909-bd1f0c9

Package Content:
612e7bb6ed1fb0a05abf2ebdc834c18b *./updater4.sh
0eafbee07282315829d0f42135ec7c0c *./gbb_utility
6074e3ca424cb30a67c378c1d9681f9c *./mosys
68935ee2fcfcffa47af81b966269cd2b *./flashrom
0eafbee07282315829d0f42135ec7c0c *./dump_fmap
490c95d6123c208d20d84d7c16857c7c *./crosfw.sh
60899148600b8673ddb711faa55aee40 *./common.sh
3c3a99346d1ca1273cbcd86c104851ff *./shflags
de7ce035e1f82a89f8909d888ee402c0 *./crosutil.sh
f9334372bdb9036ba09a6fd9bf30e7a2 *./crossystem
22257a8d5f0adc1f50a1916c3a4a35dd *./models/reef/ec.bin
faf12dbb7cdaf21ce153bdffb67841fd *./models/reef/bios.bin
c9bbb417b7921b85a7ed999ee42f550e *./models/reef/setvars.sh
29823d46f1ec1491ecacd7b830fd2686 *./models/pyro/ec.bin
2320463aba8b22eb5ea836f094d281b3 *./models/pyro/bios.bin
81614833ad77c9cd093360ba7bea76b8 *./models/pyro/setvars.sh
411562e0589dacec131f5fdfbe95a561 *./models/sand/ec.bin
387da034a4f0a3f53e278ebfdcc2a412 *./models/sand/bios.bin
fcd8cb0ac0e2ed6be220aaae435d43ff *./models/sand/setvars.sh
c4db159e84428391d2ee25368c5fe5b6 *./models/snappy/ec.bin
3ab63ff080596bd7de4e7619f003bb64 *./models/snappy/bios.bin
fe5d699f2e9e4a7de031497953313dbd *./models/snappy/setvars.sh
79aabd7cd8a215a54234c53d7bb2e6fb *./vpd
""")
    build_sbin = os.path.join(self._buildroot, constants.DEFAULT_CHROOT_DIR,
                              'build', self._board, 'usr', 'sbin')
    osutils.Touch(os.path.join(build_sbin, 'chromeos-firmwareupdate'),
                  makedirs=True)
    result = commands.GetAllFirmwareVersions(self._buildroot, self._board)
    self.assertEqual(len(result), 5)
    self.assertEqual(
        result['reef'],
        commands.FirmwareVersions(
            'reef',
            'Google_Reef.9042.87.1',
            'Google_Reef.9042.110.0',
            'reef_v1.1.5900-ab1ee51',
            'reef_v1.1.5909-bd1f0c9'))
    self.assertEqual(
        result['pyro'],
        commands.FirmwareVersions(
            'pyro',
            'Google_Pyro.9042.87.1',
            'Google_Pyro.9042.110.0',
            'pyro_v1.1.5900-ab1ee51',
            'pyro_v1.1.5909-bd1f0c9'))
    self.assertEqual(
        result['snappy'],
        commands.FirmwareVersions(
            'snappy',
            'Google_Snappy.9042.110.0',
            None,
            'snappy_v1.1.5909-bd1f0c9',
            None))
    self.assertEqual(
        result['sand'],
        commands.FirmwareVersions(
            'sand',
            'Google_Sand.9042.110.0',
            None,
            'sand_v1.1.5909-bd1f0c9',
            None))
    self.assertEqual(
        result['electro'],
        commands.FirmwareVersions(
            'electro',
            'Google_Reef.9042.87.1',
            'Google_Reef.9042.110.0',
            'reef_v1.1.5900-ab1ee51',
            'reef_v1.1.5909-bd1f0c9'))

  def testGetModels(self):
    self.rc.SetDefaultCmdResult(output='pyro\nreef\nsnappy\n')
    build_bin = os.path.join(self._buildroot, constants.DEFAULT_CHROOT_DIR,
                             'usr', 'bin')
    osutils.Touch(os.path.join(build_bin, 'cros_config_host'), makedirs=True)
    result = commands.GetModels(self._buildroot, self._board)
    self.assertEqual(result, ['pyro', 'reef', 'snappy'])

  def testBuildMaximum(self):
    """Base case where Build is called with all options (except extra_env)."""
    self.testBuild(default=True)

  def testBuildWithEnv(self):
    """Case where Build is called with a custom environment."""
    extra_env = {'A': 'Av', 'B': 'Bv'}
    self.testBuild(extra_env=extra_env)
    self.assertCommandContains(['./build_packages'], extra_env=extra_env)

  def testGenerateBreakpadSymbols(self):
    """Test GenerateBreakpadSymbols Command."""
    commands.GenerateBreakpadSymbols(self.tempdir, self._board, False)
    self.assertCommandContains(['--board=%s' % self._board])

  def testGenerateAndroidBreakpadSymbols(self):
    """Test GenerateAndroidBreakpadSymbols Command."""
    with mock.patch.object(path_util, 'ToChrootPath', side_effect=lambda s: s):
      commands.GenerateAndroidBreakpadSymbols(
          '/buildroot', 'MyBoard', 'symbols.zip')
    self.assertCommandContains(
        ['/buildroot/chromite/bin/cros_generate_android_breakpad_symbols',
         '--symbols_file=symbols.zip',
         '--breakpad_dir=/build/MyBoard/usr/lib/debug/breakpad'])

  def testUploadSymbolsMinimal(self):
    """Test uploading symbols for official builds"""
    commands.UploadSymbols('/buildroot', 'MyBoard')
    self.assertCommandContains(
        ['/buildroot/chromite/bin/upload_symbols', '--yes',
         '--root', '/buildroot/chroot',
         '--board', 'MyBoard'])

  def testUploadSymbolsMinimalNoneChromeOS(self):
    """Test uploading symbols for official builds"""
    commands.UploadSymbols(
        '/buildroot', breakpad_root='/breakpad', product_name='CoolProduct')
    self.assertCommandContains(
        ['/buildroot/chromite/bin/upload_symbols', '--yes',
         '--breakpad_root', '/breakpad',
         '--product_name', 'CoolProduct'])

  def testUploadSymbolsMaximal(self):
    """Test uploading symbols for official builds"""
    commands.UploadSymbols(
        '/buildroot', 'MyBoard', official=True, cnt=55,
        failed_list='/failed_list.txt', breakpad_root='/breakpad',
        product_name='CoolProduct')
    self.assertCommandContains(
        ['/buildroot/chromite/bin/upload_symbols', '--yes',
         '--root', '/buildroot/chroot',
         '--board', 'MyBoard',
         '--official_build',
         '--upload-limit', '55',
         '--failed-list', '/failed_list.txt',
         '--breakpad_root', '/breakpad',
         '--product_name', 'CoolProduct'])

  def testExportToGCloudParentKey(self):
    """Test ExportToGCloud with parent_key"""
    build_root = '/buildroot'
    creds_file = 'dummy.cert'
    json_file = 'dummy.json'
    parent_key = ('MyParent', 42)
    parent_key_str = repr(parent_key)
    commands.ExportToGCloud(build_root, creds_file, json_file,
                            parent_key=parent_key)
    self.assertCommandContains(
        ['/buildroot/chromite/bin/export_to_gcloud',
         creds_file,
         json_file,
         '--parent_key',
         parent_key_str])

  def testPushImages(self):
    """Test PushImages Command."""
    m = self.PatchObject(pushimage, 'PushImage')
    commands.PushImages(self._board, 'gs://foo/R34-1234.0.0', False, None)
    self.assertEqual(m.call_count, 1)

  def testBuildImage(self):
    """Test Basic BuildImage Command."""
    commands.BuildImage(self._buildroot, self._board, None)
    self.assertCommandContains(['./build_image'])

  def testCompleteBuildImage(self):
    """Test Complete BuildImage Command."""
    images_to_build = ['bob', 'carol', 'ted', 'alice']
    commands.BuildImage(
        self._buildroot, self._board, images_to_build,
        rootfs_verification=False, extra_env={'LOVE': 'free'},
        disk_layout='2+2', version='1969')
    self.assertCommandContains(['./build_image'])

  def _TestChromeLKGM(self, chrome_revision):
    """Helper method for testing the GetChromeLKGM method."""
    chrome_lkgm = b'3322.0.0'
    url = '%s/+/%s/%s?format=text' % (
        constants.CHROMIUM_SRC_PROJECT,
        chrome_revision or 'refs/heads/master',
        constants.PATH_TO_CHROME_LKGM)
    site_params = config_lib.GetSiteParams()
    with mock.patch.object(
        gob_util, 'FetchUrl',
        return_value=base64.b64encode(chrome_lkgm)) as patcher:
      self.assertEqual(chrome_lkgm, commands.GetChromeLKGM(chrome_revision))
      patcher.assert_called_with(site_params.EXTERNAL_GOB_HOST, url)

  def testChromeLKGM(self):
    """Verifies that we can get the chrome lkgm without a chrome revision."""
    self._TestChromeLKGM(None)

  def testChromeLKGMWithRevision(self):
    """Verifies that we can get the chrome lkgm with a chrome revision."""
    self._TestChromeLKGM('deadbeef' * 5)

  def testAbortHWTests(self):
    """Verifies that HWTests are aborted for a specific non-CQ config."""
    topology.FetchTopology()
    commands.AbortHWTests('my_config', 'my_version', debug=False)
    self.assertCommandContains(['-i', 'my_config/my_version'])


class GenerateDebugTarballTests(cros_test_lib.TempDirTestCase):
  """Tests related to building tarball artifacts."""

  def setUp(self):
    self._board = 'test-board'
    self._buildroot = os.path.join(self.tempdir, 'buildroot')
    self._debug_base = os.path.join(
        self._buildroot, 'chroot', 'build', self._board, 'usr', 'lib')

    self._files = [
        'debug/s1',
        'debug/breakpad/b1',
        'debug/tests/t1',
        'debug/stuff/nested/deep',
        'debug/usr/local/build/autotest/a1',
    ]

    cros_test_lib.CreateOnDiskHierarchy(self._debug_base, self._files)

    self._tarball_dir = self.tempdir

  def testGenerateDebugTarballGdb(self):
    """Test the simplest case."""
    commands.GenerateDebugTarball(
        self._buildroot, self._board, self._tarball_dir, gdb_symbols=True)

    cros_test_lib.VerifyTarball(
        os.path.join(self._tarball_dir, 'debug.tgz'),
        [
            'debug/',
            'debug/s1',
            'debug/breakpad/',
            'debug/breakpad/b1',
            'debug/stuff/',
            'debug/stuff/nested/',
            'debug/stuff/nested/deep',
            'debug/usr/',
            'debug/usr/local/',
            'debug/usr/local/build/',
        ])

  def testGenerateDebugTarballNoGdb(self):
    """Test the simplest case."""
    commands.GenerateDebugTarball(
        self._buildroot, self._board, self._tarball_dir, gdb_symbols=False)

    cros_test_lib.VerifyTarball(
        os.path.join(self._tarball_dir, 'debug.tgz'),
        [
            'debug/breakpad/',
            'debug/breakpad/b1',
        ])


class RunLocalTryjobTests(cros_test_lib.RunCommandTestCase):
  """Tests related to RunLocalTryjob."""

  def testRunLocalTryjobsSimple(self):
    """Test that a minimul command works correctly."""
    commands.RunLocalTryjob('current_root', 'build_config')

    self.assertCommandCalled([
        'current_root/chromite/bin/cros', 'tryjob', '--local', '--yes',
        '--buildroot', mock.ANY,
        'build_config',
    ], cwd='current_root')

  def testRunLocalTryjobsComplex(self):
    """Test that a minimul command works correctly."""
    commands.RunLocalTryjob(
        'current_root', 'build_config', args=('--funky', 'stuff'),
        target_buildroot='target_root')

    self.assertCommandCalled([
        'current_root/chromite/bin/cros', 'tryjob', '--local', '--yes',
        '--buildroot', 'target_root', 'build_config', '--funky', 'stuff',
    ], cwd='current_root')


class BuildTarballTests(cros_test_lib.RunCommandTempDirTestCase):
  """Tests related to building tarball artifacts."""

  def setUp(self):
    self._buildroot = os.path.join(self.tempdir, 'buildroot')
    os.makedirs(self._buildroot)
    self._board = 'test-board'
    self._cwd = os.path.abspath(
        os.path.join(self._buildroot, 'chroot', 'build', self._board,
                     constants.AUTOTEST_BUILD_PATH, '..'))
    self._sysroot_build = os.path.abspath(
        os.path.join(self._buildroot, 'chroot', 'build', self._board, 'build'))
    self._tarball_dir = self.tempdir


  def testBuildFullAutotestTarball(self):
    """Tests that our call to generate the full autotest tarball is correct."""
    with mock.patch.object(commands, 'BuildTarball') as m:
      m.return_value.returncode = 0
      commands.BuildFullAutotestTarball(self._buildroot, self._board,
                                        self._tarball_dir)
      m.assert_called_once_with(self._buildroot, ['autotest'],
                                os.path.join(self._tarball_dir,
                                             'autotest.tar.bz2'),
                                cwd=self._cwd, check=False)

  def testBuildAutotestPackagesTarball(self):
    """Tests that generating the autotest packages tarball is correct."""
    with mock.patch.object(commands, 'BuildTarball') as m:
      commands.BuildAutotestPackagesTarball(self._buildroot, self._cwd,
                                            self._tarball_dir)
      m.assert_called_once_with(self._buildroot, ['autotest/packages'],
                                os.path.join(self._tarball_dir,
                                             'autotest_packages.tar'),
                                cwd=self._cwd, compressed=False)

  def testBuildAutotestControlFilesTarball(self):
    """Tests that generating the autotest control files tarball is correct."""
    control_file_list = ['autotest/client/site_tests/testA/control',
                         'autotest/server/site_tests/testB/control']
    with mock.patch.object(commands, 'FindFilesWithPattern') as find_mock:
      find_mock.return_value = control_file_list
      with mock.patch.object(commands, 'BuildTarball') as tar_mock:
        commands.BuildAutotestControlFilesTarball(self._buildroot, self._cwd,
                                                  self._tarball_dir)
        tar_mock.assert_called_once_with(self._buildroot, control_file_list,
                                         os.path.join(self._tarball_dir,
                                                      'control_files.tar'),
                                         cwd=self._cwd, compressed=False)

  def testBuildAutotestServerPackageTarball(self):
    """Tests that generating the autotest server package tarball is correct."""
    control_file_list = ['autotest/server/site_tests/testA/control',
                         'autotest/server/site_tests/testB/control']
    # Pass a copy of the file list so the code under test can't mutate it.
    self.PatchObject(commands, 'FindFilesWithPattern',
                     return_value=list(control_file_list))
    tar_mock = self.PatchObject(commands, 'BuildTarball')

    expected_files = list(control_file_list)

    # Touch Tast paths so they'll be included in the tar command. Skip creating
    # the last file so we can verify that it's omitted from the tar command.
    for p in commands.TAST_SSP_FILES[:-1]:
      path = os.path.join(self._buildroot, p)
      if not os.path.exists(os.path.dirname(path)):
        os.makedirs(os.path.dirname(path))
      open(path, 'a').close()
      expected_files.append(path)

    commands.BuildAutotestServerPackageTarball(self._buildroot, self._cwd,
                                               self._tarball_dir)

    tar_mock.assert_called_once_with(
        self._buildroot, expected_files,
        os.path.join(self._tarball_dir, commands.AUTOTEST_SERVER_PACKAGE),
        cwd=self._cwd, extra_args=mock.ANY, check=False)

  def testBuildTastTarball(self):
    """Tests that generating the Tast private bundles tarball is correct."""
    expected_tarball = os.path.join(self._tarball_dir, 'tast_bundles.tar.bz2')

    for d in ('libexec/tast', 'share/tast'):
      os.makedirs(os.path.join(self._cwd, d))

    chroot = chroot_lib.Chroot(os.path.join(self._buildroot, 'chroot'))
    sysroot = sysroot_lib.Sysroot(os.path.join('/build', self._board))
    patch = self.PatchObject(artifacts_service, 'BundleTastFiles',
                             return_value=expected_tarball)

    tarball = commands.BuildTastBundleTarball(self._buildroot,
                                              self._sysroot_build,
                                              self._tarball_dir)
    self.assertEqual(expected_tarball, tarball)
    patch.assert_called_once_with(chroot, sysroot, self._tarball_dir)

  def testBuildTastTarballNoBundle(self):
    """Tests the case when the Tast private bundles tarball is not generated."""
    self.PatchObject(artifacts_service, 'BundleTastFiles', return_value=None)
    tarball = commands.BuildTastBundleTarball(self._buildroot,
                                              self._sysroot_build,
                                              self._tarball_dir)
    self.assertIsNone(tarball)

  def testBuildPinnedGuestImagesTarball(self):
    """Tests that generating a guest images tarball."""
    expected_tarball = os.path.join(self._tarball_dir, 'guest_images.tar')

    pin_dir = os.path.join(self._buildroot, 'chroot', 'build', self._board,
                           constants.GUEST_IMAGES_PINS_PATH)
    os.makedirs(pin_dir)
    for filename in ('file1', 'file2'):
      pin_file = os.path.join(pin_dir, filename + '.json')
      with open(pin_file, 'w') as f:
        pin = {
            'filename': filename + '.tar.gz',
            'gsuri': 'gs://%s' % filename,
        }
        json.dump(pin, f)

    gs_mock = self.PatchObject(gs.GSContext, 'Copy')

    with mock.patch.object(commands, 'BuildTarball') as m:
      tarball = commands.BuildPinnedGuestImagesTarball(self._buildroot,
                                                       self._board,
                                                       self._tarball_dir)
      self.assertEqual(expected_tarball, tarball)
      gs_mock.assert_called_with('gs://file2', os.path.join(self._tarball_dir,
                                                            'file2.tar.gz'))
      m.assert_called_once_with(self._buildroot,
                                ['file1.tar.gz', 'file2.tar.gz'],
                                expected_tarball,
                                cwd=self._tarball_dir,
                                compressed=False)

  def testBuildPinnedGuestImagesTarballBadPin(self):
    """Tests that generating a guest images tarball with a bad pin file."""
    pin_dir = os.path.join(self._buildroot, 'chroot', 'build', self._board,
                           constants.GUEST_IMAGES_PINS_PATH)
    os.makedirs(pin_dir)
    pin_file = os.path.join(pin_dir, 'file1.json')
    with open(pin_file, 'w') as f:
      pin = {
          'gsuri': 'gs://%s' % 'file1',
      }
      json.dump(pin, f)

    gs_mock = self.PatchObject(gs.GSContext, 'Copy')

    with mock.patch.object(commands, 'BuildTarball') as m:
      tarball = commands.BuildPinnedGuestImagesTarball(self._buildroot,
                                                       self._board,
                                                       self._tarball_dir)
      self.assertIs(tarball, None)
      gs_mock.assert_not_called()
      m.assert_not_called()

  def testBuildPinnedGuestImagesTarballNoPins(self):
    """Tests that generating a guest images tarball with no pins."""
    gs_mock = self.PatchObject(gs.GSContext, 'Copy')

    with mock.patch.object(commands, 'BuildTarball') as m:
      tarball = commands.BuildPinnedGuestImagesTarball(self._buildroot,
                                                       self._board,
                                                       self._tarball_dir)
      self.assertIs(tarball, None)
      gs_mock.assert_not_called()
      m.assert_not_called()

  def testBuildStrippedPackagesArchive(self):
    """Test generation of stripped package tarball using globs."""
    package_globs = ['chromeos-base/chromeos-chrome', 'sys-kernel/*kernel*']
    self.PatchObject(
        portage_util, 'FindPackageNameMatches',
        side_effect=[
            [portage_util.SplitCPV('chromeos-base/chrome-1-r0')],
            [portage_util.SplitCPV('sys-kernel/kernel-1-r0'),
             portage_util.SplitCPV('sys-kernel/kernel-2-r0')]])
    # Drop "stripped packages".
    sysroot = os.path.join(self._buildroot, 'chroot', 'build', 'test-board')
    pkg_dir = os.path.join(sysroot, 'stripped-packages')
    osutils.Touch(os.path.join(pkg_dir, 'chromeos-base', 'chrome-1-r0.tbz2'),
                  makedirs=True)
    sys_kernel = os.path.join(pkg_dir, 'sys-kernel')
    osutils.Touch(os.path.join(sys_kernel, 'kernel-1-r0.tbz2'), makedirs=True)
    osutils.Touch(os.path.join(sys_kernel, 'kernel-1-r01.tbz2'), makedirs=True)
    osutils.Touch(os.path.join(sys_kernel, 'kernel-2-r0.tbz1'), makedirs=True)
    osutils.Touch(os.path.join(sys_kernel, 'kernel-2-r0.tbz2'), makedirs=True)
    stripped_files_list = [
        os.path.join('stripped-packages', 'chromeos-base', 'chrome-1-r0.tbz2'),
        os.path.join('stripped-packages', 'sys-kernel', 'kernel-1-r0.tbz2'),
        os.path.join('stripped-packages', 'sys-kernel', 'kernel-2-r0.tbz2'),
    ]

    tar_mock = self.PatchObject(commands, 'BuildTarball')
    self.PatchObject(cros_build_lib, 'run')
    commands.BuildStrippedPackagesTarball(self._buildroot,
                                          'test-board',
                                          package_globs,
                                          self.tempdir)
    tar_mock.assert_called_once_with(
        self._buildroot, stripped_files_list,
        os.path.join(self.tempdir, 'stripped-packages.tar'),
        cwd=sysroot, compressed=False)


class UnmockedTests(cros_test_lib.TempDirTestCase):
  """Test cases which really run tests, instead of using mocks."""

  def testBuildFirmwareArchive(self):
    """Verifies that the archiver creates a tarfile with the expected files."""
    # Set of files to tar up
    fw_files = (
        'dts/emeraldlake2.dts',
        'image-link.rw.bin',
        'nv_image-link.bin',
        'pci8086,0166.rom',
        'seabios.cbfs',
        'u-boot.elf',
        'u-boot_netboot.bin',
        'updater-link.rw.sh',
        'x86-memtest',
    )
    board = 'link'
    chroot_path = 'chroot/build/%s/firmware' % board
    fw_test_root = os.path.join(self.tempdir, os.path.basename(__file__))
    fw_files_root = os.path.join(fw_test_root, chroot_path)

    # Generate the fw_files in fw_files_root.
    cros_test_lib.CreateOnDiskHierarchy(fw_files_root, fw_files)

    # Create an archive from the specified test directory.
    returned_archive_name = commands.BuildFirmwareArchive(fw_test_root, board,
                                                          fw_test_root)
    # Verify we get a valid tarball returned whose name uses the default name.
    self.assertTrue(returned_archive_name is not None)
    self.assertEqual(returned_archive_name, constants.FIRMWARE_ARCHIVE_NAME)

    # Create an archive and specify that archive filename.
    archive_name = 'alternative_archive.tar.bz2'
    returned_archive_name = commands.BuildFirmwareArchive(fw_test_root, board,
                                                          fw_test_root,
                                                          archive_name)
    # Verify that we get back an archive file using the specified name.
    self.assertEqual(archive_name, returned_archive_name)


  def findFilesWithPatternExpectedResults(self, root, files):
    """Generate the expected results for testFindFilesWithPattern"""
    return [os.path.join(root, f) for f in files]

  def testFindFilesWithPattern(self):
    """Verifies FindFilesWithPattern searches and excludes files properly"""
    search_files = (
        'file1',
        'test1',
        'file2',
        'dir1/file1',
        'dir1/test1',
        'dir2/file2',
    )
    search_files_root = os.path.join(self.tempdir, 'FindFilesWithPatternTest')
    cros_test_lib.CreateOnDiskHierarchy(search_files_root, search_files)
    find_all = commands.FindFilesWithPattern('*', target=search_files_root)
    expected_find_all = self.findFilesWithPatternExpectedResults(
        search_files_root, search_files)
    self.assertEqual(set(find_all), set(expected_find_all))
    find_test_files = commands.FindFilesWithPattern('test*',
                                                    target=search_files_root)
    find_test_expected = self.findFilesWithPatternExpectedResults(
        search_files_root, ['test1', 'dir1/test1'])
    self.assertEqual(set(find_test_files), set(find_test_expected))
    find_exclude = commands.FindFilesWithPattern(
        '*', target=search_files_root,
        exclude_dirs=(os.path.join(search_files_root, 'dir1'),))
    find_exclude_expected = self.findFilesWithPatternExpectedResults(
        search_files_root, ['file1', 'test1', 'file2', 'dir2/file2'])
    self.assertEqual(set(find_exclude), set(find_exclude_expected))

  def testGenerateUploadJSON(self):
    """Verifies GenerateUploadJSON"""
    archive = os.path.join(self.tempdir, 'archive')
    osutils.SafeMakedirs(archive)

    # Text file.
    text_str = 'Happiness equals reality minus expectations.\n'
    osutils.WriteFile(os.path.join(archive, 'file1.txt'), text_str)

    # JSON file.
    json_str = json.dumps([{'Salt': 'Pepper', 'Pots': 'Pans',
                            'Cloak': 'Dagger', 'Shoes': 'Socks'}])
    osutils.WriteFile(os.path.join(archive, 'file2.json'), json_str)

    # Binary file.
    bin_blob = struct.pack('6B', 228, 39, 123, 87, 2, 168)
    with open(os.path.join(archive, 'file3.bin'), 'wb') as f:
      f.write(bin_blob)

    # Directory.
    osutils.SafeMakedirs(os.path.join(archive, 'dir'))

    # List of files in archive.
    uploaded = os.path.join(self.tempdir, 'uploaded')
    osutils.WriteFile(uploaded, 'file1.txt\nfile2.json\nfile3.bin\ndir\n')

    upload_file = os.path.join(self.tempdir, 'upload.json')
    commands.GenerateUploadJSON(upload_file, archive, uploaded)
    parsed = json.loads(osutils.ReadFile(upload_file))

    # Directory should be ignored.
    test_content = {'file1.txt': text_str.encode('utf-8'),
                    'file2.json': json_str.encode('utf-8'),
                    'file3.bin': bin_blob}

    self.assertEqual(set(parsed.keys()), set(test_content.keys()))

    # Verify the math.
    for filename, content in test_content.items():
      entry = parsed[filename]
      size = len(content)
      sha1 = base64.b64encode(hashlib.sha1(content).digest()).decode('utf-8')
      sha256 = base64.b64encode(
          hashlib.sha256(content).digest()).decode('utf-8')

      self.assertEqual(entry['size'], size)
      self.assertEqual(entry['sha1'], sha1)
      self.assertEqual(entry['sha256'], sha256)

  def testGenerateHtmlIndexTuple(self):
    """Verifies GenerateHtmlIndex gives us something sane (input: tuple)"""
    index = os.path.join(self.tempdir, 'index.html')
    files = ('file1', 'monkey tree', 'flying phone',)
    commands.GenerateHtmlIndex(index, files)
    html = osutils.ReadFile(index)
    for f in files:
      self.assertIn('>%s</a>' % f, html)

  def testGenerateHtmlIndexTupleDupe(self):
    """Verifies GenerateHtmlIndex gives us something unique (input: tuple)"""
    index = os.path.join(self.tempdir, 'index.html')
    files = ('file1', 'file1', 'file1',)
    commands.GenerateHtmlIndex(index, files)
    html = osutils.ReadFile(index)
    self.assertEqual(html.count('>file1</a>'), 1)

  def testGenerateHtmlIndexTuplePretty(self):
    """Verifies GenerateHtmlIndex gives us something pretty (input: tuple)"""
    index = os.path.join(self.tempdir, 'index.html')
    files = ('..|up', 'f.txt|MY FILE', 'm.log|MONKEY', 'b.bin|Yander',)
    commands.GenerateHtmlIndex(index, files)
    html = osutils.ReadFile(index)
    for f in files:
      a = f.split('|')
      self.assertIn('href="%s"' % a[0], html)
      self.assertIn('>%s</a>' % a[1], html)

  def testGenerateHtmlIndexDir(self):
    """Verifies GenerateHtmlIndex gives us something sane (input: dir)"""
    index = os.path.join(self.tempdir, 'index.html')
    files = ('a', 'b b b', 'c', 'dalsdkjfasdlkf',)
    simple_dir = os.path.join(self.tempdir, 'dir')
    for f in files:
      osutils.Touch(os.path.join(simple_dir, f), makedirs=True)
    commands.GenerateHtmlIndex(index, files)
    html = osutils.ReadFile(index)
    for f in files:
      self.assertIn('>%s</a>' % f, html)

  def testGenerateHtmlIndexFile(self):
    """Verifies GenerateHtmlIndex gives us something sane (input: file)"""
    index = os.path.join(self.tempdir, 'index.html')
    files = ('a.tgz', 'b b b.txt', 'c', 'dalsdkjfasdlkf',)
    filelist = os.path.join(self.tempdir, 'listing')
    osutils.WriteFile(filelist, '\n'.join(files))
    commands.GenerateHtmlIndex(index, filelist)
    html = osutils.ReadFile(index)
    for f in files:
      self.assertIn('>%s</a>' % f, html)

  def testGenerateHtmlTimeline(self):
    """Verifies GenerateHtmlTimeline gives us something sane."""
    timeline = os.path.join(self.tempdir, 'timeline.html')
    now = dt.datetime.now()
    rows = [
        ('test1', now - dt.timedelta(0, 3600), now - dt.timedelta(0, 1800)),
        ('test2', now - dt.timedelta(0, 3600), now - dt.timedelta(0, 600)),
        ('test3', now - dt.timedelta(0, 1800), now - dt.timedelta(0, 1200))
    ]
    commands.GenerateHtmlTimeline(timeline, rows, 'my-timeline')
    html = osutils.ReadFile(timeline)
    self.assertIn('my-timeline', html)
    for r in rows:
      self.assertIn('["%s", new Date' % r[0], html)

  def testArchiveGeneration(self):
    """Verifies BuildStandaloneImageArchive produces correct archives"""
    image_dir = os.path.join(self.tempdir, 'inputs')
    archive_dir = os.path.join(self.tempdir, 'outputs')
    files = ('a.bin', 'aa', 'b b b', 'c', 'dalsdkjfasdlkf',)
    dlc_dir = 'dlc'
    osutils.SafeMakedirs(image_dir)
    osutils.SafeMakedirs(archive_dir)
    for f in files:
      osutils.Touch(os.path.join(image_dir, f))
    osutils.SafeMakedirs(os.path.join(image_dir, dlc_dir))

    # Check specifying tar functionality.
    artifact = {'paths': ['a.bin'], 'output': 'a.tar.gz', 'archive': 'tar',
                'compress': 'gz'}
    path = commands.BuildStandaloneArchive(archive_dir, image_dir, artifact)
    self.assertEqual(path, ['a.tar.gz'])
    cros_test_lib.VerifyTarball(os.path.join(archive_dir, path[0]),
                                ['a.bin'])

    # Check multiple input files.
    artifact = {'paths': ['a.bin', 'aa'], 'output': 'aa.tar.gz',
                'archive': 'tar', 'compress': 'gz'}
    path = commands.BuildStandaloneArchive(archive_dir, image_dir, artifact)
    self.assertEqual(path, ['aa.tar.gz'])
    cros_test_lib.VerifyTarball(os.path.join(archive_dir, path[0]),
                                ['a.bin', 'aa'])

    # Check zip functionality.
    artifact = {'paths': ['a.bin'], 'archive': 'zip'}
    path = commands.BuildStandaloneArchive(archive_dir, image_dir, artifact)
    self.assertEqual(path, ['a.zip'])
    self.assertExists(os.path.join(archive_dir, path[0]))

    # Check directory copy functionality.
    artifact = {'paths': ['dlc'], 'output': 'dlc'}
    path = commands.BuildStandaloneArchive(archive_dir, image_dir, artifact)
    self.assertEqual(path, ['dlc'])
    self.assertExists(os.path.join(archive_dir, path[0]))

  def testGceTarballGeneration(self):
    """Verifies BuildGceTarball produces correct archives"""
    image_dir = os.path.join(self.tempdir, 'inputs')
    archive_dir = os.path.join(self.tempdir, 'outputs')
    image = constants.TEST_IMAGE_BIN
    output = constants.TEST_IMAGE_GCE_TAR

    osutils.SafeMakedirs(image_dir)
    osutils.SafeMakedirs(archive_dir)
    osutils.Touch(os.path.join(image_dir, image))

    output_tar = commands.BuildGceTarball(archive_dir, image_dir, image)
    self.assertEqual(output, output_tar)

    output_path = os.path.join(archive_dir, output_tar)
    self.assertExists(output_path)

    # GCE expects the tarball to be in a particular format.
    cros_test_lib.VerifyTarball(output_path, ['disk.raw'])

  def testBuildEbuildLogsTarballPositive(self):
    """Verifies that the ebuild logs archiver builds correct logs"""
    # Names of log files typically found in a build directory.
    log_files = (
        '',
        'x11-libs:libdrm-2.4.81-r24:20170816-175008.log',
        'x11-libs:libpciaccess-0.12.902-r2:20170816-174849.log',
        'x11-libs:libva-1.7.1-r2:20170816-175019.log',
        'x11-libs:libva-intel-driver-1.7.1-r4:20170816-175029.log',
        'x11-libs:libxkbcommon-0.4.3-r2:20170816-174908.log',
        'x11-libs:pango-1.32.5-r1:20170816-174954.log',
        'x11-libs:pixman-0.32.4:20170816-174832.log',
        'x11-misc:xkeyboard-config-2.15-r3:20170816-174908.log',
        'x11-proto:kbproto-1.0.5:20170816-174849.log',
        'x11-proto:xproto-7.0.31:20170816-174849.log',
    )
    tarred_files = [os.path.join('logs', x) for x in log_files]
    board = 'samus'
    log_files_root = os.path.join(self.tempdir,
                                  'chroot/build/%s/tmp/portage/logs' % board)
    # Generate a representative set of log files produced by a typical build.
    cros_test_lib.CreateOnDiskHierarchy(log_files_root, log_files)
    # Create an archive from the simulated logs directory
    tarball = os.path.join(self.tempdir,
                           commands.BuildEbuildLogsTarball(self.tempdir, board,
                                                           self.tempdir))
    # Verify the tarball contents.
    cros_test_lib.VerifyTarball(tarball, tarred_files)

  def testBuildEbuildLogsTarballNegative(self):
    """Verifies that the Ebuild logs archiver handles wrong inputs"""
    # Names of log files typically found in a build directory.
    log_files = (
        '',
        'x11-libs:libdrm-2.4.81-r24:20170816-175008.log',
        'x11-libs:libpciaccess-0.12.902-r2:20170816-174849.log',
        'x11-libs:libva-1.7.1-r2:20170816-175019.log',
        'x11-libs:libva-intel-driver-1.7.1-r4:20170816-175029.log',
        'x11-libs:libxkbcommon-0.4.3-r2:20170816-174908.log',
        'x11-libs:pango-1.32.5-r1:20170816-174954.log',
        'x11-libs:pixman-0.32.4:20170816-174832.log',
        'x11-misc:xkeyboard-config-2.15-r3:20170816-174908.log',
        'x11-proto:kbproto-1.0.5:20170816-174849.log',
        'x11-proto:xproto-7.0.31:20170816-174849.log',
    )

    board = 'samus'
    # Create a malformed directory name.
    log_files_root = os.path.join(self.tempdir,
                                  '%s/tmp/portage/wrong_dir_name' % board)
    # Generate a representative set of log files produced by a typical build.
    cros_test_lib.CreateOnDiskHierarchy(log_files_root, log_files)

    # Create an archive from the simulated logs directory
    wrong_board = 'chell'
    tarball_rel_path = commands.BuildEbuildLogsTarball(self.tempdir,
                                                       wrong_board,
                                                       self.tempdir)
    self.assertEqual(tarball_rel_path, None)
    tarball_rel_path = commands.BuildEbuildLogsTarball(self.tempdir,
                                                       board, self.tempdir)
    self.assertEqual(tarball_rel_path, None)


class ImageTestCommandsTest(cros_test_lib.RunCommandTestCase):
  """Test commands related to ImageTest tests."""

  def setUp(self):
    self._build = 'test-build'
    self._board = 'test-board'
    self._image_dir = 'image-dir'
    self._result_dir = 'result-dir'
    self.PatchObject(path_util, 'ToChrootPath',
                     side_effect=lambda x: x)

  def testRunTestImage(self):
    """Verifies RunTestImage calls into test-image script properly."""
    commands.RunTestImage(self._build, self._board, self._image_dir,
                          self._result_dir)
    self.assertCommandContains(
        [
            'sudo', '--',
            os.path.join(self._build, 'chromite', 'bin', 'test_image'),
            '--board', self._board,
            '--test_results_root',
            path_util.ToChrootPath(self._result_dir),
            path_util.ToChrootPath(self._image_dir),
        ],
        enter_chroot=True,
    )

class GenerateAFDOArtifactsTests(
    cros_test_lib.RunCommandTempDirTestCase):
  """Test GenerateChromeOrderfileArtifacts command."""

  def setUp(self):
    self.buildroot = os.path.join(self.tempdir, 'buildroot')
    osutils.SafeMakedirs(self.buildroot)
    chroot_tmp = os.path.join(self.buildroot, 'chroot', 'tmp')
    osutils.SafeMakedirs(chroot_tmp)
    self.board = 'board'
    self.target = 'any_target'
    self.chrome_root = '/path/to/chrome_root'
    self.output_path = os.path.join(self.tempdir, 'output_dir')
    osutils.SafeMakedirs(self.output_path)
    self.mock_command = self.PatchObject(commands,
                                         'RunBuildScript')

  def testGeneratePass(self):
    """Test generate call correctly."""
    # Redirect the current tempdir to read/write contents inside it.
    self.PatchObject(osutils.TempDir, '__enter__',
                     return_value=self.tempdir)
    input_proto_file = os.path.join(self.tempdir, 'input.json')
    output_proto_file = os.path.join(self.tempdir, 'output.json')
    # Write dummy outputs to output JSON file
    with open(output_proto_file, 'w') as f:
      output_proto = {
          'artifacts': [
              {'path': 'artifact1'},
              {'path': 'artifact2'},
          ]
      }
      json.dump(output_proto, f)

    ret = commands.GenerateAFDOArtifacts(
        self.buildroot, self.chrome_root, self.board,
        self.output_path, self.target)

    cmd = [
        'build_api',
        'chromite.api.ArtifactsService/BundleAFDOGenerationArtifacts',
        '--input-json', input_proto_file,
        '--output-json', output_proto_file,
    ]

    self.mock_command.assert_called_once_with(
        self.buildroot, cmd,
        chromite_cmd=True, stdout=True)

    # Verify the input proto has all the information
    input_proto = json.loads(osutils.ReadFile(input_proto_file))
    self.assertEqual(input_proto['chroot']['path'],
                     os.path.join(self.buildroot, 'chroot'))
    self.assertEqual(input_proto['chroot']['chrome_dir'],
                     self.chrome_root)
    self.assertEqual(input_proto['build_target']['name'],
                     self.board)
    self.assertEqual(input_proto['output_dir'],
                     self.output_path)
    self.assertEqual(input_proto['artifact_type'], self.target)

    # Verify the output matches the proto
    self.assertEqual(ret, ['artifact1', 'artifact2'])


class VerifyAFDOArtifactsTests(
    cros_test_lib.RunCommandTempDirTestCase):
  """Test VerifyChromeOrderfileArtifacts command."""

  def setUp(self):
    self.buildroot = os.path.join(self.tempdir, 'buildroot')
    osutils.SafeMakedirs(self.buildroot)
    chroot_tmp = os.path.join(self.buildroot, 'chroot', 'tmp')
    osutils.SafeMakedirs(chroot_tmp)
    self.board = 'board'
    self.target = 'any_target'
    self.build_api = 'path.to.anyBuildAPI'
    self.mock_command = self.PatchObject(commands,
                                         'RunBuildScript')

  def testVerifyPass(self):
    """Test verify call correctly."""
    # Redirect the current tempdir to read/write contents inside it.
    self.PatchObject(osutils.TempDir, '__enter__',
                     return_value=self.tempdir)
    input_proto_file = os.path.join(self.tempdir, 'input.json')
    output_proto_file = os.path.join(self.tempdir, 'output.json')
    # Write dummy outputs to output JSON file
    with open(output_proto_file, 'w') as f:
      output_proto = {
          'status': True
      }
      json.dump(output_proto, f)

    ret = commands.VerifyAFDOArtifacts(
        self.buildroot, self.board, self.target, self.build_api)

    cmd = [
        'build_api',
        self.build_api,
        '--input-json', input_proto_file,
        '--output-json', output_proto_file,
    ]

    self.mock_command.assert_called_once_with(
        self.buildroot, cmd,
        chromite_cmd=True, stdout=True)

    # Verify the input proto has all the information
    input_proto = json.loads(osutils.ReadFile(input_proto_file))
    self.assertEqual(input_proto['build_target']['name'],
                     self.board)
    self.assertEqual(input_proto['artifact_type'], self.target)

    # Verify the output matches the proto
    self.assertTrue(ret)


class MarkAndroidAsStableTest(cros_test_lib.RunCommandTempDirTestCase):
  """MarkAndroidAsStable tests."""

  def testSuccess(self):
    """Test input and success handling."""
    # Raw arguments for the function.
    buildroot = '/buildroot'
    tracking_branch = 'refs/tracking'
    android_package = 'android/android-1.0-r1'
    android_build_branch = 'refs/build'
    boards = ['foo', 'bar']
    android_version = '1.0'

    # Write out the mock response.
    response_package = {'category': 'android', 'package_name': 'android',
                        'version': '1.0-r2'}

    call_patch = self.PatchObject(
        commands, 'CallBuildApiWithInputProto',
        return_value={'status': 1, 'android_atom': response_package})

    new_atom = commands.MarkAndroidAsStable(
        buildroot, tracking_branch, android_package, android_build_branch,
        boards=boards, android_version=android_version)

    # Make sure the atom is rebuilt correctly from the package info.
    self.assertEqual('android/android-1.0-r2', new_atom)

    expected_input = {
        'trackingBranch': tracking_branch,
        'packageName': android_package,
        'androidBuildBranch': android_build_branch,
        'androidVersion': android_version,
        'buildTargets': [{'name': 'foo'}, {'name': 'bar'}],
    }
    call_patch.assert_called_with(
        buildroot, 'chromite.api.AndroidService/MarkStable', expected_input)
