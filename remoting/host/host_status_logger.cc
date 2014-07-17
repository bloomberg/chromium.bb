// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_status_logger.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "remoting/base/constants.h"
#include "remoting/host/host_status_monitor.h"
#include "remoting/host/server_log_entry_host.h"
#include "remoting/protocol/transport.h"
#include "remoting/signaling/server_log_entry.h"

namespace remoting {

HostStatusLogger::HostStatusLogger(base::WeakPtr<HostStatusMonitor> monitor,
                                   ServerLogEntry::Mode mode,
                                   SignalStrategy* signal_strategy,
                                   const std::string& directory_bot_jid)
    : log_to_server_(mode, signal_strategy, directory_bot_jid),
      monitor_(monitor) {
  monitor_->AddStatusObserver(this);
}

HostStatusLogger::~HostStatusLogger() {
  if (monitor_.get())
    monitor_->RemoveStatusObserver(this);
}

void HostStatusLogger::LogSessionStateChange(const std::string& jid,
                                             bool connected) {
  DCHECK(CalledOnValidThread());

  scoped_ptr<ServerLogEntry> entry(
      MakeLogEntryForSessionStateChange(connected));
  AddHostFieldsToLogEntry(entry.get());
  entry->AddModeField(log_to_server_.mode());

  if (connected) {
    DCHECK_EQ(connection_route_type_.count(jid), 1u);
    AddConnectionTypeToLogEntry(entry.get(), connection_route_type_[jid]);
  }
  log_to_server_.Log(*entry.get());
}

void HostStatusLogger::OnClientConnected(const std::string& jid) {
  DCHECK(CalledOnValidThread());
  LogSessionStateChange(jid, true);
}

void HostStatusLogger::OnClientDisconnected(const std::string& jid) {
  DCHECK(CalledOnValidThread());
  LogSessionStateChange(jid, false);
  connection_route_type_.erase(jid);
}

void HostStatusLogger::OnClientRouteChange(
    const std::string& jid,
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  // Store connection type for the video channel. It is logged later
  // when client authentication is finished.
  if (channel_name == kVideoChannelName) {
    connection_route_type_[jid] = route.type;
  }
}

void HostStatusLogger::SetSignalingStateForTest(SignalStrategy::State state) {
  log_to_server_.OnSignalStrategyStateChange(state);
}

}  // namespace remoting
