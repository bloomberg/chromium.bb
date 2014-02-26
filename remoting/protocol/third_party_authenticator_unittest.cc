// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "net/base/net_errors.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/authenticator_test_base.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/fake_authenticator.h"
#include "remoting/protocol/third_party_authenticator_base.h"
#include "remoting/protocol/third_party_client_authenticator.h"
#include "remoting/protocol/third_party_host_authenticator.h"
#include "remoting/protocol/token_validator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using testing::_;
using testing::DeleteArg;
using testing::SaveArg;

namespace {

const int kMessageSize = 100;
const int kMessages = 1;

const char kTokenUrl[] = "https://example.com/Issue";
const char kTokenScope[] = "host:a@b.com/1 client:a@b.com/2";
const char kToken[] = "abc123456xyz789";
const char kSharedSecret[] = "1234-1234-5678";
const char kSharedSecretBad[] = "0000-0000-0001";

}  // namespace

namespace remoting {
namespace protocol {

class ThirdPartyAuthenticatorTest : public AuthenticatorTestBase {
  class FakeTokenFetcher : public ThirdPartyClientAuthenticator::TokenFetcher {
   public:
    virtual void FetchThirdPartyToken(
        const GURL& token_url,
        const std::string& scope,
        const TokenFetchedCallback& token_fetched_callback) OVERRIDE {
     ASSERT_EQ(token_url.spec(), kTokenUrl);
     ASSERT_EQ(scope, kTokenScope);
     ASSERT_FALSE(token_fetched_callback.is_null());
     on_token_fetched_ = token_fetched_callback;
    }

    void OnTokenFetched(const std::string& token,
                        const std::string& shared_secret) {
      ASSERT_FALSE(on_token_fetched_.is_null());
      TokenFetchedCallback on_token_fetched = on_token_fetched_;
      on_token_fetched_.Reset();
      on_token_fetched.Run(token, shared_secret);
    }

   private:
    TokenFetchedCallback on_token_fetched_;
  };

  class FakeTokenValidator : public TokenValidator {
   public:
    FakeTokenValidator()
     : token_url_(kTokenUrl),
       token_scope_(kTokenScope) {}

    virtual ~FakeTokenValidator() {}

    virtual void ValidateThirdPartyToken(
        const std::string& token,
        const TokenValidatedCallback& token_validated_callback) OVERRIDE {
      ASSERT_FALSE(token_validated_callback.is_null());
      on_token_validated_ = token_validated_callback;
    }

    void OnTokenValidated(const std::string& shared_secret) {
      ASSERT_FALSE(on_token_validated_.is_null());
      TokenValidatedCallback on_token_validated = on_token_validated_;
      on_token_validated_.Reset();
      on_token_validated.Run(shared_secret);
    }

    virtual const GURL& token_url() const OVERRIDE {
      return token_url_;
    }

    virtual const std::string& token_scope() const OVERRIDE {
      return token_scope_;
    }

   private:
    GURL token_url_;
    std::string token_scope_;
    base::Callback<void(const std::string& shared_secret)> on_token_validated_;
  };

 public:
  ThirdPartyAuthenticatorTest() {}
  virtual ~ThirdPartyAuthenticatorTest() {}

 protected:
  void InitAuthenticators() {
    scoped_ptr<TokenValidator> token_validator(new FakeTokenValidator());
    token_validator_ = static_cast<FakeTokenValidator*>(token_validator.get());
    host_.reset(new ThirdPartyHostAuthenticator(
        host_cert_, key_pair_, token_validator.Pass()));
    scoped_ptr<ThirdPartyClientAuthenticator::TokenFetcher>
        token_fetcher(new FakeTokenFetcher());
    token_fetcher_ = static_cast<FakeTokenFetcher*>(token_fetcher.get());
    client_.reset(new ThirdPartyClientAuthenticator(token_fetcher.Pass()));
  }

  FakeTokenFetcher* token_fetcher_;
  FakeTokenValidator* token_validator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ThirdPartyAuthenticatorTest);
};

// These tests use net::SSLServerSocket which is not implemented for OpenSSL.
#if defined(USE_OPENSSL)
#define MAYBE(x) DISABLED_##x
#else
#define MAYBE(x) x
#endif

TEST_F(ThirdPartyAuthenticatorTest, MAYBE(SuccessfulAuth)) {
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators());
  ASSERT_NO_FATAL_FAILURE(RunHostInitiatedAuthExchange());
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, client_->state());
  ASSERT_NO_FATAL_FAILURE(token_fetcher_->OnTokenFetched(
      kToken, kSharedSecret));
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, host_->state());
  ASSERT_NO_FATAL_FAILURE(
      token_validator_->OnTokenValidated(kSharedSecret));

  // Both sides have finished.
  ASSERT_EQ(Authenticator::ACCEPTED, host_->state());
  ASSERT_EQ(Authenticator::ACCEPTED, client_->state());

  // An authenticated channel can be created after the authentication.
  client_auth_ = client_->CreateChannelAuthenticator();
  host_auth_ = host_->CreateChannelAuthenticator();
  RunChannelAuth(false);

  StreamConnectionTester tester(host_socket_.get(), client_socket_.get(),
                                kMessageSize, kMessages);

  tester.Start();
  message_loop_.Run();
  tester.CheckResults();
}

TEST_F(ThirdPartyAuthenticatorTest, MAYBE(ClientNoSecret)) {
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators());
  ASSERT_NO_FATAL_FAILURE(RunHostInitiatedAuthExchange());
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, client_->state());
  ASSERT_NO_FATAL_FAILURE(
      token_fetcher_->OnTokenFetched(kToken, std::string()));

  // The end result is that the client rejected the connection, since it
  // couldn't fetch the secret.
  ASSERT_EQ(Authenticator::REJECTED, client_->state());
}

TEST_F(ThirdPartyAuthenticatorTest, MAYBE(InvalidToken)) {
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators());
  ASSERT_NO_FATAL_FAILURE(RunHostInitiatedAuthExchange());
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, client_->state());
  ASSERT_NO_FATAL_FAILURE(token_fetcher_->OnTokenFetched(
      kToken, kSharedSecret));
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, host_->state());
  ASSERT_NO_FATAL_FAILURE(token_validator_->OnTokenValidated(std::string()));

  // The end result is that the host rejected the token.
  ASSERT_EQ(Authenticator::REJECTED, host_->state());
}

TEST_F(ThirdPartyAuthenticatorTest, MAYBE(CannotFetchToken)) {
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators());
  ASSERT_NO_FATAL_FAILURE(RunHostInitiatedAuthExchange());
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, client_->state());
  ASSERT_NO_FATAL_FAILURE(
      token_fetcher_->OnTokenFetched(std::string(), std::string()));

  // The end result is that the client rejected the connection, since it
  // couldn't fetch the token.
  ASSERT_EQ(Authenticator::REJECTED, client_->state());
}

// Test that negotiation stops when the fake authentication is rejected.
TEST_F(ThirdPartyAuthenticatorTest, MAYBE(HostBadSecret)) {
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators());
  ASSERT_NO_FATAL_FAILURE(RunHostInitiatedAuthExchange());
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, client_->state());
  ASSERT_NO_FATAL_FAILURE(token_fetcher_->OnTokenFetched(
      kToken, kSharedSecret));
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, host_->state());
  ASSERT_NO_FATAL_FAILURE(
      token_validator_->OnTokenValidated(kSharedSecretBad));

  // The end result is that the host rejected the fake authentication.
  ASSERT_EQ(Authenticator::REJECTED, client_->state());
}

TEST_F(ThirdPartyAuthenticatorTest, MAYBE(ClientBadSecret)) {
  ASSERT_NO_FATAL_FAILURE(InitAuthenticators());
  ASSERT_NO_FATAL_FAILURE(RunHostInitiatedAuthExchange());
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, client_->state());
  ASSERT_NO_FATAL_FAILURE(
      token_fetcher_->OnTokenFetched(kToken, kSharedSecretBad));
  ASSERT_EQ(Authenticator::PROCESSING_MESSAGE, host_->state());
  ASSERT_NO_FATAL_FAILURE(
      token_validator_->OnTokenValidated(kSharedSecret));

  // The end result is that the host rejected the fake authentication.
  ASSERT_EQ(Authenticator::REJECTED, client_->state());
}

}  // namespace protocol
}  // namespace remoting
