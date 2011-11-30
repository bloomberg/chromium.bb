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

namespace remoting {

class SignalStrategy;

namespace protocol {

class Authenticator;
class AuthenticatorFactory;

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

  class Listener {
   public:
    Listener() { }
    ~Listener() { }

    // Called when the session manager has finished
    // initialization. May be called from Init() or after Init()
    // returns. Outgoing connections can be created after this method
    // is called.
    virtual void OnSessionManagerInitialized() = 0;

    // Called when a new session is received. If the host decides to
    // accept the session it should set the |response| to
    // ACCEPT. Otherwise it should set it to DECLINE, or
    // INCOMPATIBLE. INCOMPATIBLE indicates that the session has
    // incompatible configuration, and cannot be accepted. If the
    // callback accepts the |session| then it must also set
    // configuration for the |session| using Session::set_config().
    // The callback must take ownership of the |session| if it ACCEPTs it.
    virtual void OnIncomingSession(Session* session,
                                   IncomingSessionResponse* response) = 0;
  };

  // Initializes the session client. Caller retains ownership of the
  // |signal_strategy| and |listener|. |allow_nat_traversal| must be
  // set to true to enable NAT traversal. STUN/Relay servers are not
  // used when NAT traversal is disabled, so P2P connection will work
  // only when both peers are on the same network. If this object is
  // used in server mode, then |private_key| and |certificate| are
  // used to establish a secured communication with the client. It
  // will also take ownership of these objects. On the client side
  // pass in NULL for |private_key| and empty string for
  // |certificate|.
  virtual void Init(const std::string& local_jid,
                    SignalStrategy* signal_strategy,
                    Listener* listener,
                    bool allow_nat_traversal) = 0;

  // Tries to create a session to the host |jid|. Must be called only
  // after initialization has finished successfully, i.e. after
  // Listener::OnInitialized() has been called.
  //
  // |host_jid| is the full jid of the host to connect to.
  // |host_public_key| is used to for authentication.
  // |authenticator| is a client authenticator for the session.
  // |config| contains the session configurations that the client supports.
  // |state_change_callback| is called when the connection state changes.
  //
  // Ownership of the |config| is passed to the new session.
  virtual Session* Connect(
      const std::string& host_jid,
      Authenticator* authenticator,
      CandidateSessionConfig* config,
      const Session::StateChangeCallback& state_change_callback) = 0;

  // Close session manager. Can be called only after all corresponding
  // sessions are destroyed. No callbacks are called after this method
  // returns.
  virtual void Close() = 0;

  // Set authenticator factory that should be used to authenticate
  // incoming connection. No connections will be accepted if
  // authenticator factory isn't set.
  virtual void set_authenticator_factory(
      AuthenticatorFactory* authenticator_factory) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionManager);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_SESSION_MANAGER_H_
