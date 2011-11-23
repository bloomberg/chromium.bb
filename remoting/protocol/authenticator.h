// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_AUTHENTICATOR_H_

#include <string>

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace remoting {
namespace protocol {

class ChannelAuthenticator;

// Authenticator is an abstract interface for authentication protocol
// implementations. Different implementations of this interface may be
// used on each side of the connection depending of type of the auth
// protocol. Client and host will repeatedly call their Authenticators
// and deliver the messages they generate, until successful
// authentication is reported.
//
// Authenticator may exchange multiple messages before session is
// authenticated. Each message sent/received by an Authenticator is
// delivered either in a session description inside session-initiate
// and session-accept messages or in a session-info
// message. Session-info messages are used only if authenticators need
// to exchange more than one message.
class Authenticator {
 public:
  // Allowed state transitions:
  // When ProcessMessage() is called:
  //    WAITING_MESSAGE -> MESSAGE_READY
  //    WAITING_MESSAGE -> ACCEPTED
  //    WAITING_MESSAGE -> REJECTED
  // When GetNextMessage() is called:
  //    MESSAGE_READY -> WAITING_MESSAGE
  //    MESSAGE_READY -> ACCEPTED
  enum State {
    // Waiting for the next message from the peer.
    WAITING_MESSAGE,

    // Next message is ready to be sent to the peer.
    MESSAGE_READY,

    // Session is authenticated successufully.
    ACCEPTED,

    // Session is rejected.
    REJECTED,
  };

  Authenticator() {}
  virtual ~Authenticator() {}

  // Returns current state of the authenticator.
  virtual State state() const = 0;

  // Called in response to incoming message received from the peer.
  // Should only be called when in WAITING_MESSAGE state. Caller
  // retains ownership of |message|.
  virtual void ProcessMessage(const buzz::XmlElement* message) = 0;

  // Must be called when in MESSAGE_READY state. Returns next
  // authentication message that needs to be sent to the peer.
  virtual buzz::XmlElement* GetNextMessage() = 0;

  // Creates new authenticator for a channel. Caller must take
  // ownership of the result. Can be called only in the ACCEPTED
  // state.
  virtual ChannelAuthenticator* CreateChannelAuthenticator() const = 0;
};

// Factory for Authenticator instances.
class AuthenticatorFactory {
 public:
  AuthenticatorFactory() {}
  virtual ~AuthenticatorFactory() {}

  // Called when session-initiate stanza is received to create
  // authenticator for the new session. |first_message| specifies
  // authentication part of the session-initiate stanza so that
  // appropriate type of Authenticator can be chosen for the session
  // (useful when multiple authenticators is supported). Returns NULL
  // if the |first_message| is invalid and the session should be
  // rejected. ProcessMessage() should be called with |first_message|
  // for the result of this method.
  virtual Authenticator* CreateAuthenticator(
      const std::string& remote_jid,
      const buzz::XmlElement* first_message) = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_AUTHENTICATOR_H_
