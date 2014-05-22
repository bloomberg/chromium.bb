// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/log_to_server.h"

#include "base/macros.h"
#include "base/rand_util.h"
#include "remoting/base/constants.h"
#include "remoting/client/chromoting_stats.h"
#include "remoting/client/server_log_entry_client.h"
#include "remoting/jingle_glue/iq_sender.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"
#include "third_party/libjingle/source/talk/xmpp/constants.h"

using buzz::QName;
using buzz::XmlElement;
using remoting::protocol::ConnectionToHost;

namespace {

const char kSessionIdAlphabet[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
const int kSessionIdLength = 20;

const int kMaxSessionIdAgeDays = 1;

bool IsStartOfSession(ConnectionToHost::State state) {
  return state == ConnectionToHost::INITIALIZING ||
      state == ConnectionToHost::CONNECTING ||
      state == ConnectionToHost::AUTHENTICATED ||
      state == ConnectionToHost::CONNECTED;
}

bool IsEndOfSession(ConnectionToHost::State state) {
  return state == ConnectionToHost::FAILED ||
      state == ConnectionToHost::CLOSED;
}

bool ShouldAddDuration(ConnectionToHost::State state) {
  // Duration is added to log entries at the end of the session, as well as at
  // some intermediate states where it is relevant (e.g. to determine how long
  // it took for a session to become CONNECTED).
  return IsEndOfSession(state) || state == ConnectionToHost::CONNECTED;
}

}  // namespace

namespace remoting {

namespace client {

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

void LogToServer::LogSessionStateChange(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  DCHECK(CalledOnValidThread());

  scoped_ptr<ServerLogEntry> entry(
      MakeLogEntryForSessionStateChange(state, error));
  AddClientFieldsToLogEntry(entry.get());
  entry->AddModeField(mode_);

  MaybeExpireSessionId();
  if (IsStartOfSession(state)) {
    // Maybe set the session ID and start time.
    if (session_id_.empty()) {
      GenerateSessionId();
    }
    if (session_start_time_.is_null()) {
      session_start_time_ = base::TimeTicks::Now();
    }
  }

  if (!session_id_.empty()) {
    AddSessionIdToLogEntry(entry.get(), session_id_);
  }

  // Maybe clear the session start time and log the session duration.
  if (ShouldAddDuration(state) && !session_start_time_.is_null()) {
    AddSessionDurationToLogEntry(entry.get(),
                                 base::TimeTicks::Now() - session_start_time_);
  }

  if (IsEndOfSession(state)) {
    session_start_time_ = base::TimeTicks();
    session_id_.clear();
  }

  Log(*entry.get());
}

void LogToServer::LogStatistics(ChromotingStats* statistics) {
  DCHECK(CalledOnValidThread());

  MaybeExpireSessionId();

  scoped_ptr<ServerLogEntry> entry(MakeLogEntryForStatistics(statistics));
  AddClientFieldsToLogEntry(entry.get());
  entry->AddModeField(mode_);
  AddSessionIdToLogEntry(entry.get(), session_id_);
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

void LogToServer::GenerateSessionId() {
  session_id_.resize(kSessionIdLength);
  for (int i = 0; i < kSessionIdLength; i++) {
    const int alphabet_size = arraysize(kSessionIdAlphabet) - 1;
    session_id_[i] = kSessionIdAlphabet[base::RandGenerator(alphabet_size)];
  }
  session_id_generation_time_ = base::TimeTicks::Now();
}

void LogToServer::MaybeExpireSessionId() {
  if (session_id_.empty()) {
    return;
  }

  base::TimeDelta max_age = base::TimeDelta::FromDays(kMaxSessionIdAgeDays);
  if (base::TimeTicks::Now() - session_id_generation_time_ > max_age) {
    // Log the old session ID.
    scoped_ptr<ServerLogEntry> entry(MakeLogEntryForSessionIdOld(session_id_));
    entry->AddModeField(mode_);
    Log(*entry.get());

    // Generate a new session ID.
    GenerateSessionId();

    // Log the new session ID.
    entry = MakeLogEntryForSessionIdNew(session_id_);
    entry->AddModeField(mode_);
    Log(*entry.get());
  }
}

}  // namespace client

}  // namespace remoting
