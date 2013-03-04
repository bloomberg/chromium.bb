// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_host_event_logger.h"

#include "base/logging.h"
#include "ipc/ipc_sender.h"
#include "net/base/ip_endpoint.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/host_status_monitor.h"
#include "remoting/protocol/transport.h"

namespace remoting {

IpcHostEventLogger::IpcHostEventLogger(base::WeakPtr<HostStatusMonitor> monitor,
                                       IPC::Sender* daemon_channel)
    : daemon_channel_(daemon_channel),
      monitor_(monitor) {
  monitor_->AddStatusObserver(this);
}

IpcHostEventLogger::~IpcHostEventLogger() {
  DCHECK(CalledOnValidThread());

  if (monitor_)
    monitor_->RemoveStatusObserver(this);
}

void IpcHostEventLogger::OnAccessDenied(const std::string& jid) {
  DCHECK(CalledOnValidThread());

  daemon_channel_->Send(new ChromotingNetworkDaemonMsg_AccessDenied(jid));
}

void IpcHostEventLogger::OnClientAuthenticated(const std::string& jid) {
  DCHECK(CalledOnValidThread());

  daemon_channel_->Send(
      new ChromotingNetworkDaemonMsg_ClientAuthenticated(jid));
}

void IpcHostEventLogger::OnClientConnected(const std::string& jid) {
  DCHECK(CalledOnValidThread());

  daemon_channel_->Send(new ChromotingNetworkDaemonMsg_ClientConnected(jid));
}

void IpcHostEventLogger::OnClientDisconnected(const std::string& jid) {
  DCHECK(CalledOnValidThread());

  daemon_channel_->Send(new ChromotingNetworkDaemonMsg_ClientDisconnected(jid));
}

void IpcHostEventLogger::OnClientRouteChange(
    const std::string& jid,
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  DCHECK(CalledOnValidThread());

  SerializedTransportRoute serialized_route;
  serialized_route.type = route.type;
  serialized_route.remote_address = route.remote_address.address();
  serialized_route.remote_port = route.remote_address.port();
  serialized_route.local_address = route.local_address.address();
  serialized_route.local_port = route.local_address.port();

  daemon_channel_->Send(new ChromotingNetworkDaemonMsg_ClientRouteChange(
      jid, channel_name, serialized_route));
}

void IpcHostEventLogger::OnShutdown() {
  DCHECK(CalledOnValidThread());

  daemon_channel_->Send(new ChromotingNetworkDaemonMsg_HostShutdown());
}

void IpcHostEventLogger::OnStart(const std::string& xmpp_login) {
  DCHECK(CalledOnValidThread());

  daemon_channel_->Send(new ChromotingNetworkDaemonMsg_HostStarted(xmpp_login));
}

}  // namespace remoting
