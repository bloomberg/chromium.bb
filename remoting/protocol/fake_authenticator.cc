// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_authenticator.h"

#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "net/base/io_buffer.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

FakeChannelAuthenticator::FakeChannelAuthenticator(bool accept, bool async)
    : result_(accept ? net::OK : net::ERR_FAILED),
      async_(async),
      did_read_bytes_(false),
      did_write_bytes_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

FakeChannelAuthenticator::~FakeChannelAuthenticator() {
}

void FakeChannelAuthenticator::SecureAndAuthenticate(
    scoped_ptr<net::StreamSocket> socket,
    const DoneCallback& done_callback) {
  socket_ = socket.Pass();

  if (async_) {
    done_callback_ = done_callback;

    scoped_refptr<net::IOBuffer> write_buf = new net::IOBuffer(1);
    write_buf->data()[0] = 0;
    int result = socket_->Write(
        write_buf, 1, base::Bind(&FakeChannelAuthenticator::OnAuthBytesWritten,
                                 weak_factory_.GetWeakPtr()));
    if (result != net::ERR_IO_PENDING) {
      // This will not call the callback because |did_read_bytes_| is
      // still set to false.
      OnAuthBytesWritten(result);
    }

    scoped_refptr<net::IOBuffer> read_buf = new net::IOBuffer(1);
    result = socket_->Read(
        read_buf, 1, base::Bind(&FakeChannelAuthenticator::OnAuthBytesRead,
                                weak_factory_.GetWeakPtr()));
    if (result != net::ERR_IO_PENDING)
      OnAuthBytesRead(result);
  } else {
    if (result_ != net::OK)
      socket_.reset();
    done_callback.Run(result_, socket_.Pass());
  }
}

void FakeChannelAuthenticator::OnAuthBytesWritten(int result) {
  EXPECT_EQ(1, result);
  EXPECT_FALSE(did_write_bytes_);
  did_write_bytes_ = true;
  if (did_read_bytes_)
    done_callback_.Run(result_, socket_.Pass());
}

void FakeChannelAuthenticator::OnAuthBytesRead(int result) {
  EXPECT_EQ(1, result);
  EXPECT_FALSE(did_read_bytes_);
  did_read_bytes_ = true;
  if (did_write_bytes_)
    done_callback_.Run(result_, socket_.Pass());
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

Authenticator::State FakeAuthenticator::state() const {
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

Authenticator::RejectionReason FakeAuthenticator::rejection_reason() const {
  EXPECT_EQ(REJECTED, state());
  return INVALID_CREDENTIALS;
}

void FakeAuthenticator::ProcessMessage(const buzz::XmlElement* message,
                                       const base::Closure& resume_callback) {
  EXPECT_EQ(WAITING_MESSAGE, state());
  std::string id =
      message->TextNamed(buzz::QName(kChromotingXmlNamespace, "id"));
  EXPECT_EQ(id, base::IntToString(messages_));
  ++messages_;
  resume_callback.Run();
}

scoped_ptr<buzz::XmlElement> FakeAuthenticator::GetNextMessage() {
  EXPECT_EQ(MESSAGE_READY, state());

  scoped_ptr<buzz::XmlElement> result(new buzz::XmlElement(
      buzz::QName(kChromotingXmlNamespace, "authentication")));
  buzz::XmlElement* id = new buzz::XmlElement(
      buzz::QName(kChromotingXmlNamespace, "id"));
  id->AddText(base::IntToString(messages_));
  result->AddElement(id);

  ++messages_;
  return result.Pass();
}

scoped_ptr<ChannelAuthenticator>
FakeAuthenticator::CreateChannelAuthenticator() const {
  EXPECT_EQ(ACCEPTED, state());
  return scoped_ptr<ChannelAuthenticator>(
      new FakeChannelAuthenticator(action_ != REJECT_CHANNEL, async_));
}

FakeHostAuthenticatorFactory::FakeHostAuthenticatorFactory(
    int round_trips, FakeAuthenticator::Action action, bool async)
    : round_trips_(round_trips),
      action_(action), async_(async) {
}

FakeHostAuthenticatorFactory::~FakeHostAuthenticatorFactory() {
}

scoped_ptr<Authenticator> FakeHostAuthenticatorFactory::CreateAuthenticator(
    const std::string& local_jid,
    const std::string& remote_jid,
    const buzz::XmlElement* first_message) {
  return scoped_ptr<Authenticator>(new FakeAuthenticator(
      FakeAuthenticator::HOST, round_trips_, action_, async_));
}

}  // namespace protocol
}  // namespace remoting
