// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/client_status_logger.h"

#include "base/macros.h"
#include "base/rand_util.h"
#include "remoting/client/chromoting_stats.h"
#include "remoting/client/server_log_entry_client.h"

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

ClientStatusLogger::ClientStatusLogger(ServerLogEntry::Mode mode,
                                       SignalStrategy* signal_strategy,
                                       const std::string& directory_bot_jid)
    : log_to_server_(mode, signal_strategy, directory_bot_jid) {
}

ClientStatusLogger::~ClientStatusLogger() {
}

void ClientStatusLogger::LogSessionStateChange(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  DCHECK(CalledOnValidThread());

  scoped_ptr<ServerLogEntry> entry(
      MakeLogEntryForSessionStateChange(state, error));
  AddClientFieldsToLogEntry(entry.get());
  entry->AddModeField(log_to_server_.mode());

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

  log_to_server_.Log(*entry.get());
}

void ClientStatusLogger::LogStatistics(ChromotingStats* statistics) {
  DCHECK(CalledOnValidThread());

  MaybeExpireSessionId();

  scoped_ptr<ServerLogEntry> entry(MakeLogEntryForStatistics(statistics));
  AddClientFieldsToLogEntry(entry.get());
  entry->AddModeField(log_to_server_.mode());
  AddSessionIdToLogEntry(entry.get(), session_id_);
  log_to_server_.Log(*entry.get());
}

void ClientStatusLogger::SetSignalingStateForTest(SignalStrategy::State state) {
  log_to_server_.OnSignalStrategyStateChange(state);
}

void ClientStatusLogger::GenerateSessionId() {
  session_id_.resize(kSessionIdLength);
  for (int i = 0; i < kSessionIdLength; i++) {
    const int alphabet_size = arraysize(kSessionIdAlphabet) - 1;
    session_id_[i] = kSessionIdAlphabet[base::RandGenerator(alphabet_size)];
  }
  session_id_generation_time_ = base::TimeTicks::Now();
}

void ClientStatusLogger::MaybeExpireSessionId() {
  if (session_id_.empty()) {
    return;
  }

  base::TimeDelta max_age = base::TimeDelta::FromDays(kMaxSessionIdAgeDays);
  if (base::TimeTicks::Now() - session_id_generation_time_ > max_age) {
    // Log the old session ID.
    scoped_ptr<ServerLogEntry> entry(MakeLogEntryForSessionIdOld(session_id_));
    entry->AddModeField(log_to_server_.mode());
    log_to_server_.Log(*entry.get());

    // Generate a new session ID.
    GenerateSessionId();

    // Log the new session ID.
    entry = MakeLogEntryForSessionIdNew(session_id_);
    entry->AddModeField(log_to_server_.mode());
    log_to_server_.Log(*entry.get());
  }
}

}  // namespace remoting
