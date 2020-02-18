# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import tempfile

from telemetry.core import util
from telemetry.internal import forwarders
from telemetry.internal.forwarders import do_nothing_forwarder
from telemetry.internal.forwarders import forwarder_utils

import py_utils


class CrOsForwarderFactory(forwarders.ForwarderFactory):

  def __init__(self, cri):
    super(CrOsForwarderFactory, self).__init__()
    self._cri = cri

  def Create(self, local_port, remote_port, reverse=False):
    if self._cri.local:
      return do_nothing_forwarder.DoNothingForwarder(local_port, remote_port)
    else:
      return CrOsSshForwarder(
          self._cri, local_port, remote_port, port_forward=not reverse)


class CrOsSshForwarder(forwarders.Forwarder):

  def __init__(self, cri, local_port, remote_port, port_forward):
    super(CrOsSshForwarder, self).__init__()
    self._cri = cri
    self._proc = None

    if port_forward:
      assert local_port, 'Local port must be given'
    else:
      assert remote_port, 'Remote port must be given'
      if not local_port:
        # Choose an available port on the host.
        local_port = util.GetUnreservedAvailableLocalPort()

    forwarding_args = forwarder_utils.GetForwardingArgs(
        local_port, remote_port, self.host_ip, port_forward)

    # TODO(crbug.com/793256): Consider avoiding the extra tempfile and
    # read stderr directly from the subprocess instead.
    with tempfile.NamedTemporaryFile() as stderr_file:
      self._proc = subprocess.Popen(
          self._cri.FormSSHCommandLine(['-NT'], forwarding_args,
                                       port_forward=port_forward),
          stdout=subprocess.PIPE,
          stderr=stderr_file,
          stdin=subprocess.PIPE,
          shell=False)
      if not remote_port:
        remote_port = forwarder_utils.ReadRemotePort(stderr_file.name)

    self._StartedForwarding(local_port, remote_port)
    py_utils.WaitFor(self._IsConnectionReady, timeout=60)

  def _IsConnectionReady(self):
    return self._cri.IsHTTPServerRunningOnPort(self.remote_port)

  def Close(self):
    if self._proc:
      self._proc.kill()
      self._proc = None
    super(CrOsSshForwarder, self).Close()
