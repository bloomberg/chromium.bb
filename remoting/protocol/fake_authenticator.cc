// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_authenticator.h"

#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

FakeChannelAuthenticator::FakeChannelAuthenticator(bool accept, bool async)
    : accept_(accept),
      async_(async),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

FakeChannelAuthenticator::~FakeChannelAuthenticator() {
}

void FakeChannelAuthenticator::SecureAndAuthenticate(
    net::StreamSocket* socket, const DoneCallback& done_callback) {
  net::Error error;

  if (accept_) {
    error = net::OK;
  } else {
    error = net::ERR_FAILED;
    delete socket;
    socket = NULL;
  }

  if (async_) {
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &FakeChannelAuthenticator::CallCallback, weak_factory_.GetWeakPtr(),
        done_callback, error, socket));
  } else {
    done_callback.Run(error, socket);
  }
}

void FakeChannelAuthenticator::CallCallback(
    const DoneCallback& done_callback,
    net::Error error,
    net::StreamSocket* socket) {
  done_callback.Run(error, socket);
}

FakeAuthenticator::FakeAuthenticator(
    Type type, int round_trips, Action action, bool async)
    : type_(type),
      round_trips_(round_trips),
      action_(action),
      async_(async),
      messages_(0) {
}

FakeAuthenticator::~FakeAuthenticator() {
}

Authenticator::State FakeAuthenticator::state() const{
  EXPECT_LE(messages_, round_trips_ * 2);
  if (messages_ >= round_trips_ * 2) {
    if (action_ == REJECT) {
      return REJECTED;
    } else {
      return ACCEPTED;
    }
  }

  // Don't send the last message if this is a host that wants to
  // reject a connection.
  if (messages_ == round_trips_ * 2 - 1 &&
      type_ == HOST && action_ == REJECT) {
    return REJECTED;
  }

  // We are not done yet. process next message.
  if ((messages_ % 2 == 0 && type_ == CLIENT) ||
      (messages_ % 2 == 1 && type_ == HOST)) {
    return MESSAGE_READY;
  } else {
    return WAITING_MESSAGE;
  }
}

void FakeAuthenticator::ProcessMessage(const buzz::XmlElement* message) {
  EXPECT_EQ(WAITING_MESSAGE, state());
  std::string id =
      message->TextNamed(buzz::QName(kChromotingXmlNamespace, "id"));
  EXPECT_EQ(id, base::IntToString(messages_));
  ++messages_;
}

buzz::XmlElement* FakeAuthenticator::GetNextMessage() {
  EXPECT_EQ(MESSAGE_READY, state());

  buzz::XmlElement* result = new buzz::XmlElement(
      buzz::QName(kChromotingXmlNamespace, "authentication"));
  buzz::XmlElement* id = new buzz::XmlElement(
      buzz::QName(kChromotingXmlNamespace, "id"));
  id->AddText(base::IntToString(messages_));
  result->AddElement(id);

  ++messages_;
  return result;
}

ChannelAuthenticator*
FakeAuthenticator::CreateChannelAuthenticator() const {
  EXPECT_EQ(ACCEPTED, state());
  return new FakeChannelAuthenticator(action_ != REJECT_CHANNEL, async_);
}

FakeHostAuthenticatorFactory::FakeHostAuthenticatorFactory(
    int round_trips, FakeAuthenticator::Action action, bool async)
    : round_trips_(round_trips),
      action_(action), async_(async) {
}

FakeHostAuthenticatorFactory::~FakeHostAuthenticatorFactory() {
}

Authenticator* FakeHostAuthenticatorFactory::CreateAuthenticator(
    const std::string& remote_jid,
    const buzz::XmlElement* first_message) {
  return new FakeAuthenticator(FakeAuthenticator::HOST, round_trips_,
                               action_, async_);
}

}  // namespace protocol
}  // namespace remoting
