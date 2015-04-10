# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility library for running a startup profile on an Android device.

Sets up a device for cygprofile, disables sandboxing permissions, and sets up
support for web page replay, device forwarding, and fake certificate authority
to make runs repeatable.
"""

import logging
import os
import shutil
import subprocess
import sys
import time

sys.path.append(os.path.join(sys.path[0], '..', '..', 'build', 'android'))
from pylib import android_commands
from pylib import constants
from pylib import flag_changer
from pylib.device import device_errors
from pylib.device import device_utils
from pylib.device import intent


class NoCyglogDataError(Exception):
  """An error used to indicate that no cyglog data was collected."""

  def __init__(self, value):
    super(NoCyglogDataError, self).__init__()
    self.value = value

  def __str__(self):
    return repr(self.value)


class AndroidProfileTool(object):
  """ A utility for generating cygprofile data for chrome on andorid.

  Runs cygprofile_unittest found in output_directory, does profiling runs,
  and pulls the data to the local machine in output_directory/cyglog_data.
  """

  _DEVICE_CYGLOG_DIR = '/data/local/tmp/chrome/cyglog'
  _TEST_URL = 'https://www.google.com/#hl=en&q=science'

  def __init__(self, output_directory):
    devices = android_commands.GetAttachedDevices()
    self._device = device_utils.DeviceUtils(devices[0])
    self._cygprofile_tests = os.path.join(
        output_directory, 'cygprofile_unittests')
    self._host_cyglog_dir = os.path.join(
        output_directory, 'cyglog_data')
    self._SetUpDevice()

  def RunCygprofileTests(self):
    """ Run the cygprofile unit tests suite on the device.

    Args:
      path_to_tests: The location on the host machine with the compiled
                     cygprofile test binary.
    Returns:
      The exit code for the tests.
    """
    device_path = '/data/local/tmp/cygprofile_unittests'
    self._device.old_interface.PushIfNeeded(
        self._cygprofile_tests, device_path)
    (exit_code, _) = (
        self._device.old_interface.GetShellCommandStatusAndOutput(
            command=device_path, log_result=True))
    return exit_code

  def CollectProfile(self, apk, package_info):
    """ Run a profile and collect the log files.

    Args:
      apk: The location of the chrome apk to profile.
      package_info: A PackageInfo structure describing the chrome apk,
                    as from pylib/constants.
    Returns:
      A list of cygprofile data files.
    Raises:
      NoCyglogDataError: No data was found on the device.
    """
    self._Install(apk, package_info)
    self._SetChromeFlags(package_info)
    self._SetUpDeviceFolders()
    # Start up chrome once with a blank page, just to get the one-off
    # activities out of the way such as apk resource extraction and profile
    # creation.
    self._StartChrome(package_info, 'about:blank')
    time.sleep(15)
    self._KillChrome(package_info)
    self._SetUpDeviceFolders()
    # TODO(azarchs): Set up WPR, device forwarding, test CA.
    self._StartChrome(package_info, self._TEST_URL)
    time.sleep(60)
    self._KillChrome(package_info)
    data = self._PullCyglogData()
    self._DeleteDeviceData()
    return data

  def Cleanup(self):
    """ Delete all local and device files left over from profiling. """
    self._DeleteDeviceData()
    self._DeleteHostData()

  def _Install(self, apk, package_info):
    """Installs Chrome.apk on the device.
    Args:
      apk: The location of the chrome apk to profile.
      package_info: A PackageInfo structure describing the chrome apk,
                    as from pylib/constants.
    """
    print 'Installing apk...'
    self._device.old_interface.ManagedInstall(apk, package_info.package)

  def _SetUpDevice(self):
    """ When profiling, files are output to the disk by every process.  This
    means running without sandboxing enabled.
    """
    # We need to have adb root in order to pull cyglog data
    try:
      print 'Enabling root...'
      self._device.EnableRoot()
      # SELinux need to be in permissive mode, otherwise the process cannot
      # write the log files.
      print 'Putting SELinux in permissive mode...'
      self._device.old_interface.RunShellCommand('setenforce 0')
    except device_errors.CommandFailedError as e:
      # TODO(jbudorick) Handle this exception appropriately once interface
      #                 conversions are finished.
      logging.error(str(e))

  def _SetChromeFlags(self, package_info):
    print 'Setting flags...'
    changer = flag_changer.FlagChanger(
        self._device, package_info.cmdline_file)
    changer.AddFlags(['--no-sandbox', '--disable-fre'])
    # TODO(azarchs): backup and restore flags.
    logging.warning('Chrome flags changed and will not be restored.')

  def _SetUpDeviceFolders(self):
    """ Creates folders on the device to store cyglog data. """
    print 'Setting up device folders...'
    self._DeleteDeviceData()
    self._device.old_interface.RunShellCommand(
        'mkdir -p %s' % self._DEVICE_CYGLOG_DIR)

  def _DeleteDeviceData(self):
    """ Clears out cyglog storage locations on the device. """
    self._device.old_interface.RunShellCommand(
        'rm -rf %s' % self._DEVICE_CYGLOG_DIR)

  def _StartChrome(self, package_info, url):
    print 'Launching chrome...'
    self._device.StartActivity(
        intent.Intent(package=package_info.package,
                      activity=package_info.activity,
                      data=url,
                      extras={'create_new_tab' : True}),
        blocking=True, force_stop=True)

  def _KillChrome(self, package_info):
    self._device.KillAll(package_info.package)

  def _DeleteHostData(self):
    """
    Clears out cyglog storage locations on the host.
    """
    shutil.rmtree(self._host_cyglog_dir, ignore_errors=True)

  def _SetUpHostFolders(self):
    self._DeleteHostData()
    os.mkdir(self._host_cyglog_dir)

  def _PullCyglogData(self):
    """ Pull the cyglog data off of the device.

    Returns:
      A list of cyglog data files which were pulled.
    Raises:
      NoCyglogDataError: No data was found on the device.
    """
    print 'Pulling cyglog data...'
    self._SetUpHostFolders()
    self._device.old_interface.Adb().Pull(
        self._DEVICE_CYGLOG_DIR, self._host_cyglog_dir)
    files = os.listdir(self._host_cyglog_dir)

    if len(files) == 0:
      raise NoCyglogDataError('No cyglog data was collected')

    return [os.path.join(self._host_cyglog_dir, x) for x in files]
