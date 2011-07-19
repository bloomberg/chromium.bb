// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The purpose of SessionManager is to facilitate creation of chromotocol
// sessions. Both host and client use it to establish chromotocol
// sessions. JingleChromotocolServer implements this inteface using
// libjingle.
//
// OUTGOING SESSIONS
// Connect() must be used to create new session to a remote host. The
// returned session is initially in INITIALIZING state. Later state is
// changed to CONNECTED if the session is accepted by the host or
// CLOSED if the session is rejected.
//
// INCOMING SESSIONS
// The IncomingSessionCallback is called when a client attempts to connect.
// The callback function decides whether the session should be accepted or
// rejected.
//
// SESSION OWNERSHIP AND SHUTDOWN
// SessionManager owns all Sessions it creates. The manager must not
// be closed or destroyed before all sessions created by that
// SessionManager are destroyed. Caller owns Sessions created by a
// SessionManager (except rejected sessions). Sessions must outlive
// SessionManager, and SignalStrategy must outlive SessionManager.
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
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/protocol/session.h"

class Task;

namespace crypto {
class RSAPrivateKey;
}  // namespace base

namespace remoting {

class SignalStrategy;

namespace protocol {

// Generic interface for Chromoting session manager.
//
// TODO(sergeyu): Split this into two separate interfaces: one for the
// client side and one for the host side.
class SessionManager : public base::NonThreadSafe {
 public:
  SessionManager() { }
  virtual ~SessionManager() { }

  enum IncomingSessionResponse {
    ACCEPT,
    INCOMPATIBLE,
    DECLINE,
  };

  // IncomingSessionCallback is called when a new session is
  // received. If the callback decides to accept the session it should
  // set the second argument to ACCEPT. Otherwise it should set it to
  // DECLINE, or INCOMPATIBLE. INCOMPATIBLE indicates that the session
  // has incompatible configuration, and cannot be accepted.  If the
  // callback accepts session then it must also set configuration for
  // the new session using Session::set_config(). The callback must
  // take ownership of the session if it ACCEPTs it.
  typedef Callback2<Session*, IncomingSessionResponse*>::Type
      IncomingSessionCallback;

  // Initializes the session client. Caller retains ownership of the
  // |signal_strategy|. If this object is used in server mode, then
  // |private_key| and |certificate| are used to establish a secured
  // communication with the client. It will also take ownership of
  // these objects. In case this is used in client mode, pass in NULL
  // for both private key and certificate.
  virtual void Init(const std::string& local_jid,
                    SignalStrategy* signal_strategy,
                    IncomingSessionCallback* incoming_session_callback,
                    crypto::RSAPrivateKey* private_key,
                    const std::string& certificate) = 0;

  // Tries to create a session to the host |jid|.
  //
  // |host_jid| is the full jid of the host to connect to.
  // |host_public_key| is used to for authentication.
  // |client_oauth_token| is a short-lived OAuth token identify the client.
  // |config| contains the session configurations that the client supports.
  // |state_change_callback| is called when the connection state changes.
  //
  // Ownership of the |config| is passed to the new session.
  virtual Session* Connect(
      const std::string& host_jid,
      const std::string& host_public_key,
      const std::string& client_token,
      CandidateSessionConfig* config,
      Session::StateChangeCallback* state_change_callback) = 0;

  // Close session manager. Can be called only after all corresponding
  // sessions are destroyed. No callbacks are called after this method
  // returns.
  virtual void Close() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionManager);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_SESSION_MANAGER_H_
