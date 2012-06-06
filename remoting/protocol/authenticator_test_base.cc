// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/authenticator_test_base.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "crypto/rsa_private_key.h"
#include "net/base/cert_test_util.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/fake_session.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using testing::_;
using testing::SaveArg;

namespace remoting {
namespace protocol {

AuthenticatorTestBase::MockChannelDoneCallback::MockChannelDoneCallback() {
}
AuthenticatorTestBase::MockChannelDoneCallback::~MockChannelDoneCallback() {
}

AuthenticatorTestBase::AuthenticatorTestBase() {
}
AuthenticatorTestBase::~AuthenticatorTestBase() {
}

void AuthenticatorTestBase::SetUp() {
  FilePath certs_dir(net::GetTestCertsDirectory());

  FilePath cert_path = certs_dir.AppendASCII("unittest.selfsigned.der");
  ASSERT_TRUE(file_util::ReadFileToString(cert_path, &host_cert_));

  FilePath key_path = certs_dir.AppendASCII("unittest.key.bin");
  std::string key_string;
  ASSERT_TRUE(file_util::ReadFileToString(key_path, &key_string));
  std::vector<uint8> key_vector(
      reinterpret_cast<const uint8*>(key_string.data()),
      reinterpret_cast<const uint8*>(key_string.data() +
                                     key_string.length()));
  private_key_.reset(
      crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(key_vector));
}

void AuthenticatorTestBase::RunAuthExchange() {
  do {
    scoped_ptr<buzz::XmlElement> message;

    // Pass message from client to host.
    ASSERT_EQ(Authenticator::MESSAGE_READY, client_->state());
    message = client_->GetNextMessage();
    ASSERT_TRUE(message.get());
    ASSERT_NE(Authenticator::MESSAGE_READY, client_->state());

    ASSERT_EQ(Authenticator::WAITING_MESSAGE, host_->state());
    host_->ProcessMessage(message.get());
    ASSERT_NE(Authenticator::WAITING_MESSAGE, host_->state());

    // Are we done yet?
    if (host_->state() == Authenticator::ACCEPTED ||
        host_->state() == Authenticator::REJECTED) {
      break;
    }

    // Pass message from host to client.
    ASSERT_EQ(Authenticator::MESSAGE_READY, host_->state());
    message = host_->GetNextMessage();
    ASSERT_TRUE(message.get());
    ASSERT_NE(Authenticator::MESSAGE_READY, host_->state());

    ASSERT_EQ(Authenticator::WAITING_MESSAGE, client_->state());
    client_->ProcessMessage(message.get());
    ASSERT_NE(Authenticator::WAITING_MESSAGE, client_->state());
  } while (client_->state() != Authenticator::ACCEPTED &&
           client_->state() != Authenticator::REJECTED);
}

void AuthenticatorTestBase::RunChannelAuth(bool expected_fail) {
  client_fake_socket_.reset(new FakeSocket());
  host_fake_socket_.reset(new FakeSocket());
  client_fake_socket_->PairWith(host_fake_socket_.get());

  client_auth_->SecureAndAuthenticate(
      client_fake_socket_.PassAs<net::StreamSocket>(),
      base::Bind(&AuthenticatorTestBase::OnClientConnected,
                 base::Unretained(this)));

  host_auth_->SecureAndAuthenticate(
      host_fake_socket_.PassAs<net::StreamSocket>(),
      base::Bind(&AuthenticatorTestBase::OnHostConnected,
                 base::Unretained(this)));

  net::StreamSocket* client_socket = NULL;
  net::StreamSocket* host_socket = NULL;

  EXPECT_CALL(client_callback_, OnDone(net::OK, _))
      .WillOnce(SaveArg<1>(&client_socket));
  if (expected_fail) {
    EXPECT_CALL(host_callback_, OnDone(net::ERR_FAILED, NULL));
  } else {
    EXPECT_CALL(host_callback_, OnDone(net::OK, _))
        .WillOnce(SaveArg<1>(&host_socket));
  }

  message_loop_.RunAllPending();

  testing::Mock::VerifyAndClearExpectations(&client_callback_);
  testing::Mock::VerifyAndClearExpectations(&host_callback_);

  client_socket_.reset(client_socket);
  host_socket_.reset(host_socket);
}

void AuthenticatorTestBase::OnHostConnected(
    net::Error error,
    scoped_ptr<net::StreamSocket> socket) {
  host_callback_.OnDone(error, socket.get());
  host_socket_ = socket.Pass();
}

void AuthenticatorTestBase::OnClientConnected(
    net::Error error,
    scoped_ptr<net::StreamSocket> socket) {
  client_callback_.OnDone(error, socket.get());
  client_socket_ = socket.Pass();
}

}  // namespace protocol
}  // namespace remoting
