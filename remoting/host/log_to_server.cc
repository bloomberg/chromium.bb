// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/log_to_server.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "remoting/base/constants.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/server_log_entry.h"
#include "remoting/jingle_glue/iq_sender.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "remoting/protocol/transport.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

using buzz::QName;
using buzz::XmlElement;

namespace remoting {

LogToServer::LogToServer(ChromotingHost* host,
                         ServerLogEntry::Mode mode,
                         SignalStrategy* signal_strategy)
    : host_(host),
      mode_(mode),
      signal_strategy_(signal_strategy),
      connection_type_set_(false) {
  signal_strategy_->AddListener(this);

  // |host| may be NULL in tests.
  if (host_)
    host_->AddStatusObserver(this);
}

LogToServer::~LogToServer() {
  signal_strategy_->RemoveListener(this);
  if (host_)
    host_->RemoveStatusObserver(this);
}

void LogToServer::LogSessionStateChange(bool connected) {
  DCHECK(CalledOnValidThread());

  scoped_ptr<ServerLogEntry> entry(
      ServerLogEntry::MakeSessionStateChange(connected));
  entry->AddHostFields();
  entry->AddModeField(mode_);

  if (connected) {
    DCHECK(connection_type_set_);
    entry->AddConnectionTypeField(connection_type_);
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

void LogToServer::OnClientAuthenticated(const std::string& jid) {
  DCHECK(CalledOnValidThread());
  LogSessionStateChange(true);
}

void LogToServer::OnClientDisconnected(const std::string& jid) {
  DCHECK(CalledOnValidThread());
  LogSessionStateChange(false);
  connection_type_set_ = false;
}

void LogToServer::OnAccessDenied(const std::string& jid) {
}

void LogToServer::OnClientRouteChange(const std::string& jid,
                                      const std::string& channel_name,
                                      const protocol::TransportRoute& route) {
  // Store connection type for the video channel. It is logged later
  // when client authentication is finished.
  if (channel_name == kVideoChannelName) {
    connection_type_ = route.type;
    connection_type_set_ = true;
  }
}

void LogToServer::OnShutdown() {
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
      buzz::STR_SET, kChromotingBotJid, stanza.Pass(),
      IqSender::ReplyCallback());
  // We ignore any response, so let the IqRequest be destroyed.
  return;
}

}  // namespace remoting
