// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CLIENT_SESSION_H_
#define REMOTING_HOST_CLIENT_SESSION_H_

#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"

namespace remoting {

class UserAuthenticator;

// A ClientSession keeps a reference to a connection to a client, and maintains
// per-client state.
class ClientSession : public protocol::HostStub,
                      public protocol::InputStub,
                      public base::RefCountedThreadSafe<ClientSession> {
 public:
  // Callback interface for passing events to the ChromotingHost.
  class EventHandler {
   public:
    virtual ~EventHandler() {}

    // Called to signal that local login has succeeded and ChromotingHost can
    // proceed with the next step.
    virtual void LocalLoginSucceeded(
        scoped_refptr<protocol::ConnectionToClient> client) = 0;

    // Called to signal that local login has failed.
    virtual void LocalLoginFailed(
        scoped_refptr<protocol::ConnectionToClient> client) = 0;
  };

  // Takes ownership of |user_authenticator|. Does not take ownership of
  // |event_handler| or |input_stub|.
  ClientSession(EventHandler* event_handler,
                UserAuthenticator* user_authenticator,
                scoped_refptr<protocol::ConnectionToClient> connection,
                protocol::InputStub* input_stub);

  // protocol::HostStub interface.
  virtual void SuggestResolution(
      const protocol::SuggestResolutionRequest* msg, Task* done);
  virtual void BeginSessionRequest(
      const protocol::LocalLoginCredentials* credentials, Task* done);

  // protocol::InputStub interface.
  virtual void InjectKeyEvent(const protocol::KeyEvent* event, Task* done);
  virtual void InjectMouseEvent(const protocol::MouseEvent* event, Task* done);

  // Disconnect this client session.
  void Disconnect();

  // Set the authenticated flag or log a failure message as appropriate.
  void OnAuthorizationComplete(bool success);

  protocol::ConnectionToClient* connection() const {
    return connection_.get();
  }

  bool authenticated() const {
    return authenticated_;
  }

 private:
  friend class base::RefCountedThreadSafe<ClientSession>;
  virtual ~ClientSession();

  EventHandler* event_handler_;

  // A factory for user authenticators.
  scoped_ptr<UserAuthenticator> user_authenticator_;

  // The connection to the client.
  scoped_refptr<protocol::ConnectionToClient> connection_;

  // The input stub to which this object delegates.
  protocol::InputStub* input_stub_;

  // Whether this client is authenticated.
  bool authenticated_;

  DISALLOW_COPY_AND_ASSIGN(ClientSession);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CLIENT_SESSION_H_
