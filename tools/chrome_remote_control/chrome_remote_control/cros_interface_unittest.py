# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(nduca): Rewrite what some of these tests to use mocks instead of
# actually talking to the device. This would improve our coverage quite
# a bit.
import unittest

import browser_options
import cros_interface
import run_tests

class CrOSInterfaceTest(unittest.TestCase):
  @run_tests.RequiresBrowserOfType('cros-chrome')
  def testDeviceSideProcessFailureToLaunch(self):
    remote = browser_options.options_for_unittests.cros_remote
    cri = cros_interface.CrOSInterface(remote)

    def WillFail():
      dsp = cros_interface.DeviceSideProcess(
        cri,
        ['sfsdfskjflwejfweoij'])
      dsp.Close()
    self.assertRaises(OSError, WillFail)

  @run_tests.RequiresBrowserOfType('cros-chrome')
  def testDeviceSideProcessCloseDoesClose(self):
    remote = browser_options.options_for_unittests.cros_remote
    cri = cros_interface.CrOSInterface(remote)

    with cros_interface.DeviceSideProcess(
        cri,
        ['sleep', '111']) as dsp:
      procs = cri.ListProcesses()
      sleeps = [x for x in procs
                if x[1] == 'sleep 111']
      assert dsp.IsAlive()
    procs = cri.ListProcesses()
    sleeps = [x for x in procs
              if x[1] == 'sleep 111']
    self.assertEquals(len(sleeps), 0)

  @run_tests.RequiresBrowserOfType('cros-chrome')
  def testPushContents(self):
    remote = browser_options.options_for_unittests.cros_remote
    cri = cros_interface.CrOSInterface(remote)
    cri.GetCmdOutput(['rm', '-rf', '/tmp/testPushContents'])
    cri.PushContents('hello world', '/tmp/testPushContents')
    contents = cri.GetFileContents('/tmp/testPushContents')
    self.assertEquals(contents, 'hello world')

  @run_tests.RequiresBrowserOfType('cros-chrome')
  def testExists(self):
    remote = browser_options.options_for_unittests.cros_remote
    cri = cros_interface.CrOSInterface(remote)
    self.assertTrue(cri.FileExistsOnDevice('/proc/cpuinfo'))
    self.assertTrue(cri.FileExistsOnDevice('/etc/passwd'))
    self.assertFalse(cri.FileExistsOnDevice('/etc/sdlfsdjflskfjsflj'))

  @run_tests.RequiresBrowserOfType('cros-chrome')
  def testGetFileContents(self):
    remote = browser_options.options_for_unittests.cros_remote
    cri = cros_interface.CrOSInterface(remote)
    hosts = cri.GetFileContents('/etc/hosts')
    assert hosts.startswith('# /etc/hosts')

  @run_tests.RequiresBrowserOfType('cros-chrome')
  def testGetFileContentsForSomethingThatDoesntExist(self):
    remote = browser_options.options_for_unittests.cros_remote
    cri = cros_interface.CrOSInterface(remote)
    self.assertRaises(
      OSError,
      lambda: cri.GetFileContents('/tmp/209fuslfskjf/dfsfsf'))

  @run_tests.RequiresBrowserOfType('cros-chrome')
  def testListProcesses(self):
    remote = browser_options.options_for_unittests.cros_remote
    cri = cros_interface.CrOSInterface(remote)

    with cros_interface.DeviceSideProcess(
        cri,
        ['sleep', '11']) as sleep:
      procs = cri.ListProcesses()
      sleeps = [x for x in procs
                if x[1] == 'sleep 11']

      assert len(sleeps) == 1

  @run_tests.RequiresBrowserOfType('cros-chrome')
  def testIsServiceRunning(self):
    remote = browser_options.options_for_unittests.cros_remote
    cri = cros_interface.CrOSInterface(remote)

    self.assertTrue(cri.IsServiceRunning('openssh-server'))

