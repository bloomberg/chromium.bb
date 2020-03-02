# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""An executable function cros-update for auto-update of a CrOS host.

The reason to create this file is to let devserver to trigger a background
process for CrOS auto-update. Therefore, when devserver service is restarted
sometimes, the CrOS auto-update process is still running and the corresponding
provision task won't claim failure.

It includes two classes:
  a. CrOSUpdateTrigger:
    1. Includes all logics which identify which types of update need to be
       performed in the current DUT.
    2. Responsible for write current status of CrOS auto-update process into
       progress_tracker.

  b. CrOSAUParser:
    1. Pre-setup the required args for CrOS auto-update.
    2. Parse the input parameters for cmd that runs 'cros_update.py'.
"""

from __future__ import print_function

import os
import re
import time
import traceback

from chromite.lib import auto_updater
from chromite.lib import commandline
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import cros_update_logging
from chromite.lib import cros_update_progress
from chromite.lib import remote_access
from chromite.lib import timeout_util


# The build channel for recovering host's stateful partition.
STABLE_BUILD_CHANNEL = 'stable-channel'

# Timeout for CrOS auto-update process.
CROS_UPDATE_TIMEOUT_MIN = 30

# The preserved path in remote device, won't be deleted after rebooting.
CROS_PRESERVED_PATH = ('/mnt/stateful_partition/unencrypted/'
                       'preserve/cros-update')

# Standard error tmeplate to be written into status tracking log.
CROS_ERROR_TEMPLATE = cros_update_progress.ERROR_TAG + ' %s'

# How long after a quick provision fails to wait before falling back to the
# standard provisioning flow.
QUICK_PROVISION_FAILURE_DELAY_SEC = 45

# Setting logging level
logConfig = cros_update_logging.loggingConfig()
logConfig.ConfigureLogging()


class CrOSUpdateTrigger(object):
  """The class for CrOS auto-updater trigger.

  This class is used for running all CrOS auto-update trigger logic.
  """
  # The path to dev directory
  DEV_DIR = 'src/platform/dev'

  def __init__(self, host_name, build_name, static_dir, progress_tracker=None,
               log_file=None, au_tempdir=None, force_update=False,
               full_update=False, original_build=None, payload_filename=None,
               clobber_stateful=True, quick_provision=False,
               devserver_url=None, static_url=None):
    self.host_name = host_name
    self.build_name = build_name
    self.static_dir = static_dir
    self.progress_tracker = progress_tracker
    self.log_file = log_file
    self.au_tempdir = au_tempdir
    self.force_update = force_update
    self.full_update = full_update
    self.original_build = original_build
    self.payload_filename = payload_filename
    self.clobber_stateful = clobber_stateful
    self.quick_provision = quick_provision
    self.devserver_url = devserver_url
    self.static_url = static_url

  def _WriteAUStatus(self, content):
    if self.progress_tracker:
      self.progress_tracker.WriteStatus(content)

  def _StatefulUpdate(self, cros_updater):
    """The detailed process in stateful update.

    Args:
      cros_updater: The CrOS auto updater for auto-update.
    """
    self._WriteAUStatus('pre-setup stateful update')
    cros_updater.PreSetupStatefulUpdate()
    self._WriteAUStatus('perform stateful update')
    cros_updater.UpdateStateful()
    self._WriteAUStatus('post-check stateful update')
    cros_updater.PostCheckStatefulUpdate()

  def _RootfsUpdate(self, cros_updater):
    """The detailed process in rootfs update.

    Args:
      cros_updater: The CrOS auto updater for auto-update.
    """
    self._WriteAUStatus('Check whether devserver can run before rootfs update')
    cros_updater.CheckDevserverRun()
    self._WriteAUStatus('transfer rootfs update package')
    cros_updater.TransferRootfsUpdate()
    self._WriteAUStatus('pre-setup rootfs update')
    cros_updater.PreSetupRootfsUpdate()
    self._WriteAUStatus('rootfs update')
    cros_updater.UpdateRootfs()
    self._WriteAUStatus('post-check rootfs update')
    cros_updater.PostCheckRootfsUpdate()

  def _GetOriginalPayloadDir(self):
    """Get the directory of original payload.

    Returns:
      The directory of original payload, whose format is like:
          'static/stable-channel/link/3428.210.0'
    """
    if self.original_build:
      return os.path.join(self.static_dir, STABLE_BUILD_CHANNEL,
                          self.original_build)
    else:
      return None

  def _MakeStatusUrl(self, devserver_url, host_name, pid):
    """Generates a URL to post auto update status to.

    Args:
      devserver_url: URL base for devserver RPCs.
      host_name: Host to post status for.
      pid: pid of the update process.

    Returns:
      An unescaped URL.
    """
    return '%s/post_au_status?host_name=%s&pid=%d' % (devserver_url, host_name,
                                                      pid)

  def _QuickProvision(self, device):
    """Performs a quick provision of device.

    Returns:
      A dictionary of extracted key-value pairs returned from the script
      execution.

    Raises:
      cros_build_lib.RunCommandError: error executing command or script
      remote_access.SSHConnectionError: SSH connection error
    """
    pid = os.getpid()
    pgid = os.getpgid(pid)
    if self.progress_tracker is None:
      self.progress_tracker = cros_update_progress.AUProgress(self.host_name,
                                                              pgid)

    dut_script = '/usr/local/tmp/quick-provision'
    status_url = self._MakeStatusUrl(self.devserver_url, self.host_name, pgid)
    cmd = [
        # /usr/local/tmp may not exist on base images.
        'mkdir', '-p', os.path.dirname(dut_script), '&&',
        # Download the script from the devserver.
        'curl', '-o', dut_script,
        os.path.join(self.static_url, 'quick-provision'), '&&',
        # Run the script.
        'bash', dut_script, '--status_url',
        cros_build_lib.ShellQuote(status_url),
        self.build_name, self.static_url,
    ]
    # Quick provision script issues a reboot and might result in the SSH
    # connection being terminated so set ssh_error_ok so that output can
    # still be captured.
    results = device.RunCommand(
        ' '.join(cmd), log_output=True, capture_output=True,
        ssh_error_ok=True, shell=True, encoding='utf-8')
    key_re = re.compile(r'^KEYVAL: ([^\d\W]\w*)=(.*)$')
    matches = [key_re.match(l) for l in results.output.splitlines()]
    keyvals = {m.group(1): m.group(2) for m in matches if m}
    logging.info('DUT returned keyvals: %s', keyvals)

    # If there was an SSH error, check the keyvals to see if it actually
    # completed and suppress the error if so.
    if results.returncode == remote_access.SSH_ERROR_CODE:
      if 'COMPLETED' in keyvals:
        logging.warning('Quick provision completed with ssh error, ignoring...')
      else:
        logging.error('Incomplete quick provision failed with ssh error')
        raise remote_access.SSHConnectionError(results.error)

    return keyvals

  def TriggerAU(self):
    """Execute auto update for cros_host.

    The auto update includes 4 steps:
    1. if devserver cannot run, restore the stateful partition.
    2. if possible, do stateful update first, but never raise errors, except
       for timeout_util.TimeoutError caused by system.signal.
    3. If required or stateful_update fails, first do rootfs update, then do
       stateful_update.
    4. Post-check for the whole update.
    """
    try:
      with remote_access.ChromiumOSDeviceHandler(
          self.host_name, port=None,
          base_dir=CROS_PRESERVED_PATH,
          ping=False) as device:

        logging.debug('Remote device %s is connected', self.host_name)
        payload_dir = os.path.join(self.static_dir, self.build_name)
        original_payload_dir = self._GetOriginalPayloadDir()

        chromeos_AU = auto_updater.ChromiumOSUpdater(
            device, self.build_name, payload_dir,
            dev_dir=os.path.join(constants.SOURCE_ROOT, self.DEV_DIR),
            tempdir=self.au_tempdir,
            log_file=self.log_file,
            original_payload_dir=original_payload_dir,
            yes=True,
            payload_filename=self.payload_filename,
            clobber_stateful=self.clobber_stateful)

        # Allow fall back if the quick provision does not succeed.
        invoke_autoupdate = True

        if (self.quick_provision and self.clobber_stateful and
            not self.full_update):
          try:
            logging.debug('Start CrOS quick provision process...')
            self._WriteAUStatus('Start Quick Provision')
            keyvals = self._QuickProvision(device)
            logging.debug('Start CrOS check process...')
            self._WriteAUStatus('Finish Quick Provision, reboot')
            chromeos_AU.AwaitReboot(keyvals.get('BOOT_ID'))
            self._WriteAUStatus('Finish Quick Provision, post-check')
            chromeos_AU.PostCheckCrOSUpdate()
            self._WriteAUStatus(cros_update_progress.FINISHED)
            invoke_autoupdate = False
          except (cros_build_lib.RunCommandError,
                  remote_access.SSHConnectionError,
                  auto_updater.RebootVerificationError) as e:
            logging.warning(
                'Error during quick provision, falling back to legacy: %s: %s',
                type(e).__name__, e)
            time.sleep(QUICK_PROVISION_FAILURE_DELAY_SEC)

        if invoke_autoupdate:
          chromeos_AU.CheckPayloads()

          version_match = chromeos_AU.PreSetupCrOSUpdate()
          self._WriteAUStatus('Transfer Devserver/Stateful Update Package')
          chromeos_AU.TransferDevServerPackage()
          chromeos_AU.TransferStatefulUpdate()

          restore_stateful = chromeos_AU.CheckRestoreStateful()
          do_stateful_update = (not self.full_update and
                                version_match and self.force_update)
          stateful_update_complete = False
          logging.debug('Start CrOS update process...')
          try:
            if restore_stateful:
              self._WriteAUStatus('Restore Stateful Partition')
              chromeos_AU.RestoreStateful()
              stateful_update_complete = True
            else:
              # Whether to execute stateful update depends on:
              # a. full_update=False: No full reimage is required.
              # b. The update version is matched to the current version, And
              #    force_update=True: Update is forced even if the version
              #    installed is the same.
              if do_stateful_update:
                self._StatefulUpdate(chromeos_AU)
                stateful_update_complete = True

          except timeout_util.TimeoutError:
            raise
          except Exception as e:
            logging.debug('Error happens in stateful update: %r', e)

          # Whether to execute rootfs update depends on:
          # a. stateful update is not completed, or completed by
          #    update action 'restore_stateful'.
          # b. force_update=True: Update is forced no matter what the current
          #    version is. Or, the update version is not matched to the current
          #    version.
          require_rootfs_update = self.force_update or (
              not chromeos_AU.CheckVersion())
          if (not (do_stateful_update and stateful_update_complete)
              and require_rootfs_update):
            self._RootfsUpdate(chromeos_AU)
            self._StatefulUpdate(chromeos_AU)

          self._WriteAUStatus('post-check for CrOS auto-update')
          chromeos_AU.PostCheckCrOSUpdate()
          self._WriteAUStatus(cros_update_progress.FINISHED)

        logging.debug('Provision successfully completed (%s)',
                      'legacy' if invoke_autoupdate else 'quick provision')
    except Exception as e:
      logging.debug('Error happens in CrOS auto-update: %r', e)
      self._WriteAUStatus(CROS_ERROR_TEMPLATE % str(traceback.format_exc()))
      raise


def ParseArguments(argv):
  """Returns a namespace for the CLI arguments."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--hostname', type=str, dest='host_name',
                      help='host_name of a DUT')
  parser.add_argument('-b', type=str, dest='build_name',
                      help='build name to be auto-updated')
  parser.add_argument('--static_dir', type='path',
                      help='static directory of the devserver')
  parser.add_argument('--force_update', action='store_true', default=False,
                      help='force an update even if the version installed is '
                           'the same')
  parser.add_argument('--full_update', action='store_true', default=False,
                      help='force a rootfs update, skip stateful update')
  parser.add_argument('--original_build', type=str, default='',
                      help=('force stateful update with the same version of '
                            'previous rootfs partition'))
  parser.add_argument('--payload_filename', type=str,
                      help='A custom payload filename')
  parser.add_argument('--clobber_stateful', action='store_true', default=False,
                      help='Whether to clobber stateful')
  parser.add_argument('--quick_provision', action='store_true', default=False,
                      help='Whether to attempt quick provisioning path')
  parser.add_argument('--devserver_url', type=str,
                      help='Devserver URL base for RPCs')
  parser.add_argument('--static_url', type=str,
                      help='Devserver URL base for static files')

  opts = parser.parse_args(argv)
  opts.Freeze()

  return opts


def main(argv):
  options = ParseArguments(argv)

  # Use process group id as the unique id in track and log files, since
  # os.setsid is executed before the current process is run.
  pid = os.getpid()
  pgid = os.getpgid(pid)

  # Setting log files for CrOS auto-update process.
  # Log file:  file to record every details of CrOS auto-update process.
  log_file = cros_update_progress.GetExecuteLogFile(options.host_name, pgid)
  logging.info('Writing executing logs into file: %s', log_file)
  logConfig.SetFileHandler(log_file)

  # Create a progress_tracker for tracking CrOS auto-update progress.
  progress_tracker = cros_update_progress.AUProgress(options.host_name, pgid)

  # Create a dir for temporarily storing devserver codes and logs.
  au_tempdir = cros_update_progress.GetAUTempDirectory(options.host_name, pgid)

  # Create cros_update instance to run CrOS auto-update.
  cros_updater_trigger = CrOSUpdateTrigger(
      options.host_name, options.build_name, options.static_dir,
      progress_tracker=progress_tracker,
      log_file=log_file,
      au_tempdir=au_tempdir,
      force_update=options.force_update,
      full_update=options.full_update,
      original_build=options.original_build,
      payload_filename=options.payload_filename,
      clobber_stateful=options.clobber_stateful,
      quick_provision=options.quick_provision,
      devserver_url=options.devserver_url,
      static_url=options.static_url)

  # Set timeout the cros-update process.
  try:
    with timeout_util.Timeout(CROS_UPDATE_TIMEOUT_MIN * 60):
      cros_updater_trigger.TriggerAU()
  except timeout_util.TimeoutError as e:
    error_msg = ('%s. The CrOS auto-update process is timed out, thus will be '
                 'terminated' % str(e))
    progress_tracker.WriteStatus(CROS_ERROR_TEMPLATE % error_msg)
