// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CLIENT_STATUS_LOGGER_H_
#define REMOTING_CLIENT_CLIENT_STATUS_LOGGER_H_

#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/errors.h"
#include "remoting/signaling/log_to_server.h"

namespace remoting {

class ChromotingStats;

// ClientStatusLogger sends client log entries to a server.
// The contents of the log entries are described in server_log_entry_client.cc.
// They do not contain any personally identifiable information.
class ClientStatusLogger : public base::NonThreadSafe {
 public:
  ClientStatusLogger(ServerLogEntry::Mode mode,
                     SignalStrategy* signal_strategy,
                     const std::string& directory_bot_jid);
  ~ClientStatusLogger();

  void LogSessionStateChange(protocol::ConnectionToHost::State state,
                             protocol::ErrorCode error);
  void LogStatistics(remoting::ChromotingStats* statistics);

  // Allows test code to fake SignalStrategy state change events.
  void SetSignalingStateForTest(SignalStrategy::State state);

 private:
  LogToServer log_to_server_;

  // Generates a new random session ID.
  void GenerateSessionId();

  // Expire the session ID if the maximum duration has been exceeded.
  void MaybeExpireSessionId();

  // A randomly generated session ID to be attached to log messages. This
  // is regenerated at the start of a new session.
  std::string session_id_;

  // Start time of the session.
  base::TimeTicks session_start_time_;

  // Time when the session ID was generated.
  base::TimeTicks session_id_generation_time_;

  DISALLOW_COPY_AND_ASSIGN(ClientStatusLogger);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CLIENT_STATUS_LOGGER_H_
