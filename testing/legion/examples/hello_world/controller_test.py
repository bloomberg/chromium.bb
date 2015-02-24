#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A simple host test module.

This module runs on the host machine and is responsible for creating 2
task machines, waiting for them, and running RPC calls on them.
"""

# Map the legion directory so we can import the host controller.
import sys
sys.path.append('../../')

import argparse
import logging
import time

import test_controller


class ExampleTestController(test_controller.TestController):
  """A simple example controller for a test."""

  def __init__(self):
    super(ExampleTestController, self).__init__()
    self.task1 = None
    self.task2 = None

  def CreateTask(self, isolated_hash):
    """Create a task object and set the proper values."""
    task = self.CreateNewTask(
        isolated_hash=isolated_hash,
        dimensions={'os': 'Ubuntu-14.04', 'pool': 'Legion'}, priority=200,
        idle_timeout_secs=90, connection_timeout_secs=90,
        verbosity=logging.INFO,
        run_id=1)
    task.Create()
    return task

  def SetUp(self):
    """Create the task machines and wait until they connect.

    In this call the actual creation of the task machines is done in parallel
    by the system. The WaitForConnect calls are performed in series but will
    return as soon as the tasks connect.
    """
    parser = argparse.ArgumentParser()
    parser.add_argument('--task-hash')
    args, _ = parser.parse_known_args()

    self.task1 = self.CreateTask(args.task_hash)
    self.task2 = self.CreateTask(args.task_hash)
    self.task1.WaitForConnection()
    self.task2.WaitForConnection()

  def RunTest(self):
    """Main method to run the test code."""
    self.CallEcho(self.task1)
    self.CallEcho(self.task2)
    self.CallTaskTest(self.task1)
    self.CallTaskTest(self.task2)

  def CallEcho(self, task):
    """Call rpc.Echo on a task."""
    logging.info('Calling Echo on %s', task.name)
    logging.info(task.rpc.Echo(task.name))

  def CallTaskTest(self, task):
    """Call task_test.py name on a task."""
    logging.info('Calling Subprocess to run "./task_test.py %s"', task.name)
    proc = task.rpc.subprocess.Popen(['./task_test.py', task.name])
    task.rpc.subprocess.Wait(proc)
    retcode = task.rpc.subprocess.GetReturncode(proc)
    stdout = task.rpc.subprocess.ReadStdout(proc)
    stderr = task.rpc.subprocess.ReadStderr(proc)
    logging.info('retcode: %s, stdout: %s, stderr: %s', retcode, stdout, stderr)


if __name__ == '__main__':
  ExampleTestController().RunController()
