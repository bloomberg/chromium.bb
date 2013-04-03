// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_NEGOTIATING_AUTHENTICATOR_BASE_H_
#define REMOTING_PROTOCOL_NEGOTIATING_AUTHENTICATOR_BASE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/authentication_method.h"
#include "remoting/protocol/authenticator.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

// This class provides the common base for a meta-authenticator that allows
// clients and hosts that support multiple authentication methods to negotiate a
// method to use.
//
// The typical flow is:
//  * Client sends a message to host with its supported methods.
//      (clients may additionally pick a method and send its first message).
//  * Host picks a method and sends its first message (if any).
//      (if a message for that method was sent by the client, it is processed).
//  * Client creates the authenticator selected by the host. If the method
//      starts with a message from the host, it is processed.
//  * Client and host exchange messages until the authentication is ACCEPTED or
//      REJECTED.
//
// The details:
//  * CreateAuthenticator() may be asynchronous (i.e. require user interaction
//      to determine initial parameters, like PIN). This happens inside
//      ProcessMessage, so to the outside this behaves like any asynchronous
//      message processing. Internally, CreateAuthenticator() receives a
//      callback, that will resume the authentication once the authenticator is
//      created. If there is already a message to be processed by the new
//      authenticator, this callback includes a call to the underlying
//      ProcessMessage().
//  * Some authentication methods may have a specific starting direction (e.g.
//      host always sends the first message), while others are versatile (e.g.
//      SPAKE, where either side can send the first message). When an
//      authenticator is created, it is given a preferred initial state, which
//      the authenticator may ignore.
//  * If the new authenticator state doesn't match the preferred one,
//      the NegotiatingAuthenticator deals with that, by sending an empty
//      <authenticator> stanza if the method has no message to send, and
//      ignoring such empty messages on the receiving end.
//  * The client may optimistically pick a method on its first message (assuming
//      it doesn't require user interaction to start). If the host doesn't
//      support that method, it will just discard that message, and choose
//      another method from the client's supported methods list.
//  * The host never sends its own supported methods back to the client, so once
//      the host picks a method from the client's list, it's final.
//  * Any change in this class must maintain compatibility between any version
//      mix of webapp, client plugin and host, for both Me2Me and IT2Me.
class NegotiatingAuthenticatorBase : public Authenticator {
 public:
  virtual ~NegotiatingAuthenticatorBase();

  // Authenticator interface.
  virtual State state() const OVERRIDE;
  virtual RejectionReason rejection_reason() const OVERRIDE;
  virtual scoped_ptr<ChannelAuthenticator>
      CreateChannelAuthenticator() const OVERRIDE;

  // Calls |current_authenticator_| to process |message|, passing the supplied
  // |resume_callback|.
  void ProcessMessageInternal(const buzz::XmlElement* message,
                                      const base::Closure& resume_callback);

 protected:
  static const buzz::StaticQName kMethodAttributeQName;
  static const buzz::StaticQName kSupportedMethodsAttributeQName;
  static const char kSupportedMethodsSeparator;

  explicit NegotiatingAuthenticatorBase(Authenticator::State initial_state);

  void AddMethod(const AuthenticationMethod& method);

  // Updates |state_| to reflect the current underlying authenticator state.
  // |resume_callback| is called after the state is updated.
  void UpdateState(const base::Closure& resume_callback);

  // Gets the next message from |current_authenticator_|, if any, and fills in
  // the 'method' tag with |current_method_|.
  virtual scoped_ptr<buzz::XmlElement> GetNextMessageInternal();

  std::vector<AuthenticationMethod> methods_;
  AuthenticationMethod current_method_;
  scoped_ptr<Authenticator> current_authenticator_;
  State state_;
  RejectionReason rejection_reason_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NegotiatingAuthenticatorBase);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_NEGOTIATING_AUTHENTICATOR_BASE_H_
