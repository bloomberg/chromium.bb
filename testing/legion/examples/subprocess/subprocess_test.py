#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A host test module demonstrating interacting with remote subprocesses."""

# Map the legion directory so we can import the host controller.
import sys
sys.path.append('../../')

import argparse
import logging
import time
import xmlrpclib

import test_controller


class ExampleTestController(test_controller.TestController):
  """An example controller using the remote subprocess functions."""

  def __init__(self):
    super(ExampleTestController, self).__init__()
    self.task = None

  def SetUp(self):
    """Creates the task machine and waits until it connects."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--task-hash')
    args, _ = parser.parse_known_args()

    self.task = self.CreateNewTask(
        isolated_hash=args.task_hash,
        dimensions={'os': 'Ubuntu-14.04', 'pool': 'Chromoting'},
        idle_timeout_secs=90, connection_timeout_secs=90,
        verbosity=logging.DEBUG)
    self.task.Create()
    self.task.WaitForConnection()

  def RunTest(self):
    """Main method to run the test code."""
    self.TestLs()
    self.TestTerminate()
    self.TestMultipleProcesses()

  def TestMultipleProcesses(self):
    start = time.time()

    sleep20 = self.task.rpc.subprocess.Process(['sleep', '20'])
    self.task.rpc.subprocess.Start(sleep20)
    sleep10 = self.task.rpc.subprocess.Process(['sleep', '10'])
    self.task.rpc.subprocess.Start(sleep10)

    self.task.rpc.subprocess.Wait(sleep10)
    elapsed = time.time() - start
    assert elapsed >= 10 and elapsed < 11

    self.task.rpc.subprocess.Wait(sleep20)
    elapsed = time.time() - start
    assert elapsed >= 20

    self.task.rpc.subprocess.Delete(sleep20)
    self.task.rpc.subprocess.Delete(sleep10)

  def TestTerminate(self):
    start = time.time()
    proc = self.task.rpc.subprocess.Process(['sleep', '20'])
    self.task.rpc.subprocess.Start(proc)
    self.task.rpc.subprocess.Terminate(proc)
    try:
      self.task.rpc.subprocess.Wait(proc)
    except xmlrpclib.Fault:
      pass
    finally:
      self.task.rpc.subprocess.Delete(proc)
    assert time.time() - start < 20

  def TestLs(self):
    proc = self.task.rpc.subprocess.Process(['ls'])
    self.task.rpc.subprocess.Start(proc)
    self.task.rpc.subprocess.Wait(proc)
    assert self.task.rpc.subprocess.GetReturncode(proc) == 0
    assert 'task.isolate' in self.task.rpc.subprocess.ReadStdout(proc)
    self.task.rpc.subprocess.Delete(proc)


if __name__ == '__main__':
  ExampleTestController().RunController()
