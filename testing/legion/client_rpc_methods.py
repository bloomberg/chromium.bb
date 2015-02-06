# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defines the client RPC methods."""

import logging
import subprocess
import threading


class RPCMethods(object):
  """Class exposing RPC methods."""

  def __init__(self, server):
    self.server = server

  def Echo(self, message):
    """Simple RPC method to print and return a message."""
    logging.info('Echoing %s', message)
    return 'echo %s' % str(message)

  def Subprocess(self, cmd):
    """Run the commands in a subprocess.

    Returns:
      (returncode, stdout, stderr).
    """
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
    stdout, stderr = p.communicate()
    return (p.returncode, stdout, stderr)

  def Quit(self):
    """Call server.shutdown in another thread.

    This is needed because server.shutdown waits for the server to actually
    quit. However the server cannot shutdown until it completes handling this
    call. Calling this in the same thread results in a deadlock.
    """
    t = threading.Thread(target=self.server.shutdown)
    t.start()
