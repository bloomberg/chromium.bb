// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_LOG_TO_SERVER_H_
#define REMOTING_CLIENT_LOG_TO_SERVER_H_

#include <deque>
#include <map>
#include <string>

#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "remoting/jingle_glue/server_log_entry.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/errors.h"

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace remoting {

class ChromotingStats;
class IqSender;

// Temporary namespace to prevent conflict with the same-named class in
// remoting/host when linking unittests.
//
// TODO(lambroslambrou): Remove this and factor out any shared code.
namespace client {

// LogToServer sends log entries to a server.
// The contents of the log entries are described in server_log_entry.cc.
// They do not contain any personally identifiable information.
class LogToServer : public base::NonThreadSafe,
                    public SignalStrategy::Listener {
 public:
  LogToServer(ServerLogEntry::Mode mode,
              SignalStrategy* signal_strategy,
              const std::string& directory_bot_jid);
  virtual ~LogToServer();

  // Logs a session state change.
  void LogSessionStateChange(protocol::ConnectionToHost::State state,
                             protocol::ErrorCode error);
  void LogStatistics(remoting::ChromotingStats* statistics);

  // SignalStrategy::Listener interface.
  virtual void OnSignalStrategyStateChange(
      SignalStrategy::State state) OVERRIDE;
  virtual bool OnSignalStrategyIncomingStanza(
      const buzz::XmlElement* stanza) OVERRIDE;

 private:
  void Log(const ServerLogEntry& entry);
  void SendPendingEntries();

  // Generates a new random session ID.
  void GenerateSessionId();

  // Expire the session ID if the maximum duration has been exceeded.
  void MaybeExpireSessionId();

  ServerLogEntry::Mode mode_;
  SignalStrategy* signal_strategy_;
  scoped_ptr<IqSender> iq_sender_;
  std::string directory_bot_jid_;

  std::deque<ServerLogEntry> pending_entries_;

  // A randomly generated session ID to be attached to log messages. This
  // is regenerated at the start of a new session.
  std::string session_id_;

  // Start time of the session.
  base::TimeTicks session_start_time_;

  // Time when the session ID was generated.
  base::TimeTicks session_id_generation_time_;

  DISALLOW_COPY_AND_ASSIGN(LogToServer);
};

}  // namespace client

}  // namespace remoting

#endif  // REMOTING_CLIENT_LOG_TO_SERVER_H_
