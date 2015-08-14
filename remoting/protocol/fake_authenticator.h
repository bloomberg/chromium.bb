// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_FAKE_AUTHENTICATOR_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"

namespace remoting {
namespace protocol {

class FakeChannelAuthenticator : public ChannelAuthenticator {
 public:
  FakeChannelAuthenticator(bool accept, bool async);
  ~FakeChannelAuthenticator() override;

  // ChannelAuthenticator interface.
  void SecureAndAuthenticate(scoped_ptr<P2PStreamSocket> socket,
                             const DoneCallback& done_callback) override;

 private:
  void OnAuthBytesWritten(int result);
  void OnAuthBytesRead(int result);

  void CallDoneCallback();

  int result_;
  bool async_;

  scoped_ptr<P2PStreamSocket> socket_;
  DoneCallback done_callback_;

  bool did_read_bytes_;
  bool did_write_bytes_;

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

  ~FakeAuthenticator() override;

  // Set the number of messages that the authenticator needs to process before
  // started() returns true.  Default to 0.
  void set_messages_till_started(int messages);

  // Authenticator interface.
  State state() const override;
  bool started() const override;
  RejectionReason rejection_reason() const override;
  void ProcessMessage(const buzz::XmlElement* message,
                      const base::Closure& resume_callback) override;
  scoped_ptr<buzz::XmlElement> GetNextMessage() override;
  const std::string& GetAuthKey() const override;
  scoped_ptr<ChannelAuthenticator> CreateChannelAuthenticator() const override;

 protected:
  Type type_;
  int round_trips_;
  Action action_;
  bool async_;

  // Total number of messages that have been processed.
  int messages_;
  // Number of messages that the authenticator needs to process before started()
  // returns true.  Default to 0.
  int messages_till_started_;

  std::string auth_key_;

  DISALLOW_COPY_AND_ASSIGN(FakeAuthenticator);
};

class FakeHostAuthenticatorFactory : public AuthenticatorFactory {
 public:
  FakeHostAuthenticatorFactory(
      int round_trips, int messages_till_start,
      FakeAuthenticator::Action action, bool async);
  ~FakeHostAuthenticatorFactory() override;

  // AuthenticatorFactory interface.
  scoped_ptr<Authenticator> CreateAuthenticator(
      const std::string& local_jid,
      const std::string& remote_jid,
      const buzz::XmlElement* first_message) override;

 private:
  int round_trips_;
  int messages_till_started_;
  FakeAuthenticator::Action action_;
  bool async_;

  DISALLOW_COPY_AND_ASSIGN(FakeHostAuthenticatorFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_FAKE_AUTHENTICATOR_H_
