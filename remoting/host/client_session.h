// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CLIENT_SESSION_H_
#define REMOTING_HOST_CLIENT_SESSION_H_

#include <list>
#include <set>

#include "base/time.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"
#include "ui/gfx/point.h"

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

  void set_awaiting_continue_approval(bool awaiting) {
    awaiting_continue_approval_ = awaiting;
  }

  const std::string& client_jid() { return client_jid_; }

  // Indicate that local mouse activity has been detected. This causes remote
  // inputs to be ignored for a short time so that the local user will always
  // have the upper hand in 'pointer wars'.
  void LocalMouseMoved(const gfx::Point& new_pos);

  bool ShouldIgnoreRemoteMouseInput(const protocol::MouseEvent* event) const;
  bool ShouldIgnoreRemoteKeyboardInput(const protocol::KeyEvent* event) const;

 private:
  friend class base::RefCountedThreadSafe<ClientSession>;
  friend class ClientSessionTest_UnpressKeys_Test;
  virtual ~ClientSession();

  // Keep track of keydowns and keyups so that we can clean up the keyboard
  // state when the user disconnects.
  void RecordKeyEvent(const protocol::KeyEvent* event);

  // Synthesize KeyUp events for keys that have been pressed but not released.
  // This should be used when the client has disconnected to clear out any
  // pending key events.
  void UnpressKeys();

  EventHandler* event_handler_;

  // A factory for user authenticators.
  scoped_ptr<UserAuthenticator> user_authenticator_;

  // The connection to the client.
  scoped_refptr<protocol::ConnectionToClient> connection_;

  std::string client_jid_;

  // The input stub to which this object delegates.
  protocol::InputStub* input_stub_;

  // Whether this client is authenticated.
  bool authenticated_;

  // Whether or not inputs from this client are blocked pending approval from
  // the host user to continue the connection.
  bool awaiting_continue_approval_;

  // State to control remote input blocking while the local pointer is in use.
  uint32 remote_mouse_button_state_;

  // Queue of recently-injected mouse positions.  This is used to detect whether
  // mouse events from the local input monitor are echoes of injected positions,
  // or genuine mouse movements of a local input device.
  std::list<gfx::Point> injected_mouse_positions_;

  base::Time latest_local_input_time_;
  std::set<int> pressed_keys_;

  DISALLOW_COPY_AND_ASSIGN(ClientSession);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CLIENT_SESSION_H_
