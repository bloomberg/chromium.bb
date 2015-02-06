# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defines the host controller base library.

This module is the basis on which host controllers are built and executed.
"""

import logging
import sys

#pylint: disable=relative-import
import client_lib
import common_lib
import discovery_server


class HostController(object):
  """The base host controller class."""

  def __init__(self):
    self._discovery_server = discovery_server.DiscoveryServer()

  def SetUp(self):
    """Setup method used by the subclass."""
    pass

  def Task(self):
    """Main task method used by the subclass."""
    pass

  def TearDown(self):
    """Teardown method used by the subclass."""
    pass

  def NewClient(self, *args, **kwargs):
    controller = client_lib.ClientController(*args, **kwargs)
    self._discovery_server.RegisterClientCallback(
        controller.otp, controller.OnConnect)
    return controller

  def RunController(self):
    """Main entry point for the controller."""
    print ' '.join(sys.argv)
    common_lib.InitLogging()
    self._discovery_server.Start()

    error = None
    tb = None
    try:
      self.SetUp()
      self.Task()
    except Exception as e:
      # Defer raising exceptions until after TearDown and _TearDown are called.
      error = e
      tb = sys.exc_info()[-1]
    try:
      self.TearDown()
    except Exception as e:
      # Defer raising exceptions until after _TearDown is called.
      # Note that an error raised here will obscure any errors raised
      # previously.
      error = e
      tb = sys.exc_info()[-1]

    self._discovery_server.Shutdown()
    client_lib.ClientController.ReleaseAllControllers()
    if error:
      raise error, None, tb  #pylint: disable=raising-bad-type
