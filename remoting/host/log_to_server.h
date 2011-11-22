// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LOG_TO_SERVER_H_
#define REMOTING_HOST_LOG_TO_SERVER_H_

#include <deque>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/server_log_entry.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace remoting {

class IqSender;

/**
 * A class that sends log entries to a server.
 *
 * The class is not thread-safe.
 */
class LogToServer : public HostStatusObserver {
 public:
  explicit LogToServer(base::MessageLoopProxy* message_loop);
  virtual ~LogToServer();

  // Logs a session state change.
  // Currently, this is either connection or disconnection.
  void LogSessionStateChange(bool connected);

  // HostStatusObserver implementation.
  virtual void OnSignallingConnected(SignalStrategy* signal_strategy,
                                     const std::string& full_jid) OVERRIDE;
  virtual void OnSignallingDisconnected() OVERRIDE;
  virtual void OnClientAuthenticated(const std::string& jid) OVERRIDE;
  virtual void OnClientDisconnected(const std::string& jid) OVERRIDE;
  virtual void OnAccessDenied() OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

 private:
  void Log(const ServerLogEntry& entry);
  void SendPendingEntries();

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  scoped_ptr<IqSender> iq_sender_;
  std::deque<ServerLogEntry> pending_entries_;

  DISALLOW_COPY_AND_ASSIGN(LogToServer);
};

}  // namespace remoting

#endif  // REMOTING_HOST_LOG_TO_SERVER_H_
