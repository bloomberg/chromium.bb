// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The purprose of SessionManager is to facilitate creation of chromotocol
// sessions. Both host and client use it to establish chromotocol
// sessions. JingleChromotocolServer implements this inteface using
// libjingle.
//
// OUTGOING SESSIONS
// Connect() must be used to create new session to a remote host. The
// returned sessionion is initially in INITIALIZING state. Later state is
// changed to CONNECTED if the session is accepted by the host or CLOSED
// if the session is rejected.
//
// INCOMING SESSIONS
// The IncomingSessionCallback is called when a client attempts to connect.
// The callback function decides whether the session should be accepted or
// rejected.
//
// SESSION OWNERSHIP AND SHUTDOWN
// SessionManager owns all Chromotocol Session it creates. The server
// must not be closed while sessions created by the server are still in use.
// When shutting down the Close() method for the sessionion and the server
// objects must be called in the following order: Session,
// SessionManager, JingleClient. The same order must be followed in the case
// of rejected and failed sessions.
//
// PROTOCOL VERSION NEGOTIATION
// When client connects to a host it sends a session-initiate stanza with list
// of supported configurations for each channel. If the host decides to accept
// session, then it selects configuration that is supported by both sides
// and then replies with the session-accept stanza that contans selected
// configuration. The configuration specified in the session-accept is used
// for the session.
//
// The CandidateSessionConfig class represents list of configurations
// supported by an endpoint. The |candidate_config| argument in the Connect()
// specifies configuration supported on the client side. When the host receives
// session-initiate stanza, the IncomingSessionCallback is called. The
// configuration sent in the session-intiate staza is available via
// ChromotocolConnnection::candidate_config(). If an incoming session is
// being accepted then the IncomingSessionCallback callback function must
// select session configuration and then set it with Session::set_config().

#ifndef REMOTING_PROTOCOL_SESSION_MANAGER_H_
#define REMOTING_PROTOCOL_SESSION_MANAGER_H_

#include <string>

#include "base/callback.h"
#include "base/ref_counted.h"
#include "remoting/protocol/session.h"

class Task;

namespace remoting {
namespace protocol {

// Generic interface for Chromoting session manager.
class SessionManager : public base::RefCountedThreadSafe<SessionManager> {
 public:
  enum IncomingSessionResponse {
    ACCEPT,
    INCOMPATIBLE,
    DECLINE,
  };

  // IncomingSessionCallback is called when a new session is received. If
  // the callback decides to accept the session it should set the second
  // argument to ACCEPT. Otherwise it should set it to DECLINE, or
  // INCOMPATIBLE. INCOMPATIBLE indicates that the session has incompatible
  // configuration, and cannot be accepted.
  // If the callback accepts session then it must also set configuration
  // for the new session using Session::set_config().
  typedef Callback2<Session*, IncomingSessionResponse*>::Type
      IncomingSessionCallback;

  // Initializes session to the host |jid|.  Ownership of the
  // |config| is passed to the new session.
  virtual scoped_refptr<Session> Connect(
      const std::string& jid,
      CandidateSessionConfig* config,
      Session::StateChangeCallback* state_change_callback) = 0;

  // Close session manager and all current sessions. |close_task| is executed
  // after the session client is actually closed. No callbacks are called after
  // |closed_task| is executed.
  virtual void Close(Task* closed_task) = 0;

 protected:
  friend class base::RefCountedThreadSafe<SessionManager>;

  SessionManager() { }
  virtual ~SessionManager() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionManager);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_SESSION_MANAGER_H_
