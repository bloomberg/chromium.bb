// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LOG_TO_SERVER_H_
#define REMOTING_HOST_LOG_TO_SERVER_H_

#include <deque>
#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/server_log_entry.h"
#include "remoting/jingle_glue/signal_strategy.h"
#include "remoting/protocol/transport.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace remoting {

class HostStatusMonitor;
class IqSender;

// LogToServer sends log entries to a server.
// The contents of the log entries are described in server_log_entry.cc.
// They do not contain any personally identifiable information.
class LogToServer : public base::NonThreadSafe,
                    public HostStatusObserver,
                    public SignalStrategy::Listener {
 public:
  explicit LogToServer(base::WeakPtr<HostStatusMonitor> monitor,
                       ServerLogEntry::Mode mode,
                       SignalStrategy* signal_strategy,
                       const std::string& directory_bot_jid);
  virtual ~LogToServer();

  // Logs a session state change. Currently, this is either
  // connection or disconnection.
  void LogSessionStateChange(const std::string& jid, bool connected);

  // SignalStrategy::Listener interface.
  virtual void OnSignalStrategyStateChange(
      SignalStrategy::State state) OVERRIDE;
  virtual bool OnSignalStrategyIncomingStanza(
      const buzz::XmlElement* stanza) OVERRIDE;

  // HostStatusObserver interface.
  virtual void OnClientConnected(const std::string& jid) OVERRIDE;
  virtual void OnClientDisconnected(const std::string& jid) OVERRIDE;
  virtual void OnClientRouteChange(
      const std::string& jid,
      const std::string& channel_name,
      const protocol::TransportRoute& route) OVERRIDE;

 private:
  void Log(const ServerLogEntry& entry);
  void SendPendingEntries();

  base::WeakPtr<HostStatusMonitor> monitor_;
  ServerLogEntry::Mode mode_;
  SignalStrategy* signal_strategy_;
  scoped_ptr<IqSender> iq_sender_;
  std::string directory_bot_jid_;

  // A map from client JID to the route type of that client's connection to
  // this host.
  std::map<std::string, protocol::TransportRoute::RouteType>
      connection_route_type_;
  std::deque<ServerLogEntry> pending_entries_;

  DISALLOW_COPY_AND_ASSIGN(LogToServer);
};

}  // namespace remoting

#endif  // REMOTING_HOST_LOG_TO_SERVER_H_
