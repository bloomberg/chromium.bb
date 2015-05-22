# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Command-line interface for the Mob* Monitor."""

from __future__ import print_function

from chromite.lib import commandline
from chromite.lib import remote_access
from chromite.mobmonitor.rpc import rpc


class MobMonCli(object):
  """Provides command-line functionality for using the Mob* Monitor."""

  def __init__(self, host='localhost', port=9999):
    self.host = host
    self.port = remote_access.NormalizePort(port)

  def ExecuteRequest(self, request, service, action):
    """Execute the request if an appropriate RPC function is defined.

    Args:
      request: The name of the RPC.
      service: The name of the service involved in the RPC.
      action: The action to be performed.
    """
    rpcexec = rpc.RpcExecutor(self.host, self.port)

    if not hasattr(rpcexec, request):
      raise rpc.RpcError('The request "%s" is not recognized.' % request)

    if 'GetServiceList' == request:
      return rpcexec.GetServiceList()

    if 'GetStatus' == request:
      return rpcexec.GetStatus(service=service)

    if 'RepairService' == request:
      return rpcexec.RepairService(service=service, action=action)


def ParseArguments(argv):
  parser = commandline.ArgumentParser()
  parser.add_argument('request', choices=rpc.RPC_LIST)
  parser.add_argument('-s', '--service', help='The service to act upon')
  parser.add_argument('-a', '--action', help='The action to execute')
  parser.add_argument('--host', default='localhost',
                      help='The hostname of the Mob* Monitor.')
  parser.add_argument('-p', '--port', type=int, default=9999,
                      help='The Mob* Monitor port.')

  return parser.parse_args(argv)


def main(argv):
  """Command line interface for the Mob* Monitor.

  The basic syntax is:
    mobmon <request> [args]
    mobmon --help
  """
  options = ParseArguments(argv)

  cli = MobMonCli(options.host, options.port)
  result = cli.ExecuteRequest(options.request, options.service,
                              options.action)

  print(result)
