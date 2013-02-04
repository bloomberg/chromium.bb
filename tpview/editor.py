# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import random
import socket
import server

# path to current script directory
script_dir = os.path.dirname(os.path.realpath(__file__))

# path to directory which contains the log files accessed by tpview
logs_dir = os.path.join(script_dir, "logs")

class LogEditor(object):
  """
    This class wraps the tpview server and allows log files to be viewed
    or edited.
    Viewing or editing a file means that a URL will be displayed which
    takes the user to a website showing TPView with the provided log data.
  """
  def __init__(self, persistent):
    """
      Prepare the server. Per default the server is killed after serving
      one request for editing or viewing. If you would like to keep the
      server alive provide persistent=True als a parameter.
    """
    self.port = self._FindPort()
    self.server_url = "http://%s:%d/" % (socket.gethostname(), self.port)
    self.persistent = persistent

  def _CheckPort(self, port):
    test_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
      test_socket.bind(("", port));
      test_socket.close()
    except socket.error as error:
      if error.strerror == 'Address already in use':
        return False
      else:
        raise error
    return True

  def _FindPort(self):
    for port in range(9900, 9999):
      if self._CheckPort(port):
        return port

    print "No port between 9900-9999 is available"
    exit(-1)

  def _PrintTPViewURL(self):
    # add a random id so we don't hit any cached files
    print self.server_url + ("edit#id=%05x" % random.randrange(256 ** 5))

  def View(self, log):
    """
      View 'log' in TPView. If the persistent flag is set the server
      will stay alive until the process is killed. Otherwise it will exit
      after the view has been served.
    """
    self._PrintTPViewURL()
    server.View(self.port, log.activity, self.persistent)

  def Edit(self, log):
    """
      View 'log' in TPView and wait for file to be saved.
      Returns the edited results. The server will exit after the
      file has been saved.
    """
    self._PrintTPViewURL()
    log.activity = server.Edit(self.port, log.activity)
    return log

  def Serve(self):
    """
      Serve standalone TPView without any server-side handling
    """
    print self.server_url + "edit"
    server.Serve(self.port)
