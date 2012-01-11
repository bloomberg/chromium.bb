// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/v1_authenticator.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "crypto/rsa_private_key.h"
#include "net/base/net_errors.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/fake_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using testing::_;
using testing::DeleteArg;
using testing::Mock;
using testing::SaveArg;

namespace remoting {
namespace protocol {

namespace {

const int kMessageSize = 100;
const int kMessages = 1;

const char kClientJid[] = "host2@gmail.com/321";

const char kTestSharedSecret[] = "1234-1234-5678";
const char kTestSharedSecretBad[] = "0000-0000-0001";

class MockChannelDoneCallback {
 public:
  MOCK_METHOD2(OnDone, void(net::Error error, net::StreamSocket* socket));
};

}  // namespace

class V1AuthenticatorTest : public testing::Test {
 public:
  V1AuthenticatorTest() {
  }
  virtual ~V1AuthenticatorTest() {
  }

 protected:
  virtual void SetUp() OVERRIDE {
    FilePath certs_dir;
    PathService::Get(base::DIR_SOURCE_ROOT, &certs_dir);
    certs_dir = certs_dir.AppendASCII("net");
    certs_dir = certs_dir.AppendASCII("data");
    certs_dir = certs_dir.AppendASCII("ssl");
    certs_dir = certs_dir.AppendASCII("certificates");

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

  void InitAuthenticators(const std::string& client_secret,
                          const std::string& host_secret) {
    host_.reset(new V1HostAuthenticator(
        host_cert_, *private_key_, host_secret, kClientJid));
    client_.reset(new V1ClientAuthenticator(kClientJid, client_secret));
  }

  void RunAuthExchange() {
    do {
      scoped_ptr<buzz::XmlElement> message;

      // Pass message from client to host.
      ASSERT_EQ(Authenticator::MESSAGE_READY, client_->state());
      message.reset(client_->GetNextMessage());
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
      message.reset(host_->GetNextMessage());
      ASSERT_TRUE(message.get());
      ASSERT_NE(Authenticator::MESSAGE_READY, host_->state());

      ASSERT_EQ(Authenticator::WAITING_MESSAGE, client_->state());
      client_->ProcessMessage(message.get());
      ASSERT_NE(Authenticator::WAITING_MESSAGE, client_->state());
    } while (host_->state() != Authenticator::ACCEPTED &&
             host_->state() != Authenticator::REJECTED);
  }

  void RunChannelAuth(bool expected_fail) {
    client_fake_socket_.reset(new FakeSocket());
    host_fake_socket_.reset(new FakeSocket());
    client_fake_socket_->PairWith(host_fake_socket_.get());

    client_auth_->SecureAndAuthenticate(
        client_fake_socket_.release(),
        base::Bind(&MockChannelDoneCallback::OnDone,
                   base::Unretained(&client_callback_)));

    host_auth_->SecureAndAuthenticate(
        host_fake_socket_.release(),
        base::Bind(&MockChannelDoneCallback::OnDone,
                   base::Unretained(&host_callback_)));

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

    Mock::VerifyAndClearExpectations(&client_callback_);
    Mock::VerifyAndClearExpectations(&host_callback_);

    client_socket_.reset(client_socket);
    host_socket_.reset(host_socket);
  }

  MessageLoop message_loop_;

  scoped_ptr<crypto::RSAPrivateKey> private_key_;
  std::string host_cert_;
  scoped_ptr<V1HostAuthenticator> host_;
  scoped_ptr<V1ClientAuthenticator> client_;
  scoped_ptr<FakeSocket> client_fake_socket_;
  scoped_ptr<FakeSocket> host_fake_socket_;
  scoped_ptr<ChannelAuthenticator> client_auth_;
  scoped_ptr<ChannelAuthenticator> host_auth_;
  MockChannelDoneCallback client_callback_;
  MockChannelDoneCallback host_callback_;
  scoped_ptr<net::StreamSocket> client_socket_;
  scoped_ptr<net::StreamSocket> host_socket_;

  DISALLOW_COPY_AND_ASSIGN(V1AuthenticatorTest);
};

TEST_F(V1AuthenticatorTest, SuccessfulAuth) {
  {
    SCOPED_TRACE("RunAuthExchange");
    InitAuthenticators(kTestSharedSecret, kTestSharedSecret);
    RunAuthExchange();
  }
  ASSERT_EQ(Authenticator::ACCEPTED, host_->state());
  ASSERT_EQ(Authenticator::ACCEPTED, client_->state());

  client_auth_.reset(client_->CreateChannelAuthenticator());
  host_auth_.reset(host_->CreateChannelAuthenticator());
  RunChannelAuth(false);

  EXPECT_TRUE(client_socket_.get() != NULL);
  EXPECT_TRUE(host_socket_.get() != NULL);

  StreamConnectionTester tester(host_socket_.get(), client_socket_.get(),
                                kMessageSize, kMessages);

  tester.Start();
  message_loop_.Run();
  tester.CheckResults();
}

// Verify that connection is rejected when secrets don't match.
TEST_F(V1AuthenticatorTest, InvalidSecret) {
  {
    SCOPED_TRACE("RunAuthExchange");
    InitAuthenticators(kTestSharedSecretBad, kTestSharedSecret);
    RunAuthExchange();
  }
  ASSERT_EQ(Authenticator::REJECTED, host_->state());
}

}  // namespace protocol
}  // namespace remoting
