// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/authenticator_test_base.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/test_timeouts.h"
#include "base/timer.h"
#include "crypto/rsa_private_key.h"
#include "net/base/test_data_directory.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/fake_session.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using testing::_;
using testing::SaveArg;

namespace remoting {
namespace protocol {

namespace {

ACTION_P(QuitThreadOnCounter, counter) {
  --(*counter);
  EXPECT_GE(*counter, 0);
  if (*counter == 0)
    MessageLoop::current()->Quit();
}

}  // namespace

AuthenticatorTestBase::MockChannelDoneCallback::MockChannelDoneCallback() {}

AuthenticatorTestBase::MockChannelDoneCallback::~MockChannelDoneCallback() {}

AuthenticatorTestBase::AuthenticatorTestBase() {}

AuthenticatorTestBase::~AuthenticatorTestBase() {}

void AuthenticatorTestBase::SetUp() {
  base::FilePath certs_dir(net::GetTestCertsDirectory());

  base::FilePath cert_path = certs_dir.AppendASCII("unittest.selfsigned.der");
  ASSERT_TRUE(file_util::ReadFileToString(cert_path, &host_cert_));

  base::FilePath key_path = certs_dir.AppendASCII("unittest.key.bin");
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
  ContinueAuthExchangeWith(client_.get(), host_.get());
}

// static
void AuthenticatorTestBase::ContinueAuthExchangeWith(Authenticator* sender,
                                                     Authenticator* receiver) {
  scoped_ptr<buzz::XmlElement> message;
  ASSERT_NE(Authenticator::WAITING_MESSAGE, sender->state());
  if (sender->state() == Authenticator::ACCEPTED ||
      sender->state() == Authenticator::REJECTED)
    return;
  // Pass message from client to host.
  ASSERT_EQ(Authenticator::MESSAGE_READY, sender->state());
  message = sender->GetNextMessage();
  ASSERT_TRUE(message.get());
  ASSERT_NE(Authenticator::MESSAGE_READY, sender->state());

  ASSERT_EQ(Authenticator::WAITING_MESSAGE, receiver->state());
  receiver->ProcessMessage(message.get(), base::Bind(
      &AuthenticatorTestBase::ContinueAuthExchangeWith,
      base::Unretained(receiver), base::Unretained(sender)));
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

  // Expect two callbacks to be called - the client callback and the host
  // callback.
  int callback_counter = 2;

  EXPECT_CALL(client_callback_, OnDone(net::OK))
      .WillOnce(QuitThreadOnCounter(&callback_counter));
  if (expected_fail) {
    EXPECT_CALL(host_callback_, OnDone(net::ERR_FAILED))
         .WillOnce(QuitThreadOnCounter(&callback_counter));
  } else {
    EXPECT_CALL(host_callback_, OnDone(net::OK))
        .WillOnce(QuitThreadOnCounter(&callback_counter));
  }

  // Ensure that .Run() does not run unbounded if the callbacks are never
  // called.
  base::Timer shutdown_timer(false, false);
  shutdown_timer.Start(FROM_HERE, TestTimeouts::action_timeout(),
                       MessageLoop::QuitClosure());
  message_loop_.Run();
  shutdown_timer.Stop();

  testing::Mock::VerifyAndClearExpectations(&client_callback_);
  testing::Mock::VerifyAndClearExpectations(&host_callback_);

  if (!expected_fail) {
    ASSERT_TRUE(client_socket_.get() != NULL);
    ASSERT_TRUE(host_socket_.get() != NULL);
  }
}

void AuthenticatorTestBase::OnHostConnected(
    net::Error error,
    scoped_ptr<net::StreamSocket> socket) {
  host_callback_.OnDone(error);
  host_socket_ = socket.Pass();
}

void AuthenticatorTestBase::OnClientConnected(
    net::Error error,
    scoped_ptr<net::StreamSocket> socket) {
  client_callback_.OnDone(error);
  client_socket_ = socket.Pass();
}

}  // namespace protocol
}  // namespace remoting
