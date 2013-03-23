// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/negotiating_authenticator.h"

#include "base/bind.h"
#include "net/base/net_errors.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/authenticator_test_base.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/connection_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using testing::_;
using testing::DeleteArg;
using testing::SaveArg;

namespace remoting {
namespace protocol {

namespace {

const int kMessageSize = 100;
const int kMessages = 1;

const char kTestHostId[] = "12345678910123456";

const char kTestSharedSecret[] = "1234-1234-5678";
const char kTestSharedSecretBad[] = "0000-0000-0001";

}  // namespace

class NegotiatingAuthenticatorTest : public AuthenticatorTestBase {
 public:
  NegotiatingAuthenticatorTest() {
  }
  virtual ~NegotiatingAuthenticatorTest() {
  }

 protected:
  void InitAuthenticators(
      const std::string& client_secret,
      const std::string& host_secret,
      AuthenticationMethod::HashFunction hash_function,
      bool client_hmac_only) {
    std::string host_secret_hash = AuthenticationMethod::ApplyHashFunction(
        hash_function, kTestHostId, host_secret);
    host_ = NegotiatingAuthenticator::CreateForHost(
        host_cert_, key_pair_, host_secret_hash, hash_function);

    std::vector<AuthenticationMethod> methods;
    methods.push_back(AuthenticationMethod::Spake2(
        AuthenticationMethod::HMAC_SHA256));
    if (!client_hmac_only) {
      methods.push_back(AuthenticationMethod::Spake2(
          AuthenticationMethod::NONE));
    }
    client_ = NegotiatingAuthenticator::CreateForClient(
        kTestHostId, base::Bind(&NegotiatingAuthenticatorTest::FetchSecret,
                                client_secret), methods);
  }

  static void FetchSecret(
      const std::string& client_secret,
      const protocol::SecretFetchedCallback& secret_fetched_callback) {
    secret_fetched_callback.Run(client_secret);
  }
  void VerifyRejected(Authenticator::RejectionReason reason) {
    ASSERT_TRUE((client_->state() == Authenticator::REJECTED &&
                 (client_->rejection_reason() == reason)) ||
                (host_->state() == Authenticator::REJECTED &&
                 (host_->rejection_reason() == reason)));
  }

  void VerifyAccepted() {
    ASSERT_NO_FATAL_FAILURE(RunAuthExchange());

    ASSERT_EQ(Authenticator::ACCEPTED, host_->state());
    ASSERT_EQ(Authenticator::ACCEPTED, client_->state());

    client_auth_ = client_->CreateChannelAuthenticator();
    host_auth_ = host_->CreateChannelAuthenticator();
    RunChannelAuth(false);

    EXPECT_TRUE(client_socket_.get() != NULL);
    EXPECT_TRUE(host_socket_.get() != NULL);

    StreamConnectionTester tester(host_socket_.get(), client_socket_.get(),
                                  kMessageSize, kMessages);

    tester.Start();
    message_loop_.Run();
    tester.CheckResults();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NegotiatingAuthenticatorTest);
};

TEST_F(NegotiatingAuthenticatorTest, SuccessfulAuthHmac) {
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators(
      kTestSharedSecret, kTestSharedSecret,
      AuthenticationMethod::HMAC_SHA256, false));
  VerifyAccepted();
}

TEST_F(NegotiatingAuthenticatorTest, SuccessfulAuthPlain) {
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators(
      kTestSharedSecret, kTestSharedSecret,
      AuthenticationMethod::NONE, false));
  VerifyAccepted();
}

TEST_F(NegotiatingAuthenticatorTest, InvalidSecretHmac) {
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators(
      kTestSharedSecret, kTestSharedSecretBad,
      AuthenticationMethod::HMAC_SHA256, false));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());

  VerifyRejected(Authenticator::INVALID_CREDENTIALS);
}

TEST_F(NegotiatingAuthenticatorTest, InvalidSecretPlain) {
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators(
      kTestSharedSecret, kTestSharedSecretBad,
      AuthenticationMethod::NONE, false));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());

  VerifyRejected(Authenticator::INVALID_CREDENTIALS);
}

TEST_F(NegotiatingAuthenticatorTest, IncompatibleMethods) {
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators(
      kTestSharedSecret, kTestSharedSecretBad,
      AuthenticationMethod::NONE, true));
  ASSERT_NO_FATAL_FAILURE(RunAuthExchange());

  VerifyRejected(Authenticator::PROTOCOL_ERROR);
}

}  // namespace protocol
}  // namespace remoting
