// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CLIENT_SESSION_H_
#define REMOTING_HOST_CLIENT_SESSION_H_

#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/host_stub.h"

namespace remoting {

// A ClientSession keeps a reference to a connection to a client, and maintains
// per-client state.
class ClientSession : public protocol::HostStub,
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

  ClientSession(EventHandler* event_handler,
                scoped_refptr<protocol::ConnectionToClient> connection);

  // protocol::HostStub interface.
  virtual void SuggestResolution(
      const protocol::SuggestResolutionRequest* msg, Task* done);
  virtual void BeginSessionRequest(
      const protocol::LocalLoginCredentials* credentials, Task* done);

  // Disconnect this client session.
  void Disconnect();

  protocol::ConnectionToClient* connection() const;

 protected:
  friend class base::RefCountedThreadSafe<ClientSession>;
  ~ClientSession();

 private:
  EventHandler* event_handler_;
  scoped_refptr<protocol::ConnectionToClient> connection_;

  DISALLOW_COPY_AND_ASSIGN(ClientSession);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CLIENT_SESSION_H_
