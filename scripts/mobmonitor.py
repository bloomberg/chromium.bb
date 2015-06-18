# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The Mob* Monitor web interface."""

from __future__ import print_function

import cherrypy

from chromite.lib import remote_access
from chromite.lib import commandline
from chromite.mobmonitor.checkfile import manager


class MobMonitorRoot(object):
  """The central object supporting the Mob* Monitor web interface."""

  def __init__(self, checkfile_manager):
    self.checkfile_manager = checkfile_manager

  @cherrypy.expose
  def index(self):
    """Presents a welcome message."""
    return 'Welcome to the Mob* Monitor!'

  # TODO (msartori): Stub until crbug.com/493318 is implemented.
  @cherrypy.expose
  def GetServiceList(self):
    """Return a list of the monitored services.

    Returns:
      A list of the monitored services.
    """
    return []

  # TODO (msartori): Stub until crbug.com/493319 is implemented.
  @cherrypy.expose
  def GetStatus(self, service=None):
    """Return the health status of the specified service.

    Args:
      service: The service whose health status is being queried. If service
        is None, return the health status of all monitored services.
    """
    return 'GetStatus reached: service="%s"' % service

  # TODO (msartori): Stub until crbug.com/493320 is implemented.
  @cherrypy.expose
  def RepairService(self, service, action):
    """Execute the repair action on the specified service.

    Args:
      service: The service that the specified action will be applied to.
      action: The action to be applied.
    """
    return 'RepairService reached: service="%s", actions="%s"' % (service,
                                                                  action)


def ParseArguments(argv):
  """Creates the argument parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('-d', '--checkdir',
                      default='/etc/mobmonitor/checkfiles/',
                      help='The Mob* Monitor checkfile directory.')
  parser.add_argument('-p', '--port', type=int, default=9999,
                      help='The Mob* Monitor port.')

  return parser.parse_args(argv)


def main(argv):
  options = ParseArguments(argv)
  options.Freeze()

  # Start the Mob* Monitor web interface.
  cherrypy.config.update({'server.socket_port':
                          remote_access.NormalizePort(options.port)})

  # Setup the mobmonitor
  checkfile_manager = manager.CheckFileManager(checkdir=options.checkdir)
  mobmonitor = MobMonitorRoot(checkfile_manager)

  # Start the checkfile collection and execution background task.
  checkfile_manager.StartCollectionExecution()

  cherrypy.quickstart(mobmonitor)
