// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/log_to_server.h"

#include "remoting/base/constants.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/signal_strategy.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"

using buzz::QName;
using buzz::XmlElement;

namespace remoting {

LogToServer::LogToServer(ServerLogEntry::Mode mode,
                         SignalStrategy* signal_strategy,
                         const std::string& directory_bot_jid)
    : mode_(mode),
      signal_strategy_(signal_strategy),
      directory_bot_jid_(directory_bot_jid) {
  signal_strategy_->AddListener(this);
}

LogToServer::~LogToServer() {
  signal_strategy_->RemoveListener(this);
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
