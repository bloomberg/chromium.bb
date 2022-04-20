# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import absolute_import
import tempfile

import pexpect # pylint: disable=import-error

from telemetry.core import util
from telemetry.internal import forwarders
from telemetry.internal.forwarders import forwarder_utils


class CastForwarderFactory(forwarders.ForwarderFactory):

  def __init__(self, ip_addr):
    super(CastForwarderFactory, self).__init__()
    self._ip_addr = ip_addr

  def Create(self, local_port, remote_port, reverse=False):
    return CastSshForwarder(local_port, remote_port,
                            self._ip_addr, port_forward=not reverse)


class CastSshForwarder(forwarders.Forwarder):

  def __init__(self, local_port, remote_port, ip_addr, port_forward):
    """Sets up ssh port forwarding betweeen a Cast device and the host.

    Args:
      local_port: Port on the host.
      remote_port: Port on the Cast device.
      ip_addr: IP address of the cast receiver.
      port_forward: Determines the direction of the connection."""
    super(CastSshForwarder, self).__init__()
    self._proc = None

    if port_forward:
      assert local_port, 'Local port must be given'
    else:
      assert remote_port, 'Remote port must be given'
      if not local_port:
        # Choose an available port on the host.
        local_port = util.GetUnreservedAvailableLocalPort()

    ssh_args = [
        '-N',  # Don't execute command
        '-T',  # Don't allocate terminal.
        # Ensure SSH is at least verbose enough to print the allocated port
        '-o', 'LogLevel=VERBOSE'
    ]
    ssh_args.extend(forwarder_utils.GetForwardingArgs(
        local_port, remote_port, self.host_ip,
        port_forward))

    with tempfile.NamedTemporaryFile() as stderr_file:
      self._proc = pexpect.spawn(
          'ssh %s %s' % (' '.join(ssh_args), ip_addr), logfile=stderr_file.name)
      self._proc.expect('.*password:')
      self._proc.sendline('root')
      if not remote_port:
        remote_port = forwarder_utils.ReadRemotePort(stderr_file.name)

    self._StartedForwarding(local_port, remote_port)

  def Close(self):
    if self._proc:
      self._proc.kill()
      self._proc = None
    super(CastSshForwarder, self).Close()
