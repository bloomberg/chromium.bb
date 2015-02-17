# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defines the client library."""

import argparse
import datetime
import logging
import os
import socket
import subprocess
import sys
import tempfile
import threading
import xmlrpclib

#pylint: disable=relative-import
import common_lib

ISOLATE_PY = os.path.join(common_lib.SWARMING_DIR, 'isolate.py')
SWARMING_PY = os.path.join(common_lib.SWARMING_DIR, 'swarming.py')


class Error(Exception):
  pass


class ConnectionTimeoutError(Error):
  pass


class ClientController(object):
  """Creates, configures, and controls a client machine."""

  _client_count = 0
  _controllers = []

  def __init__(self, isolate_file, config_vars, dimensions, priority=100,
               idle_timeout_secs=common_lib.DEFAULT_TIMEOUT_SECS,
               connection_timeout_secs=common_lib.DEFAULT_TIMEOUT_SECS,
               verbosity='ERROR', name=None):
    assert isinstance(config_vars, dict)
    assert isinstance(dimensions, dict)
    type(self)._controllers.append(self)
    type(self)._client_count += 1
    self.verbosity = verbosity
    self._name = name or 'Client%d' % type(self)._client_count
    self._priority = priority
    self._isolate_file = isolate_file
    self._isolated_file = isolate_file + 'd'
    self._idle_timeout_secs = idle_timeout_secs
    self._config_vars = config_vars
    self._dimensions = dimensions
    self._connect_event = threading.Event()
    self._connected = False
    self._ip_address = None
    self._otp = self._CreateOTP()
    self._rpc = None

    parser = argparse.ArgumentParser()
    parser.add_argument('--isolate-server')
    parser.add_argument('--swarming-server')
    parser.add_argument('--client-connection-timeout-secs',
                        default=common_lib.DEFAULT_TIMEOUT_SECS)
    args, _ = parser.parse_known_args()

    self._isolate_server = args.isolate_server
    self._swarming_server = args.swarming_server
    self._connection_timeout_secs = (connection_timeout_secs or
                                    args.client_connection_timeout_secs)

  @property
  def name(self):
    return self._name

  @property
  def otp(self):
    return self._otp

  @property
  def connected(self):
    return self._connected

  @property
  def connect_event(self):
    return self._connect_event

  @property
  def rpc(self):
    return self._rpc

  @property
  def verbosity(self):
    return self._verbosity

  @verbosity.setter
  def verbosity(self, level):
    """Sets the verbosity level as a string.

    Either a string ('INFO', 'DEBUG', etc) or a logging level (logging.INFO,
    logging.DEBUG, etc) is allowed.
    """
    assert isinstance(level, (str, int))
    if isinstance(level, int):
      level = logging.getLevelName(level)
    self._verbosity = level  #pylint: disable=attribute-defined-outside-init

  @classmethod
  def ReleaseAllControllers(cls):
    for controller in cls._controllers:
      controller.Release()

  def _CreateOTP(self):
    """Creates the OTP."""
    host_name = socket.gethostname()
    test_name = os.path.basename(sys.argv[0])
    creation_time = datetime.datetime.utcnow()
    otp = 'client:%s-host:%s-test:%s-creation:%s' % (
        self._name, host_name, test_name, creation_time)
    return otp

  def Create(self):
    """Creates the client machine."""
    logging.info('Creating %s', self.name)
    self._connect_event.clear()
    self._ExecuteIsolate()
    self._ExecuteSwarming()

  def WaitForConnection(self):
    """Waits for the client machine to connect.

    Raises:
      ConnectionTimeoutError if the client doesn't connect in time.
    """
    logging.info('Waiting for %s to connect with a timeout of %d seconds',
                 self._name, self._connection_timeout_secs)
    self._connect_event.wait(self._connection_timeout_secs)
    if not self._connect_event.is_set():
      raise ConnectionTimeoutError('%s failed to connect' % self.name)

  def Release(self):
    """Quits the client's RPC server so it can release the machine."""
    if self._rpc is not None and self._connected:
      logging.info('Releasing %s', self._name)
      try:
        self._rpc.Quit()
      except (socket.error, xmlrpclib.Fault):
        logging.error('Unable to connect to %s to call Quit', self.name)
      self._rpc = None
      self._connected = False

  def _ExecuteIsolate(self):
    """Executes isolate.py."""
    cmd = [
        'python',
        ISOLATE_PY,
        'archive',
        '--isolate', self._isolate_file,
        '--isolated', self._isolated_file,
        ]

    if self._isolate_server:
      cmd.extend(['--isolate-server', self._isolate_server])
    for key, value in self._config_vars.iteritems():
      cmd.extend(['--config-var', key, value])

    self._ExecuteProcess(cmd)

  def _ExecuteSwarming(self):
    """Executes swarming.py."""
    cmd = [
        'python',
        SWARMING_PY,
        'trigger',
        self._isolated_file,
        '--priority', str(self._priority),
        ]

    if self._isolate_server:
      cmd.extend(['--isolate-server', self._isolate_server])
    if self._swarming_server:
      cmd.extend(['--swarming', self._swarming_server])
    for key, value in self._dimensions.iteritems():
      cmd.extend(['--dimension', key, value])

    cmd.extend([
        '--',
        '--host', common_lib.MY_IP,
        '--otp', self._otp,
        '--verbosity', self._verbosity,
        '--idle-timeout', str(self._idle_timeout_secs),
        ])

    self._ExecuteProcess(cmd)

  def _ExecuteProcess(self, cmd):
    """Executes a process, waits for it to complete, and checks for success."""
    logging.debug('Running %s', ' '.join(cmd))
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    _, stderr = p.communicate()
    if p.returncode != 0:
      raise Error(stderr)

  def OnConnect(self, ip_address):
    """Receives client ip address on connection."""
    self._ip_address = ip_address
    self._connected = True
    self._rpc = common_lib.ConnectToServer(self._ip_address)
    logging.info('%s connected from %s', self._name, ip_address)
    self._connect_event.set()
