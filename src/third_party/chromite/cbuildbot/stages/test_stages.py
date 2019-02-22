# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the test stages."""

from __future__ import print_function

import collections
import math
import os

from chromite.cbuildbot import afdo
from chromite.cbuildbot import cbuildbot_run
from chromite.cbuildbot import commands
from chromite.cbuildbot import validation_pool
from chromite.cbuildbot.stages import generic_stages
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import failures_lib
from chromite.lib import gs
from chromite.lib import hwtest_results
from chromite.lib import image_test_lib
from chromite.lib import osutils
from chromite.lib import path_util
from chromite.lib import perf_uploader
from chromite.lib import portage_util
from chromite.lib import timeout_util


PRE_CQ = validation_pool.PRE_CQ


class UnitTestStage(generic_stages.BoardSpecificBuilderStage,
                    generic_stages.ArchivingStageMixin):
  """Run unit tests."""

  option_name = 'tests'
  config_name = 'unittests'
  category = constants.PRODUCT_OS_STAGE

  # If the unit tests take longer than 90 minutes, abort. They usually take
  # thirty minutes to run, but they can take twice as long if the machine is
  # under load (e.g. in canary groups).
  #
  # If the processes hang, parallel_emerge will print a status report after 60
  # minutes, so we picked 90 minutes because it gives us a little buffer time.
  UNIT_TEST_TIMEOUT = 90 * 60

  def HandleSkip(self):
    """Launch DebugSymbolsStage if UnitTestStage is skipped."""
    self.board_runattrs.SetParallel('unittest_completed', True)
    return super(UnitTestStage, self).HandleSkip()

  def _HandleStageException(self, exc_info):
    """Launch DebugSymbolStage if UnitTestStage is raising an exception."""
    self.board_runattrs.SetParallel('unittest_completed', True)
    return super(UnitTestStage, self)._HandleStageException(exc_info)

  def PerformStage(self):
    extra_env = {}
    if self._run.config.useflags:
      extra_env['USE'] = ' '.join(self._run.config.useflags)
    r = ' Reached UnitTestStage timeout.'
    with timeout_util.Timeout(self.UNIT_TEST_TIMEOUT, reason_message=r):
      commands.RunUnitTests(self._build_root,
                            self._current_board,
                            blacklist=self._run.config.unittest_blacklist,
                            extra_env=extra_env)
    # The attribute 'unittest_completed' is used in DebugSymbolsStage.
    self.board_runattrs.SetParallel('unittest_completed', True)
    # Package UnitTest binaries.
    tarball = commands.BuildUnitTestTarball(
        self._build_root, self._current_board, self.archive_path)
    self.UploadArtifact(tarball, archive=False)


class HWTestStage(generic_stages.BoardSpecificBuilderStage,
                  generic_stages.ArchivingStageMixin):
  """Stage that runs tests in the Autotest lab."""

  option_name = 'tests'
  config_name = 'hw_tests'
  category = constants.TEST_INFRA_STAGE

  PERF_RESULTS_EXTENSION = 'results'

  def __init__(
      self,
      builder_run,
      board,
      model,
      suite_config,
      suffix=None,
      lab_board_name=None,
      **kwargs):

    if suffix is None:
      suffix = ''

    if model:
      suffix += ' [%s]' % (model)

    if not self.TestsEnabled(builder_run):
      suffix += ' [DISABLED]'

    suffix = self.UpdateSuffix(suite_config.suite, suffix)
    super(HWTestStage, self).__init__(builder_run, board,
                                      suffix=suffix,
                                      **kwargs)
    if not self._run.IsToTBuild():
      suite_config.SetBranchedValues()

    self.suite_config = suite_config
    self.wait_for_results = True

    self._model = model
    self._board_name = lab_board_name or board

  # Disable complaint about calling _HandleStageException.
  # pylint: disable=protected-access
  def _HandleStageException(self, exc_info):
    """Override and don't set status to FAIL but FORGIVEN instead."""
    exc_type = exc_info[0]

    # If the suite config says HW Tests can only warn, only warn.
    if self.suite_config.warn_only:
      return self._HandleExceptionAsWarning(exc_info)

    if self.suite_config.critical:
      return super(HWTestStage, self)._HandleStageException(exc_info)

    if issubclass(exc_type, failures_lib.TestWarning):
      # HWTest passed with warning. All builders should pass.
      logging.warning('HWTest passed with warning code.')
      return self._HandleExceptionAsWarning(exc_info)
    elif issubclass(exc_type, failures_lib.BoardNotAvailable):
      # Some boards may not have been setup in the lab yet for
      # non-code-checkin configs.
      if not config_lib.IsPFQType(self._run.config.build_type):
        logging.info('HWTest did not run because the board was not '
                     'available in the lab yet')
        return self._HandleExceptionAsSuccess(exc_info)

    return super(HWTestStage, self)._HandleStageException(exc_info)

  def ReportHWTestResults(self, json_dump_dict, build_id, db):
    """Report HWTests results to cidb.

    Args:
      json_dump_dict: A dict containing the command json dump results.
      build_id: The build id (string) of this build.
      db: An instance of cidb.CIDBConnection.

    Returns:
      How many results are reported to CIDB.
    """
    if not json_dump_dict:
      logging.info('No json dump found, no HWTest results to report')
      return

    if not db:
      logging.info('No DB instance found, not reporting HWTest results.')
      return

    results = []
    for test_name, value in json_dump_dict.get('tests', dict()).iteritems():
      status = value.get('status')
      result = constants.HWTEST_STATUS_OTHER
      if status == 'GOOD':
        result = constants.HWTEST_STATUS_PASS
      elif status == 'FAIL':
        result = constants.HWTEST_STATUS_FAIL
      elif status == 'ABORT':
        result = constants.HWTEST_STATUS_ABORT
      else:
        logging.info('Unknown status for test %s:%s', test_name, result)

      results.append(hwtest_results.HWTestResult.FromReport(
          build_id, test_name, result))

    if results:
      logging.info('Reporting hwtest results: %s ', results)
      db.InsertHWTestResults(results)

    return len(results)

  def WaitUntilReady(self):
    """Wait until payloads and test artifacts are ready or not."""
    # Wait for UploadHWTestArtifacts to generate and upload the artifacts.
    if not self.GetParallel('test_artifacts_uploaded',
                            pretty_name='payloads and test artifacts'):
      logging.PrintBuildbotStepWarnings()
      logging.warning('missing test artifacts')
      logging.warning('Cannot run %s because UploadTestArtifacts failed. '
                      'See UploadTestArtifacts for details.', self.stage_name)
      return False

    return True

  def TestsEnabled(self, builder_run):
    """Abstract the logic to decide if tests are enabled."""
    if (builder_run.options.remote_trybot and
        (builder_run.options.hwtest or builder_run.config.pre_cq)):
      return not builder_run.options.debug_forced
    else:
      return not builder_run.options.debug

  def PerformStage(self):
    if self.suite_config.suite == constants.HWTEST_AFDO_SUITE:
      arch = self._GetPortageEnvVar('ARCH', self._current_board)
      cpv = portage_util.PortageqBestVisible(constants.CHROME_CP,
                                             cwd=self._build_root)
      if afdo.CheckAFDOPerfData(cpv, arch, gs.GSContext()):
        logging.info('AFDO profile already generated for arch %s '
                     'and Chrome %s. Not generating it again',
                     arch, cpv.version_no_rev.split('_')[0])
        return

    if self.suite_config.suite in [constants.HWTEST_CTS_QUAL_SUITE,
                                   constants.HWTEST_GTS_QUAL_SUITE]:
      # Increase the priority for CTS/GTS qualification suite as we want stable
      # build to have higher priority than beta build (again higher than dev).
      try:
        cros_vers = self._run.GetVersionInfo().VersionString().split('.')
        # Convert priority to corresponding integer value.
        self.suite_config.priority = constants.HWTEST_PRIORITIES_MAP[
            self.suite_config.priority]
        # We add 1/10 of the branch version to the priority. This results in a
        # modest priority bump the older the branch is. Typically beta priority
        # would be dev + [1..4] and stable priority dev + [5..9].
        self.suite_config.priority += int(math.ceil(float(cros_vers[1]) / 10.0))
      except cbuildbot_run.VersionNotSetError:
        logging.debug('Could not obtain version info. %s will use initial '
                      'priority value: %s', self.suite_config.suite,
                      self.suite_config.priority)

    build = '/'.join([self._bot_id, self.version])

    skip_duts_check = False
    if config_lib.IsCanaryType(self._run.config.build_type):
      skip_duts_check = True

    build_id, db = self._run.GetCIDBHandle()

    test_args = None
    if config_lib.IsCQType(self._run.config.build_type):
      test_args = {'fast': 'True'}

    cmd_result = commands.RunHWTestSuite(
        build, self.suite_config.suite, self._board_name,
        model=self._model,
        pool=self.suite_config.pool,
        file_bugs=self.suite_config.file_bugs,
        wait_for_results=self.wait_for_results,
        priority=self.suite_config.priority,
        timeout_mins=self.suite_config.timeout_mins,
        retry=self.suite_config.retry,
        max_retries=self.suite_config.max_retries,
        minimum_duts=self.suite_config.minimum_duts,
        suite_min_duts=self.suite_config.suite_min_duts,
        suite_args=self.suite_config.suite_args,
        offload_failures_only=self.suite_config.offload_failures_only,
        debug=not self.TestsEnabled(self._run),
        skip_duts_check=skip_duts_check,
        job_keyvals=self.GetJobKeyvals(),
        test_args=test_args)

    if config_lib.IsCQType(self._run.config.build_type):
      self.ReportHWTestResults(cmd_result.json_dump_result, build_id, db)

    if db:
      db.InsertBuildMessage(build_id, message_type=constants.SUBSYSTEMS,
                            message_subtype=constants.SUBSYSTEM_UNUSED,
                            board=self._current_board)
    if cmd_result.to_raise:
      raise cmd_result.to_raise


class SkylabHWTestStage(HWTestStage):
  """Stage that runs tests in the Autotest lab with Skylab."""

  category = constants.TEST_INFRA_STAGE

  def PerformStage(self):
    build = '/'.join([self._bot_id, self.version])

    cmd_result = commands.RunSkylabHWTestSuite(
        build, self.suite_config.suite, self._board_name,
        model=self._model,
        pool=self.suite_config.pool,
        wait_for_results=self.wait_for_results,
        priority=self.suite_config.priority,
        timeout_mins=self.suite_config.timeout_mins,
        retry=self.suite_config.retry,
        max_retries=self.suite_config.max_retries,
        suite_args=self.suite_config.suite_args)

    if cmd_result.to_raise:
      raise cmd_result.to_raise


class ASyncHWTestStage(HWTestStage, generic_stages.ForgivingBuilderStage):
  """Stage that fires and forgets hw test suites to the Autotest lab."""

  stage_name = "ASyncHWTest"
  category = constants.TEST_INFRA_STAGE

  def __init__(self, *args, **kwargs):
    super(ASyncHWTestStage, self).__init__(*args, **kwargs)
    self.wait_for_results = False


class ASyncSkylabHWTestStage(SkylabHWTestStage,
                             generic_stages.ForgivingBuilderStage):
  """Stage that fires and forgets skylab hw test suites to the Autotest lab."""

  stage_name = "ASyncSkylabHWTest"
  category = constants.TEST_INFRA_STAGE

  def __init__(self, *args, **kwargs):
    super(ASyncSkylabHWTestStage, self).__init__(*args, **kwargs)
    self.wait_for_results = False


class ImageTestStage(generic_stages.BoardSpecificBuilderStage,
                     generic_stages.ArchivingStageMixin):
  """Stage that launches tests on the produced disk image."""

  option_name = 'image_test'
  config_name = 'image_test'
  category = constants.CI_INFRA_STAGE

  # Give the tests 60 minutes to run. Image tests should be really quick but
  # the umount/rmdir bug (see osutils.UmountDir) may take a long time.
  IMAGE_TEST_TIMEOUT = 60 * 60

  def __init__(self, *args, **kwargs):
    super(ImageTestStage, self).__init__(*args, **kwargs)

  def PerformStage(self):
    test_results_dir = commands.CreateTestRoot(self._build_root)
    # CreateTestRoot returns a temp directory inside chroot.
    # We bring that back out to the build root.
    test_results_dir = os.path.join(self._build_root, test_results_dir[1:])
    test_results_dir = os.path.join(test_results_dir, 'image_test_results')
    osutils.SafeMakedirs(test_results_dir)
    try:
      with timeout_util.Timeout(self.IMAGE_TEST_TIMEOUT):
        commands.RunTestImage(
            self._build_root,
            self._current_board,
            self.GetImageDirSymlink(),
            test_results_dir,
        )
    finally:
      self.SendPerfValues(test_results_dir)

  def SendPerfValues(self, test_results_dir):
    """Gather all perf values in |test_results_dir| and send them to chromeperf.

    The uploading will be retried 3 times for each file.

    Args:
      test_results_dir: A path to the directory with perf files.
    """
    # A dict of list of perf values, keyed by test name.
    perf_entries = collections.defaultdict(list)
    for root, _, filenames in os.walk(test_results_dir):
      for relative_name in filenames:
        if not image_test_lib.IsPerfFile(relative_name):
          continue
        full_name = os.path.join(root, relative_name)
        entries = perf_uploader.LoadPerfValues(full_name)
        test_name = image_test_lib.ImageTestCase.GetTestName(relative_name)
        perf_entries[test_name].extend(entries)

    platform_name = self._run.bot_id
    try:
      cros_ver = self._run.GetVersionInfo().VersionString()
    except cbuildbot_run.VersionNotSetError:
      logging.error('Could not obtain version info. '
                    'Failed to upload perf results.')
      return

    chrome_ver = self._run.DetermineChromeVersion()
    for test_name, perf_values in perf_entries.iteritems():
      self._UploadPerfValues(perf_values, platform_name, test_name,
                             cros_version=cros_ver,
                             chrome_version=chrome_ver)


class BinhostTestStage(generic_stages.BuilderStage):
  """Stage that verifies Chrome prebuilts."""

  config_name = 'binhost_test'
  category = constants.CI_INFRA_STAGE

  def PerformStage(self):
    # Verify our binhosts.
    # Don't check for incremental compatibility when we uprev chrome.
    incremental = not (self._run.config.chrome_rev or
                       self._run.options.chrome_rev)
    commands.RunBinhostTest(self._build_root, incremental=incremental)


class BranchUtilTestStage(generic_stages.BuilderStage):
  """Stage that verifies branching works on the latest manifest version."""

  config_name = 'branch_util_test'
  category = constants.CI_INFRA_STAGE

  def PerformStage(self):
    assert (hasattr(self._run.attrs, 'manifest_manager') and
            self._run.attrs.manifest_manager is not None), \
        'Must run ManifestVersionedSyncStage before this stage.'
    manifest_manager = self._run.attrs.manifest_manager

    args = [
        '--branch-name', 'test_branch',
        '--version', manifest_manager.GetCurrentVersionInfo().VersionString(),
    ]

    if self._run.options.git_cache_dir:
      args.extend(['--git-cache-dir', self._run.options.git_cache_dir])

    commands.RunLocalTryjob(self._build_root, 'branch-util-tryjob', args)


class CrosSigningTestStage(generic_stages.BuilderStage):
  """Stage that runs the signer unittests.

  This requires an internal source code checkout.
  """

  category = constants.CI_INFRA_STAGE

  def PerformStage(self):
    """Run the cros-signing unittests."""
    commands.RunCrosSigningTests(self._build_root)


class UnexpectedTryjobResult(Exception):
  """Thrown if a nested tryjob passes or fails unexpectedly."""

class CbuildbotLaunchTestBuildStage(generic_stages.BuilderStage):
  """Perform a single build with cbuildbot_launch."""
  category = constants.CI_INFRA_STAGE

  def __init__(self, builder_run, tryjob_buildroot, branch, build_config,
               expect_success=True, **kwargs):
    """Init.

    Args:
      builder_run: See builder_run on ArchiveStage
      tryjob_buildroot: buildroot to use for test build, NOT current build.
      branch: Branch to build. None means 'current' branch.
      build_config: Name of build config to build.
      expect_success: Is the test build expected to pass?
    """
    super(CbuildbotLaunchTestBuildStage, self).__init__(builder_run, **kwargs)

    self.build_config = build_config
    self.tryjob_buildroot = tryjob_buildroot
    self.branch = branch
    self.expect_success = expect_success

  def PerformStage(self):
    args = ['--branch', self.branch]
    if self._run.options.git_cache_dir:
      args.extend(['--git-cache-dir', self._run.options.git_cache_dir])

    try:
      commands.RunLocalTryjob(
          self._build_root, self.build_config, args, self.tryjob_buildroot)
      if not self.expect_success:
        raise UnexpectedTryjobResult('Build passed unexpectedly.')
    except failures_lib.BuildScriptFailure:
      if self.expect_success:
        raise UnexpectedTryjobResult('Build failed unexpectedly.')


class CbuildbotLaunchTestStage(generic_stages.BuilderStage):
  """Stage that runs Chromite tests, including network tests."""
  category = constants.CI_INFRA_STAGE

  def __init__(self, builder_run, **kwargs):
    """Init.

    Args:
      builder_run: See builder_run on ArchiveStage
    """
    super(CbuildbotLaunchTestStage, self).__init__(builder_run, **kwargs)
    self.tryjob_buildroot = None


  def RunCbuildbotLauncher(
      self, suffix, branch, build_config, expect_success):
    """Run a new stage to test a cbuildbot_launch subbuild."""
    substage = CbuildbotLaunchTestBuildStage(
        self._run,
        tryjob_buildroot=self.tryjob_buildroot,
        branch=branch,
        build_config=build_config,
        expect_success=expect_success,
        suffix=suffix)
    substage.Run()

  def PerformStage(self):
    """Run sample cbuildbot_launch tests."""
    # TODO: Move this tempdir, it's a problem.
    with osutils.TempDir() as tryjob_buildroot:
      self.tryjob_buildroot = tryjob_buildroot

      # Iniitial build fails.
      self.RunCbuildbotLauncher(
          'Initial Build (fail)', self._run.options.branch, 'fail-build',
          expect_success=False)

      # Test cleanup after a fail.
      self.RunCbuildbotLauncher(
          'Second Build (pass)', self._run.options.branch, 'success-build',
          expect_success=True)

      # Test reduced cleanup after a pass.
      self.RunCbuildbotLauncher(
          'Third Build (pass)', self._run.options.branch, 'success-build',
          expect_success=True)

      # Test branch transition.
      self.RunCbuildbotLauncher(
          'Branch Build (pass)', 'release-R68-10718.B', 'success-build',
          expect_success=True)


class ChromiteTestStage(generic_stages.BuilderStage):
  """Stage that runs Chromite tests, including network tests."""

  category = constants.CI_INFRA_STAGE

  def PerformStage(self):
    """Run the chromite unittests."""
    buildroot_chromite = path_util.ToChrootPath(
        os.path.join(self._build_root, 'chromite'))

    cmd = [
        os.path.join(buildroot_chromite, 'run_tests'),
        # TODO(crbug.com/682381): When tests can pass, add '--network',
    ]
    # TODO: Remove enter_chroot=True when we have virtualenv support.
    # Until then, we skip all chromite tests outside the chroot.
    cros_build_lib.RunCommand(cmd, enter_chroot=True)


class CidbIntegrationTestStage(generic_stages.BuilderStage):
  """Stage that runs the CIDB integration tests."""

  category = constants.CI_INFRA_STAGE

  def PerformStage(self):
    """Run the CIDB integration tests."""
    buildroot_chromite = path_util.ToChrootPath(
        os.path.join(self._build_root, 'chromite'))

    cmd = [
        os.path.join(buildroot_chromite, 'lib', 'cidb_integration_test'),
        '-v',
        # '--network'  Doesn't work in a build, yet.
    ]
    cros_build_lib.RunCommand(cmd, enter_chroot=True)


class DebugInfoTestStage(generic_stages.BoardSpecificBuilderStage,
                         generic_stages.ForgivingBuilderStage):
  """Perform tests that are based on debug info

  Tests may include, for example,
    * whether dwarf info exists
    * whether clang is used
    * whether FORTIFY is enabled, etc.
  """

  category = constants.CI_INFRA_STAGE

  def PerformStage(self):
    cmd = ['debug_info_test',
           os.path.join(cros_build_lib.GetSysroot(board=self._current_board),
                        'usr/lib/debug')]
    cros_build_lib.RunCommand(cmd, enter_chroot=True)
