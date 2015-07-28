# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys

TESTING_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..', '..', '..')
sys.path.append(TESTING_DIR)

from legion import legion_test_case


class HttpTest(legion_test_case.TestCase):
  """Example HTTP test case."""

  @classmethod
  def GetArgs(cls):
    """Get command line args."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--http-server')
    parser.add_argument('--http-client')
    parser.add_argument('--os', default='Ubuntu-14.04')
    args, _ = parser.parse_known_args()
    return args

  @classmethod
  def CreateTask(cls, name, task_hash, os_type):
    """Create a new task."""
    #pylint: disable=unexpected-keyword-arg,no-value-for-parameter
    task = super(HttpTest, cls).CreateTask(
        name=name,
        isolated_hash=task_hash,
        dimensions={'os': os_type})
    task.Create()
    return task

  @classmethod
  def setUpClass(cls):
    """Creates the task machines and waits until they connect."""
    args = cls.GetArgs()
    cls.http_server = cls.CreateTask(
        'http_server', args.http_server, args.os)
    cls.http_client = cls.CreateTask(
        'http_client', args.http_client, args.os)
    cls.http_server.WaitForConnection()
    cls.http_client.WaitForConnection()

  def testHttpWorks(self):
    server_port = '8080'
    server_proc = None
    client_proc = None
    try:
      server_ip = self.http_server.rpc.GetIpAddress()
      server_proc = self.http_server.Process(
          ['python', 'http_server.py', '--port', server_port])
      client_proc = self.http_client.Process(
          ['python', 'http_client.py', '--server', server_ip,
           '--port', server_port])
      client_proc.Wait()
      self.assertEqual(client_proc.GetReturncode(), 0)
    finally:
      if server_proc:
        server_proc.Kill()
        server_proc.Delete()
      if client_proc:
        client_proc.Kill()
        client_proc.Delete()


if __name__ == '__main__':
  legion_test_case.main()
