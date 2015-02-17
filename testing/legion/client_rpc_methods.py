# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defines the client RPC methods."""

import os
import sys
import logging
import threading

#pylint: disable=relative-import
import common_lib

# Map swarming_client to use subprocess42
sys.path.append(common_lib.SWARMING_DIR)

from utils import subprocess42


class RPCMethods(object):
  """Class exposing RPC methods."""

  _dotted_whitelist = ['subprocess']

  def __init__(self, server):
    self._server = server
    self.subprocess = Subprocess

  def _dispatch(self, method, params):
    obj = self
    if '.' in method:
      # Allow only white listed dotted names
      name, method = method.split('.')
      assert name in self._dotted_whitelist
      obj = getattr(self, name)
    return getattr(obj, method)(*params)

  def Echo(self, message):
    """Simple RPC method to print and return a message."""
    logging.info('Echoing %s', message)
    return 'echo %s' % str(message)

  def Quit(self):
    """Call _server.shutdown in another thread.

    This is needed because server.shutdown waits for the server to actually
    quit. However the server cannot shutdown until it completes handling this
    call. Calling this in the same thread results in a deadlock.
    """
    t = threading.Thread(target=self._server.shutdown)
    t.start()


class Subprocess(object):
  """Implements a server-based non-blocking subprocess.

  This non-blocking subprocess allows the caller to continue operating while
  also able to interact with this subprocess based on a key returned to
  the caller at the time of creation.
  """

  _processes = {}
  _process_next_id = 0
  _creation_lock = threading.Lock()

  def __init__(self, cmd):
    self.proc = subprocess42.Popen(cmd, stdout=subprocess42.PIPE,
                                   stderr=subprocess42.PIPE)
    self.stdout = ''
    self.stderr = ''
    self.data_lock = threading.Lock()
    threading.Thread(target=self._run).start()

  def _run(self):
    for pipe, data in self.proc.yield_any():
      with self.data_lock:
        if pipe == 'stdout':
          self.stdout += data
        else:
          self.stderr += data

  @classmethod
  def Popen(cls, cmd):
    with cls._creation_lock:
      key = 'Process%d' % cls._process_next_id
      cls._process_next_id += 1
    logging.debug('Creating process %s', key)
    process = cls(cmd)
    cls._processes[key] = process
    return key

  @classmethod
  def Terminate(cls, key):
    logging.debug('Terminating and deleting process %s', key)
    return cls._processes.pop(key).proc.terminate()

  @classmethod
  def Kill(cls, key):
    logging.debug('Killing and deleting process %s', key)
    return cls._processes.pop(key).proc.kill()

  @classmethod
  def Delete(cls, key):
    logging.debug('Deleting process %s', key)
    cls._processes.pop(key)

  @classmethod
  def GetReturncode(cls, key):
    return cls._processes[key].proc.returncode

  @classmethod
  def ReadStdout(cls, key):
    """Returns all stdout since the last call to ReadStdout.

    This call allows the user to read stdout while the process is running.
    However each call will flush the local stdout buffer. In order to make
    multiple calls to ReadStdout and to retain the entire output the results
    of this call will need to be buffered in the calling code.
    """
    proc = cls._processes[key]
    with proc.data_lock:
      # Perform a "read" on the stdout data
      stdout = proc.stdout
      proc.stdout = ''
    return stdout

  @classmethod
  def ReadStderr(cls, key):
    """Returns all stderr read since the last call to ReadStderr.

    See ReadStdout for additional details.
    """
    proc = cls._processes[key]
    with proc.data_lock:
      # Perform a "read" on the stderr data
      stderr = proc.stderr
      proc.stderr = ''
    return stderr

  @classmethod
  def Wait(cls, key):
    return cls._processes[key].proc.wait()

  @classmethod
  def Poll(cls, key):
    return cls._processes[key].proc.poll()

  @classmethod
  def GetPid(cls, key):
    return cls._processes[key].proc.pid
