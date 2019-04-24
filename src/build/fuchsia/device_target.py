# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements commands for running and interacting with Fuchsia on devices."""

import boot_data
import filecmp
import logging
import os
import subprocess
import sys
import target
import tempfile
import time
import uuid

from common import SDK_ROOT, EnsurePathExists

CONNECT_RETRY_COUNT = 20
CONNECT_RETRY_WAIT_SECS = 1

# Number of failed connection attempts before redirecting system logs to stdout.
CONNECT_RETRY_COUNT_BEFORE_LOGGING = 10

TARGET_HASH_FILE_PATH = '/data/.hash'

class DeviceTarget(target.Target):
  def __init__(self, output_dir, target_cpu, host=None, port=None,
               ssh_config=None, system_log_file=None):
    """output_dir: The directory which will contain the files that are
                   generated to support the deployment.
    target_cpu: The CPU architecture of the deployment target. Can be
                "x64" or "arm64".
    host: The address of the deployment target device.
    port: The port of the SSH service on the deployment target device.
    ssh_config: The path to SSH configuration data."""

    super(DeviceTarget, self).__init__(output_dir, target_cpu)

    self._port = 22
    self._auto = not ssh_config
    self._new_instance = True
    self._system_log_file = system_log_file
    self._loglistener = None

    if self._auto:
      boot_data.ProvisionSSH(output_dir)
      self._ssh_config_path = boot_data.GetSSHConfigPath(output_dir)
    else:
      self._ssh_config_path = os.path.expanduser(ssh_config)
      self._host = host
      if port:
        self._port = port
      self._new_instance = False

  def __exit__(self, exc_type, exc_val, exc_tb):
    if self._loglistener:
      self._loglistener.kill()

  def _SDKHashMatches(self):
    """Checks if /data/.hash on the device matches SDK_ROOT/.hash.

    Returns True if the files are identical, or False otherwise.
    """
    with tempfile.NamedTemporaryFile() as tmp:
      try:
        self.GetFile(TARGET_HASH_FILE_PATH, tmp.name)
      except subprocess.CalledProcessError:
        # If the file is unretrievable for whatever reason, assume mismatch.
        return False

      return filecmp.cmp(tmp.name, os.path.join(SDK_ROOT, '.hash'), False)

  def __Discover(self, node_name):
    """Returns the IP address and port of a Fuchsia instance discovered on
    the local area network. If |node_name| is None then the first discovered
    device is returned. """

    # TODO(crbug.com/929874): Use the nodename when it's respected by Zircon.
    dev_finder_path = os.path.join(SDK_ROOT, 'tools', 'dev_finder')
    command = [dev_finder_path, 'list']
    logging.debug(' '.join(command))
    proc = subprocess.Popen(command,
                            stdout=subprocess.PIPE,
                            stderr=open(os.devnull, 'w'))
    proc.wait()
    if proc.returncode == 0:
      entries = proc.stdout.readlines()
      host = entries[0].strip()

      if len(entries) > 1:
        logging.warn('Found more than one device on the network:')
        logging.warn(' , '.join(set(map(lambda x: x.strip(), entries))))
        logging.warn('Automatically using the first device in the list.')

      return host

    return None

  def Start(self):
    if self._auto:
      logging.debug('Starting automatic device deployment.')
      node_name = boot_data.GetNodeName(self._output_dir)
      self._host = self.__Discover(node_name)
      if self._host and self._WaitUntilReady(retries=0):
        if not self._SDKHashMatches():
          logging.info('SDK hash does not match, rebooting.')
          self.RunCommand(['dm', 'reboot'])
          self._started = False
        else:
          logging.info('Connected to an already booted device.')
          self._new_instance = False
          return

      self.__ProvisionDevice(node_name)

    else:
      if not self._host:
        self._host = self.__Discover(node_name=None)
        if not self._host:
          raise Exception('No Fuchsia devices found.')

      if not self._WaitUntilReady(retries=0):
        raise Exception('Could not conenct to %s' % self._host)

  def __ProvisionDevice(self, node_name):
    logging.info('Netbooting Fuchsia. ' +
                 'Please ensure that your device is in bootloader mode.')
    bootserver_path = os.path.join(SDK_ROOT, 'tools', 'bootserver')
    bootserver_command = [
        bootserver_path,
        '-1',
        '--fvm',
        EnsurePathExists(boot_data.GetTargetFile(self._GetTargetSdkArch(),
                                                 'fvm.sparse.blk')),
        EnsurePathExists(boot_data.GetBootImage(self._output_dir,
                                                self._GetTargetSdkArch()))]

    if self._GetTargetSdkArch() == 'x64':
      bootserver_command += [
          '--efi',
          EnsurePathExists(boot_data.GetTargetFile(self._GetTargetSdkArch(),
                                                   'local.esp.blk'))]

    bootserver_command += ['--']
    bootserver_command += boot_data.GetKernelArgs(self._output_dir)

    logging.debug(' '.join(bootserver_command))
    subprocess.check_call(bootserver_command)

    # Start loglistener to save system logs.
    if self._system_log_file:
      loglistener_path = os.path.join(SDK_ROOT, 'tools', 'loglistener')
      self._loglistener = subprocess.Popen(
          [loglistener_path, node_name],
          stdout=self._system_log_file,
          stderr=subprocess.STDOUT, stdin=open(os.devnull))

    logging.debug('Waiting for device to join network.')
    for retry in xrange(CONNECT_RETRY_COUNT):
      self._host = self.__Discover(node_name)
      if self._host:
        break
      time.sleep(CONNECT_RETRY_WAIT_SECS)
    if not self._host:
      raise Exception('Couldn\'t connect to device.')

    logging.debug('host=%s, port=%d' % (self._host, self._port))

    self._WaitUntilReady();

    # Update the target's hash to match the current tree's.
    self.PutFile(os.path.join(SDK_ROOT, '.hash'), TARGET_HASH_FILE_PATH)

  def IsNewInstance(self):
    return self._new_instance

  def _GetEndpoint(self):
    return (self._host, self._port)

  def _GetSshConfigPath(self):
    return self._ssh_config_path
