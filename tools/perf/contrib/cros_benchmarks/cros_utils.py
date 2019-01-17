# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import time


def _RunCommand(dut_ip, cmd):
  """Runs command on the DUT.

  Args:
    dut_ip: IP of the DUT. If it's None, runs command locally.
    cmd: The command to run.
  """
  if dut_ip:
    # Commands via ssh shall be double-quoted to escape special characters.
    cmd = '"%s"' % cmd
    return os.system('ssh root@%s %s' % (dut_ip, cmd))
  else:
    return os.system(cmd)


def _Popen(dut_ip, cmd_list):
  """Popen to run a command on DUT."""
  if dut_ip:
    cmd_list = ['ssh', 'root@' + dut_ip] + cmd_list
  return subprocess.Popen(cmd_list, stdout=subprocess.PIPE)


def _CopyToDUT(dut_ip, local_path, dut_path):
  """Copies file to the DUT path."""
  if dut_ip:
    os.system('scp -q %s root@%s:%s' % (local_path, dut_ip, dut_path))
  else:
    os.system('cp %s %s' % (local_path, dut_path))


class KeyboardEmulator(object):
  """Sets up a remote emulated keyboard and sends key events to switch tab.

  Example usage:
    with KeyboardEmulator(DUT_IP) as kbd:
      for i in range(5):
        kbd.SwitchTab()
  """
  REMOTE_TEMP_DIR = '/usr/local/tmp/'
  REMOTE_LOG_KEY_FILENAME = 'log_key_tab_switch'
  REMOTE_KEY_PROP_FILENAME = 'keyboard.prop'

  def __init__(self, dut_ip):
    """Inits KeyboardEmulator.

    Args:
      dut_ip: DUT IP or hostname.
    """
    self._dut_ip = dut_ip
    self._key_device_name = None

  def _StartRemoteKeyboardEmulator(self):
    """Starts keyboard emulator on the DUT.

    Returns:
      The device name of the emulated keyboard.

    Raises:
      RuntimeError: Keyboard emulation failed.
    """
    kbd_prop_filename = os.path.join(KeyboardEmulator.REMOTE_TEMP_DIR,
                                     KeyboardEmulator.REMOTE_KEY_PROP_FILENAME)

    ret = _RunCommand(self._dut_ip, 'test -e %s' % kbd_prop_filename)
    if ret != 0:
      raise RuntimeError('Keyboard property file does not exist.')

    process = _Popen(self._dut_ip, ['evemu-device', kbd_prop_filename])

    # The evemu-device output format:
    # Emulated Keyboard: /dev/input/event10
    output = process.stdout.readline()
    if self._dut_ip:
      # The remote process would live when the ssh process was terminated.
      process.kill()
    else:
      # Needs extra sleep when running locally, otherwise the first key may not
      # dispatch.
      # TODO(cywang): crbug.com/852702
      time.sleep(1)
    if not output.startswith('Emulated Keyboard:'):
      raise RuntimeError('Keyboard emulation failed.')
    key_device_name = output.split()[2]
    return key_device_name

  def _SetupKeyDispatch(self):
    """Uploads required files to emulate keyboard."""
    cur_dir = os.path.dirname(os.path.abspath(__file__))
    for filename in (KeyboardEmulator.REMOTE_KEY_PROP_FILENAME,
                     KeyboardEmulator.REMOTE_LOG_KEY_FILENAME):
      src = os.path.join(cur_dir, 'data', filename)
      dest = os.path.join(KeyboardEmulator.REMOTE_TEMP_DIR, filename)
      _CopyToDUT(self._dut_ip, src, dest)

  def __enter__(self):
    self._SetupKeyDispatch()
    self._key_device_name = self._StartRemoteKeyboardEmulator()
    return self

  def SwitchTab(self):
    """Sending Ctrl-tab key to trigger tab switching."""
    log_key_filename = os.path.join(KeyboardEmulator.REMOTE_TEMP_DIR,
                                    KeyboardEmulator.REMOTE_LOG_KEY_FILENAME)
    cmd = ('evemu-play --insert-slot0 %s < %s' %
           (self._key_device_name, log_key_filename))
    _RunCommand(self._dut_ip, cmd)

  def __exit__(self, exc_type, exc_value, traceback):
    # Kills the emulator process explicitly.
    _RunCommand(self._dut_ip, 'pkill evemu-device')


def NoScreenOff(dut_ip):
  """Sets screen always on for 1 hour.

  Args:
    dut_ip: DUT IP or hostname.
  """
  _RunCommand(dut_ip, 'set_power_policy --ac_screen_off_delay=3600')
  _RunCommand(dut_ip, 'set_power_policy --ac_screen_dim_delay=3600')
