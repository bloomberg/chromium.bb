// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/log_to_server.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "remoting/base/constants.h"
#include "remoting/host/server_log_entry.h"
#include "remoting/jingle_glue/iq_sender.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

using buzz::QName;
using buzz::XmlElement;

namespace remoting {

namespace {
const char kLogCommand[] = "log";
}  // namespace

LogToServer::LogToServer(base::MessageLoopProxy* message_loop)
    : message_loop_(message_loop) {
}

LogToServer::~LogToServer() {
}

void LogToServer::LogSessionStateChange(bool connected) {
  scoped_ptr<ServerLogEntry> entry(ServerLogEntry::MakeSessionStateChange(
      connected));
  entry->AddHostFields();
  Log(*entry.get());
}

void LogToServer::OnSignallingConnected(SignalStrategy* signal_strategy,
                                        const std::string& full_jid) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  iq_sender_.reset(new IqSender(signal_strategy));
  SendPendingEntries();
}

void LogToServer::OnSignallingDisconnected() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  iq_sender_.reset();
}

void LogToServer::OnAccessDenied() {
}

void LogToServer::OnClientAuthenticated(const std::string& jid) {
  LogSessionStateChange(true);
}

void LogToServer::OnClientDisconnected(const std::string& jid) {
  LogSessionStateChange(false);
}

void LogToServer::OnShutdown() {
}

void LogToServer::Log(const ServerLogEntry& entry) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  pending_entries_.push_back(entry);
  SendPendingEntries();
}

void LogToServer::SendPendingEntries() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (iq_sender_ == NULL) {
    return;
  }
  if (pending_entries_.empty()) {
    return;
  }
  // Make one stanza containing all the pending entries.
  scoped_ptr<XmlElement> stanza(new XmlElement(QName(
      kChromotingXmlNamespace, kLogCommand)));
  while (!pending_entries_.empty()) {
    ServerLogEntry& entry = pending_entries_.front();
    stanza->AddElement(entry.ToStanza());
    pending_entries_.pop_front();
  }
  // Send the stanza to the server.
  scoped_ptr<IqRequest> req(iq_sender_->SendIq(
      buzz::STR_SET, kChromotingBotJid, stanza.release(),
      IqSender::ReplyCallback()));
  // We ignore any response, so let the IqRequest be destroyed.
  return;
}

}  // namespace remoting
