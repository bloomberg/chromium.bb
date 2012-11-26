// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_STATUS_SERVICE_H_
#define REMOTING_HOST_HOST_STATUS_SERVICE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "remoting/host/websocket_listener.h"

namespace base {
class DictionaryValue;
}  // namespace remoting

namespace remoting {

// HostStatusService implements a WebSocket server for the remoting web app to
// use to query the current status of the host.
class HostStatusService {
 public:
  HostStatusService();
  ~HostStatusService();

  // Must be called whenever state of the host changes.
  void SetHostIsUp(const std::string& host_id);
  void SetHostIsDown();

 private:
  class Connection;
  friend class Connection;

  // Returns true if |origin| is one of the remoting extension URLs.
  static bool IsAllowedOrigin(const std::string& origin);

  // Callback from WebsocketListener.
  void OnNewConnection(scoped_ptr<WebSocketConnection> connection);

  // Called from Connection instances when |connection| is closed and the
  // Connection object should be destroyed.
  void OnConnectionClosed(Connection* connection);

  // Creates a status message that should be sent as a response to an incoming
  // getHostState message.
  scoped_ptr<base::DictionaryValue> GetStatusMessage();

  WebSocketListener websocket_listener_;
  std::set<Connection*> connections_;

  // Host name we expect in incoming connections, e.g. "localhost:12810".
  std::string service_host_name_;

  // Current state of the host to provide to clients.
  bool started_;
  std::string host_id_;

  DISALLOW_COPY_AND_ASSIGN(HostStatusService);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_STATUS_SERVICE_H_
