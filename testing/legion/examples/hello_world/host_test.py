#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A simple host test module.

This module runs on the host machine and is responsible for creating 2
client machines, waiting for them, and running RPC calls on them.
"""

# Map the legion directory so we can import the host controller.
import sys
sys.path.append('../../')

import logging
import time

import host_controller


class ExampleController(host_controller.HostController):
  """A simple example controller for a test."""

  def __init__(self):
    super(ExampleController, self).__init__()
    self.client1 = None
    self.client2 = None

  def CreateClient(self):
    """Create a client object and set the proper values."""
    client = self.NewClient(
        isolate_file='client_test.isolate',
        config_vars={'multi_machine': '1'},
        dimensions={'os': 'legion-linux'}, priority=200,
        idle_timeout_secs=90, connection_timeout_secs=90,
        verbosity=logging.INFO)
    client.Create()
    return client

  def SetUp(self):
    """Create the client machines and wait until they connect.

    In this call the actual creation of the client machines is done in parallel
    by the system. The WaitForConnect calls are performed in series but will
    return as soon as the clients connect.
    """
    self.client1 = self.CreateClient()
    self.client2 = self.CreateClient()
    self.client1.WaitForConnection()
    self.client2.WaitForConnection()

  def Task(self):
    """Main method to run the task code."""
    self.CallEcho(self.client1)
    self.CallEcho(self.client2)
    self.CallClientTest(self.client1)
    self.CallClientTest(self.client2)

  def CallEcho(self, client):
    """Call rpc.Echo on a client."""
    logging.info('Calling Echo on %s', client.name)
    logging.info(client.rpc.Echo(client.name))

  def CallClientTest(self, client):
    """Call client_test.py name on a client."""
    logging.info('Calling Subprocess to run "./client_test.py %s"', client.name)
    proc = client.rpc.subprocess.Popen(['./client_test.py', client.name])
    client.rpc.subprocess.Wait(proc)
    retcode = client.rpc.subprocess.GetReturncode(proc)
    stdout = client.rpc.subprocess.ReadStdout(proc)
    stderr = client.rpc.subprocess.ReadStderr(proc)
    logging.info('retcode: %s, stdout: %s, stderr: %s', retcode, stdout, stderr)


if __name__ == '__main__':
  ExampleController().RunController()
