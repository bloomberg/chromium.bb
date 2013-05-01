// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A set of unit tests for TokenValidatorFactoryImpl

#include <string>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/test_rsa_key_pair.h"
#include "remoting/host/token_validator_factory_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTokenUrl[] = "https://example.com/token";
const char kTokenValidationUrl[] = "https://example.com/validate";
const char kLocalJid[] = "user@example.com/local";
const char kRemoteJid[] = "user@example.com/remote";
const char kToken[] = "xyz123456";
const char kSharedSecret[] = "abcdefgh";

// Bad scope: no nonce element.
const char kBadScope[] =
    "client:user@example.com/local host:user@example.com/remote";

}  // namespace

namespace remoting {

class TokenValidatorFactoryImplTest : public testing::Test {
 public:
  TokenValidatorFactoryImplTest() : message_loop_(base::MessageLoop::TYPE_IO) {}

  void SuccessCallback(const std::string& shared_secret) {
    EXPECT_FALSE(shared_secret.empty());
    message_loop_.Quit();
  }

  void FailureCallback(const std::string& shared_secret) {
    EXPECT_TRUE(shared_secret.empty());
    message_loop_.Quit();
  }

  void DeleteOnFailureCallback(const std::string& shared_secret) {
    EXPECT_TRUE(shared_secret.empty());
    token_validator_.reset();
    message_loop_.Quit();
  }

 protected:
  virtual void SetUp() OVERRIDE {
    key_pair_ = RsaKeyPair::FromString(kTestRsaKeyPair);
    request_context_getter_ = new net::TestURLRequestContextGetter(
        message_loop_.message_loop_proxy());
    token_validator_factory_.reset(new TokenValidatorFactoryImpl(
        GURL(kTokenUrl), GURL(kTokenValidationUrl), key_pair_,
        request_context_getter_));
  }

  static std::string CreateResponse(const std::string& scope) {
    DictionaryValue response_dict;
    response_dict.SetString("access_token", kSharedSecret);
    response_dict.SetString("token_type", "shared_secret");
    response_dict.SetString("scope", scope);
    std::string response;
    base::JSONWriter::Write(&response_dict, &response);
    return response;
  }

  static std::string CreateErrorResponse(const std::string& error) {
    DictionaryValue response_dict;
    response_dict.SetString("error", error);
    std::string response;
    base::JSONWriter::Write(&response_dict, &response);
    return response;
  }

  MessageLoop message_loop_;
  scoped_refptr<RsaKeyPair> key_pair_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_ptr<TokenValidatorFactoryImpl> token_validator_factory_;
  scoped_ptr<protocol::ThirdPartyHostAuthenticator::TokenValidator>
  token_validator_;
};

TEST_F(TokenValidatorFactoryImplTest, Success) {
  net::FakeURLFetcherFactory factory(NULL);
  token_validator_ = token_validator_factory_->CreateTokenValidator(
      kLocalJid, kRemoteJid);
  factory.SetFakeResponse(kTokenValidationUrl, CreateResponse(
      token_validator_->token_scope()), true);
  token_validator_->ValidateThirdPartyToken(
      kToken, base::Bind(&TokenValidatorFactoryImplTest::SuccessCallback,
                             base::Unretained(this)));
  message_loop_.Run();
}

TEST_F(TokenValidatorFactoryImplTest, BadToken) {
  net::FakeURLFetcherFactory factory(NULL);
  token_validator_ = token_validator_factory_->CreateTokenValidator(
      kLocalJid, kRemoteJid);
  factory.SetFakeResponse(kTokenValidationUrl, std::string(), false);
  token_validator_->ValidateThirdPartyToken(
      kToken, base::Bind(&TokenValidatorFactoryImplTest::FailureCallback,
                             base::Unretained(this)));
  message_loop_.Run();
}

TEST_F(TokenValidatorFactoryImplTest, BadScope) {
  net::FakeURLFetcherFactory factory(NULL);
  token_validator_ = token_validator_factory_->CreateTokenValidator(
      kLocalJid, kRemoteJid);
  factory.SetFakeResponse(kTokenValidationUrl, CreateResponse(kBadScope), true);
  token_validator_->ValidateThirdPartyToken(
      kToken, base::Bind(&TokenValidatorFactoryImplTest::FailureCallback,
                         base::Unretained(this)));
  message_loop_.Run();
}

TEST_F(TokenValidatorFactoryImplTest, DeleteOnFailure) {
  net::FakeURLFetcherFactory factory(NULL);
  token_validator_ = token_validator_factory_->CreateTokenValidator(
      kLocalJid, kRemoteJid);
  factory.SetFakeResponse(kTokenValidationUrl, std::string(), false);
  token_validator_->ValidateThirdPartyToken(
      kToken, base::Bind(
          &TokenValidatorFactoryImplTest::DeleteOnFailureCallback,
          base::Unretained(this)));
  message_loop_.Run();
}

}  // namespace remoting
