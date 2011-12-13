// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_FAKE_AUTHENTICATOR_H_

#include "base/memory/weak_ptr.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"

namespace remoting {
namespace protocol {

class FakeChannelAuthenticator : public ChannelAuthenticator {
 public:
  FakeChannelAuthenticator(bool accept, bool async);
  virtual ~FakeChannelAuthenticator();

  // ChannelAuthenticator interface.
  virtual void SecureAndAuthenticate(
      net::StreamSocket* socket, const DoneCallback& done_callback) OVERRIDE;

 private:
  void CallCallback(
      const DoneCallback& done_callback,
      net::Error error,
      net::StreamSocket* socket);

  bool accept_;
  bool async_;

  base::WeakPtrFactory<FakeChannelAuthenticator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeChannelAuthenticator);
};

class FakeAuthenticator : public Authenticator {
 public:
  enum Type {
    HOST,
    CLIENT,
  };

  enum Action {
    ACCEPT,
    REJECT,
    REJECT_CHANNEL
  };

  FakeAuthenticator(Type type, int round_trips, Action action, bool async);
  virtual ~FakeAuthenticator();

  // Authenticator interface.
  virtual State state() const OVERRIDE;
  virtual void ProcessMessage(const buzz::XmlElement* message) OVERRIDE;
  virtual buzz::XmlElement* GetNextMessage() OVERRIDE;
  virtual ChannelAuthenticator* CreateChannelAuthenticator() const OVERRIDE;

 protected:
  Type type_;
  int round_trips_;
  Action action_;
  bool async_;

  // Total number of messages that have been processed.
  int messages_;

  DISALLOW_COPY_AND_ASSIGN(FakeAuthenticator);
};

class FakeHostAuthenticatorFactory : public AuthenticatorFactory {
 public:
  FakeHostAuthenticatorFactory(
      int round_trips, FakeAuthenticator::Action action, bool async);
  virtual ~FakeHostAuthenticatorFactory();

  // AuthenticatorFactory interface.
  virtual Authenticator* CreateAuthenticator(
      const std::string& remote_jid,
      const buzz::XmlElement* first_message) OVERRIDE;

 private:
  int round_trips_;
  FakeAuthenticator::Action action_;
  bool async_;

  DISALLOW_COPY_AND_ASSIGN(FakeHostAuthenticatorFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_FAKE_AUTHENTICATOR_H_
