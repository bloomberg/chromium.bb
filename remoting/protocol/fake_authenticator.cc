// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_authenticator.h"

#include <utility>

#include "base/base64.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/p2p_stream_socket.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

FakeChannelAuthenticator::FakeChannelAuthenticator(bool accept, bool async)
    : result_(accept ? net::OK : net::ERR_FAILED),
      async_(async),
      weak_factory_(this) {}

FakeChannelAuthenticator::~FakeChannelAuthenticator() {}

void FakeChannelAuthenticator::SecureAndAuthenticate(
    std::unique_ptr<P2PStreamSocket> socket,
    const DoneCallback& done_callback) {
  socket_ = std::move(socket);

  done_callback_ = done_callback;

  if (async_) {
    if (result_ != net::OK) {
      // Don't write anything if we are going to reject auth to make test
      // ordering deterministic.
      did_write_bytes_ = true;
    } else {
      scoped_refptr<net::IOBuffer> write_buf = new net::IOBuffer(1);
      write_buf->data()[0] = 0;
      int result = socket_->Write(
          write_buf.get(), 1,
          base::Bind(&FakeChannelAuthenticator::OnAuthBytesWritten,
                     weak_factory_.GetWeakPtr()));
      if (result != net::ERR_IO_PENDING) {
        // This will not call the callback because |did_read_bytes_| is
        // still set to false.
        OnAuthBytesWritten(result);
      }
    }

    scoped_refptr<net::IOBuffer> read_buf = new net::IOBuffer(1);
    int result =
        socket_->Read(read_buf.get(), 1,
                      base::Bind(&FakeChannelAuthenticator::OnAuthBytesRead,
                                 weak_factory_.GetWeakPtr()));
    if (result != net::ERR_IO_PENDING)
      OnAuthBytesRead(result);
  } else {
    CallDoneCallback();
  }
}

void FakeChannelAuthenticator::OnAuthBytesWritten(int result) {
  EXPECT_EQ(1, result);
  EXPECT_FALSE(did_write_bytes_);
  did_write_bytes_ = true;
  if (did_read_bytes_)
    CallDoneCallback();
}

void FakeChannelAuthenticator::OnAuthBytesRead(int result) {
  EXPECT_EQ(1, result);
  EXPECT_FALSE(did_read_bytes_);
  did_read_bytes_ = true;
  if (did_write_bytes_)
    CallDoneCallback();
}

void FakeChannelAuthenticator::CallDoneCallback() {
  if (result_ != net::OK)
    socket_.reset();
  base::ResetAndReturn(&done_callback_).Run(result_, std::move(socket_));
}

FakeAuthenticator::FakeAuthenticator(Type type,
                                     int round_trips,
                                     Action action,
                                     bool async)
    : type_(type), round_trips_(round_trips), action_(action), async_(async) {}

FakeAuthenticator::~FakeAuthenticator() {}

void FakeAuthenticator::set_messages_till_started(int messages) {
  messages_till_started_ = messages;
}

void FakeAuthenticator::Resume() {
  base::ResetAndReturn(&resume_closure_).Run();
}

Authenticator::State FakeAuthenticator::state() const {
  EXPECT_LE(messages_, round_trips_ * 2);

  if (messages_ == pause_message_index_ && !resume_closure_.is_null())
    return PROCESSING_MESSAGE;

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

bool FakeAuthenticator::started() const {
  return messages_ > messages_till_started_;
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

  // On the client receive the key in the last message.
  if (type_ == CLIENT && messages_ == round_trips_ * 2 - 1) {
    std::string key_base64 =
        message->TextNamed(buzz::QName(kChromotingXmlNamespace, "key"));
    EXPECT_TRUE(!key_base64.empty());
    EXPECT_TRUE(base::Base64Decode(key_base64, &auth_key_));
  }

  ++messages_;
  if (messages_ == pause_message_index_) {
    resume_closure_ = resume_callback;
    return;
  }
  resume_callback.Run();
}

std::unique_ptr<buzz::XmlElement> FakeAuthenticator::GetNextMessage() {
  EXPECT_EQ(MESSAGE_READY, state());

  std::unique_ptr<buzz::XmlElement> result(new buzz::XmlElement(
      buzz::QName(kChromotingXmlNamespace, "authentication")));
  buzz::XmlElement* id = new buzz::XmlElement(
      buzz::QName(kChromotingXmlNamespace, "id"));
  id->AddText(base::IntToString(messages_));
  result->AddElement(id);

  // Add authentication key in the last message sent from host to client.
  if (type_ == HOST && messages_ == round_trips_ * 2 - 1) {
    auth_key_ =  base::RandBytesAsString(16);
    buzz::XmlElement* key = new buzz::XmlElement(
        buzz::QName(kChromotingXmlNamespace, "key"));
    std::string key_base64;
    base::Base64Encode(auth_key_, &key_base64);
    key->AddText(key_base64);
    result->AddElement(key);
  }

  ++messages_;
  return result;
}

const std::string& FakeAuthenticator::GetAuthKey() const {
  EXPECT_EQ(ACCEPTED, state());
  DCHECK(!auth_key_.empty());
  return auth_key_;
}

std::unique_ptr<ChannelAuthenticator>
FakeAuthenticator::CreateChannelAuthenticator() const {
  EXPECT_EQ(ACCEPTED, state());
  return base::MakeUnique<FakeChannelAuthenticator>(action_ != REJECT_CHANNEL,
                                                    async_);
}

FakeHostAuthenticatorFactory::FakeHostAuthenticatorFactory(
    int round_trips,
    int messages_till_started,
    FakeAuthenticator::Action action,
    bool async)
    : round_trips_(round_trips),
      messages_till_started_(messages_till_started),
      action_(action),
      async_(async) {}
FakeHostAuthenticatorFactory::~FakeHostAuthenticatorFactory() {}

std::unique_ptr<Authenticator>
FakeHostAuthenticatorFactory::CreateAuthenticator(
    const std::string& local_jid,
    const std::string& remote_jid) {
  std::unique_ptr<FakeAuthenticator> authenticator(new FakeAuthenticator(
      FakeAuthenticator::HOST, round_trips_, action_, async_));
  authenticator->set_messages_till_started(messages_till_started_);
  return std::move(authenticator);
}

}  // namespace protocol
}  // namespace remoting
