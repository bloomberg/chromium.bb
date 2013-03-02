// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/log_to_server.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "remoting/base/constants.h"
#include "remoting/host/host_status_monitor.h"
#include "remoting/host/server_log_entry.h"
#include "remoting/jingle_glue/iq_sender.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "remoting/protocol/transport.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

using buzz::QName;
using buzz::XmlElement;

namespace remoting {

LogToServer::LogToServer(base::WeakPtr<HostStatusMonitor> monitor,
                         ServerLogEntry::Mode mode,
                         SignalStrategy* signal_strategy,
                         const std::string& directory_bot_jid)
    : monitor_(monitor),
      mode_(mode),
      signal_strategy_(signal_strategy),
      directory_bot_jid_(directory_bot_jid) {
  monitor_->AddStatusObserver(this);
  signal_strategy_->AddListener(this);
}

LogToServer::~LogToServer() {
  signal_strategy_->RemoveListener(this);
  if (monitor_)
    monitor_->RemoveStatusObserver(this);
}

void LogToServer::LogSessionStateChange(const std::string& jid,
                                        bool connected) {
  DCHECK(CalledOnValidThread());

  scoped_ptr<ServerLogEntry> entry(
      ServerLogEntry::MakeForSessionStateChange(connected));
  entry->AddHostFields();
  entry->AddModeField(mode_);

  if (connected) {
    DCHECK(connection_route_type_.count(jid) == 1);
    entry->AddConnectionTypeField(connection_route_type_[jid]);
  }
  Log(*entry.get());
}

void LogToServer::OnSignalStrategyStateChange(SignalStrategy::State state) {
  DCHECK(CalledOnValidThread());

  if (state == SignalStrategy::CONNECTED) {
    iq_sender_.reset(new IqSender(signal_strategy_));
    SendPendingEntries();
  } else if (state == SignalStrategy::DISCONNECTED) {
    iq_sender_.reset();
  }
}

bool LogToServer::OnSignalStrategyIncomingStanza(
    const buzz::XmlElement* stanza) {
  return false;
}

void LogToServer::OnClientConnected(const std::string& jid) {
  DCHECK(CalledOnValidThread());
  LogSessionStateChange(jid, true);
}

void LogToServer::OnClientDisconnected(const std::string& jid) {
  DCHECK(CalledOnValidThread());
  LogSessionStateChange(jid, false);
  connection_route_type_.erase(jid);
}

void LogToServer::OnClientRouteChange(const std::string& jid,
                                      const std::string& channel_name,
                                      const protocol::TransportRoute& route) {
  // Store connection type for the video channel. It is logged later
  // when client authentication is finished.
  if (channel_name == kVideoChannelName) {
    connection_route_type_[jid] = route.type;
  }
}

void LogToServer::Log(const ServerLogEntry& entry) {
  pending_entries_.push_back(entry);
  SendPendingEntries();
}

void LogToServer::SendPendingEntries() {
  if (iq_sender_ == NULL) {
    return;
  }
  if (pending_entries_.empty()) {
    return;
  }
  // Make one stanza containing all the pending entries.
  scoped_ptr<XmlElement> stanza(ServerLogEntry::MakeStanza());
  while (!pending_entries_.empty()) {
    ServerLogEntry& entry = pending_entries_.front();
    stanza->AddElement(entry.ToStanza().release());
    pending_entries_.pop_front();
  }
  // Send the stanza to the server.
  scoped_ptr<IqRequest> req = iq_sender_->SendIq(
      buzz::STR_SET, directory_bot_jid_, stanza.Pass(),
      IqSender::ReplyCallback());
  // We ignore any response, so let the IqRequest be destroyed.
  return;
}

}  // namespace remoting
