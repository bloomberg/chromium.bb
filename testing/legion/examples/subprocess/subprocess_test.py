#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A host test module demonstrating interacting with remote subprocesses."""

# Map the legion directory so we can import the host controller.
import sys
sys.path.append('../../')

import logging
import time
import xmlrpclib

import host_controller


class ExampleController(host_controller.HostController):
  """An example controller using the remote subprocess functions."""

  def __init__(self):
    super(ExampleController, self).__init__()
    self.client = None

  def SetUp(self):
    """Creates the client machine and waits until it connects."""
    self.client = self.NewClient(
        isolate_file='client.isolate',
        config_vars={'multi_machine': '1'},
        dimensions={'os': 'legion-linux'},
        idle_timeout_secs=90, connection_timeout_secs=90,
        verbosity=logging.DEBUG)
    self.client.Create()
    self.client.WaitForConnection()

  def Task(self):
    """Main method to run the task code."""
    self.TestLs()
    self.TestTerminate()
    self.TestMultipleProcesses()

  def TestMultipleProcesses(self):
    start = time.time()

    sleep20 = self.client.rpc.subprocess.Popen(['sleep', '20'])
    sleep10 = self.client.rpc.subprocess.Popen(['sleep', '10'])

    self.client.rpc.subprocess.Wait(sleep10)
    elapsed = time.time() - start
    assert elapsed >= 10 and elapsed < 11

    self.client.rpc.subprocess.Wait(sleep20)
    elapsed = time.time() - start
    assert elapsed >= 20

    self.client.rpc.subprocess.Delete(sleep20)
    self.client.rpc.subprocess.Delete(sleep10)

  def TestTerminate(self):
    start = time.time()
    proc = self.client.rpc.subprocess.Popen(['sleep', '20'])
    self.client.rpc.subprocess.Terminate(proc)  # Implicitly deleted
    try:
      self.client.rpc.subprocess.Wait(proc)
    except xmlrpclib.Fault:
      pass
    assert time.time() - start < 20

  def TestLs(self):
    proc = self.client.rpc.subprocess.Popen(['ls'])
    self.client.rpc.subprocess.Wait(proc)
    assert self.client.rpc.subprocess.GetReturncode(proc) == 0
    assert 'client.isolate' in self.client.rpc.subprocess.ReadStdout(proc)
    self.client.rpc.subprocess.Delete(proc)


if __name__ == '__main__':
  ExampleController().RunController()
