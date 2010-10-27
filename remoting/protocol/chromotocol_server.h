// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The purprose of ChromotocolServer is to facilitate creation of chromotocol
// connections. Both host and client use it to establish chromotocol
// connections. JingleChromotocolServer implements this inteface using
// libjingle.
//
// OUTGOING CONNECTIONS
// Connect() must be used to create new connection to a remote host. The
// returned connection is initially in INITIALIZING state. Later state is
// changed to CONNECTED if the connection is accepted by the host or CLOSED
// if the connection is rejected.
//
// INCOMING CONNECTIONS
// The IncomingConnectionCallback is called when a client attempts to connect.
// The callback function decides whether the connection should be accepted or
// rejected.
//
// CONNECTION OWNERSHIP AND SHUTDOWN
// ChromotocolServer owns all ChromotocolConnections it creates. The server
// must not be closed while connections created by the server are still in use.
// When shutting down the Close() method for the connection and the server
// objects must be called in the following order: ChromotocolConnection,
// ChromotocolServer, JingleClient. The same order must be followed in the case
// of rejected and failed connections.
//
// PROTOCOL VERSION NEGOTIATION
// When client connects to a host it sends a session-initiate stanza with list
// of supported configurations for each channel. If the host decides to accept
// connection, then it selects configuration that is supported by both sides
// and then replies with the session-accept stanza that contans selected
// configuration. The configuration specified in the session-accept is used
// for the session.
//
// The CandidateChromotocolConfig class represents list of configurations
// supported by an endpoint. The |chromotocol_config| argument in the Connect()
// specifies configuration supported on the client side. When the host receives
// session-initiate stanza, the IncomingConnectionCallback is called. The
// configuration sent in the session-intiate staza is available via
// ChromotocolConnnection::candidate_config(). If an incoming connection is
// being accepted then the IncomingConnectionCallback callback function must
// select session configuration and then set it with
// ChromotocolConnection::set_config().

#ifndef REMOTING_PROTOCOL_CHROMOTOCOL_SERVER_H_
#define REMOTING_PROTOCOL_CHROMOTOCOL_SERVER_H_

#include <string>

#include "base/callback.h"
#include "base/ref_counted.h"
#include "remoting/protocol/chromotocol_connection.h"

class Task;

namespace remoting {

// Generic interface for Chromotocol server.
class ChromotocolServer : public base::RefCountedThreadSafe<ChromotocolServer> {
 public:
  enum IncomingConnectionResponse {
    ACCEPT,
    INCOMPATIBLE,
    DECLINE,
  };

  // IncomingConnectionCallback is called when a new connection is received. If
  // the callback decides to accept the connection it should set the second
  // argument to ACCEPT. Otherwise it should set it to DECLINE, or
  // INCOMPATIBLE. INCOMPATIBLE indicates that the session has incompartible
  // configuration, and cannot be accepted.
  // If the callback accepts connection then it must also set configuration
  // for the new connection using ChromotocolConnection::set_config().
  typedef Callback2<ChromotocolConnection*, IncomingConnectionResponse*>::Type
      IncomingConnectionCallback;

  // Initializes connection to the host |jid|.  Ownership of the
  // |chromotocol_config| is passed to the new connection.
  virtual scoped_refptr<ChromotocolConnection> Connect(
      const std::string& jid,
      CandidateChromotocolConfig* chromotocol_config,
      ChromotocolConnection::StateChangeCallback* state_change_callback) = 0;

  // Close server and all current connections. |close_task| is executed after
  // the session client/ is actually closed. No callbacks are called after
  // |closed_task| is executed.
  virtual void Close(Task* closed_task) = 0;

 protected:
  friend class base::RefCountedThreadSafe<ChromotocolServer>;

  ChromotocolServer() { }
  virtual ~ChromotocolServer() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromotocolServer);
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CHROMOTOCOL_SERVER_H_
