// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ssl_hmac_channel_authenticator.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/test/test_timeouts.h"
#include "base/timer.h"
#include "crypto/rsa_private_key.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/fake_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using testing::_;
using testing::NotNull;
using testing::SaveArg;

namespace remoting {
namespace protocol {

namespace {

const char kTestSharedSecret[] = "1234-1234-5678";
const char kTestSharedSecretBad[] = "0000-0000-0001";

class MockChannelDoneCallback {
 public:
  MOCK_METHOD2(OnDone, void(net::Error error, net::StreamSocket* socket));
};

ACTION_P(QuitThreadOnCounter, counter) {
  --(*counter);
  EXPECT_GE(*counter, 0);
  if (*counter == 0)
    MessageLoop::current()->Quit();
}

}  // namespace

class SslHmacChannelAuthenticatorTest : public testing::Test {
 public:
  SslHmacChannelAuthenticatorTest() {}
  virtual ~SslHmacChannelAuthenticatorTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
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

  void RunChannelAuth(bool expected_fail) {
    client_fake_socket_.reset(new FakeSocket());
    host_fake_socket_.reset(new FakeSocket());
    client_fake_socket_->PairWith(host_fake_socket_.get());

    client_auth_->SecureAndAuthenticate(
        client_fake_socket_.PassAs<net::StreamSocket>(),
        base::Bind(&SslHmacChannelAuthenticatorTest::OnClientConnected,
                   base::Unretained(this)));

    host_auth_->SecureAndAuthenticate(
        host_fake_socket_.PassAs<net::StreamSocket>(),
        base::Bind(&SslHmacChannelAuthenticatorTest::OnHostConnected,
                   base::Unretained(this)));

    // Expect two callbacks to be called - the client callback and the host
    // callback.
    int callback_counter = 2;

    if (expected_fail) {
      EXPECT_CALL(client_callback_, OnDone(net::ERR_FAILED, NULL))
          .WillOnce(QuitThreadOnCounter(&callback_counter));
      EXPECT_CALL(host_callback_, OnDone(net::ERR_FAILED, NULL))
          .WillOnce(QuitThreadOnCounter(&callback_counter));
    } else {
      EXPECT_CALL(client_callback_, OnDone(net::OK, NotNull()))
          .WillOnce(QuitThreadOnCounter(&callback_counter));
      EXPECT_CALL(host_callback_, OnDone(net::OK, NotNull()))
          .WillOnce(QuitThreadOnCounter(&callback_counter));
    }

    // Ensure that .Run() does not run unbounded if the callbacks are never
    // called.
    base::Timer shutdown_timer(false, false);
    shutdown_timer.Start(FROM_HERE, TestTimeouts::action_timeout(),
                         MessageLoop::QuitClosure());
    message_loop_.Run();
  }

  void OnHostConnected(net::Error error,
                       scoped_ptr<net::StreamSocket> socket) {
    host_callback_.OnDone(error, socket.get());
    host_socket_ = socket.Pass();
  }

  void OnClientConnected(net::Error error,
                         scoped_ptr<net::StreamSocket> socket) {
    client_callback_.OnDone(error, socket.get());
    client_socket_ = socket.Pass();
  }

  MessageLoop message_loop_;

  scoped_ptr<crypto::RSAPrivateKey> private_key_;
  std::string host_cert_;
  scoped_ptr<FakeSocket> client_fake_socket_;
  scoped_ptr<FakeSocket> host_fake_socket_;
  scoped_ptr<ChannelAuthenticator> client_auth_;
  scoped_ptr<ChannelAuthenticator> host_auth_;
  MockChannelDoneCallback client_callback_;
  MockChannelDoneCallback host_callback_;
  scoped_ptr<net::StreamSocket> client_socket_;
  scoped_ptr<net::StreamSocket> host_socket_;

  DISALLOW_COPY_AND_ASSIGN(SslHmacChannelAuthenticatorTest);
};

// Verify that a channel can be connected using a valid shared secret.
TEST_F(SslHmacChannelAuthenticatorTest, SuccessfulAuth) {
  client_auth_ = SslHmacChannelAuthenticator::CreateForClient(
      host_cert_, kTestSharedSecret);
  host_auth_ = SslHmacChannelAuthenticator::CreateForHost(
      host_cert_, private_key_.get(), kTestSharedSecret);

  RunChannelAuth(false);

  ASSERT_TRUE(client_socket_.get() != NULL);
  ASSERT_TRUE(host_socket_.get() != NULL);

  StreamConnectionTester tester(host_socket_.get(), client_socket_.get(),
                                100, 2);

  tester.Start();
  message_loop_.Run();
  tester.CheckResults();
}

// Verify that channels cannot be using invalid shared secret.
TEST_F(SslHmacChannelAuthenticatorTest, InvalidChannelSecret) {
  client_auth_ = SslHmacChannelAuthenticator::CreateForClient(
      host_cert_, kTestSharedSecretBad);
  host_auth_ = SslHmacChannelAuthenticator::CreateForHost(
      host_cert_, private_key_.get(), kTestSharedSecret);

  RunChannelAuth(true);

  ASSERT_TRUE(host_socket_.get() == NULL);
}

}  // namespace protocol
}  // namespace remoting
