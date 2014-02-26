// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A set of unit tests for TokenValidatorFactoryImpl

#include <string>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/test_rsa_key_pair.h"
#include "remoting/host/token_validator_factory_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kTokenUrl[] = "https://example.com/token";
const char kTokenValidationUrl[] = "https://example.com/validate";
const char kTokenValidationCertIssuer[] = "";
const char kLocalJid[] = "user@example.com/local";
const char kRemoteJid[] = "user@example.com/remote";
const char kToken[] = "xyz123456";
const char kSharedSecret[] = "abcdefgh";

// Bad scope: no nonce element.
const char kBadScope[] =
    "client:user@example.com/local host:user@example.com/remote";

class FakeProtocolHandler : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  FakeProtocolHandler(const std::string& headers, const std::string& response)
      : headers_(headers),
        response_(response) {
  }
  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request, net::NetworkDelegate* network_delegate) const
      OVERRIDE {
    return new net::URLRequestTestJob(
        request, network_delegate, headers_, response_, true);
  }

 private:
  std::string headers_;
  std::string response_;
};

class SetResponseURLRequestContext: public net::TestURLRequestContext {
 public:
  void SetResponse(const std::string& headers, const std::string& response) {
    net::URLRequestJobFactoryImpl* factory =
        new net::URLRequestJobFactoryImpl();
    factory->SetProtocolHandler(
        "https", new FakeProtocolHandler(headers, response));
    context_storage_.set_job_factory(factory);
  }
};

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
    scoped_ptr<net::TestURLRequestContext> context(
        new SetResponseURLRequestContext());
    request_context_getter_ = new net::TestURLRequestContextGetter(
        message_loop_.message_loop_proxy(), context.Pass());
    ThirdPartyAuthConfig config;
    config.token_url = GURL(kTokenUrl);
    config.token_validation_url = GURL(kTokenValidationUrl);
    config.token_validation_cert_issuer = kTokenValidationCertIssuer;
    token_validator_factory_.reset(new TokenValidatorFactoryImpl(
        config, key_pair_, request_context_getter_));
  }

  static std::string CreateResponse(const std::string& scope) {
    base::DictionaryValue response_dict;
    response_dict.SetString("access_token", kSharedSecret);
    response_dict.SetString("token_type", "shared_secret");
    response_dict.SetString("scope", scope);
    std::string response;
    base::JSONWriter::Write(&response_dict, &response);
    return response;
  }

  static std::string CreateErrorResponse(const std::string& error) {
    base::DictionaryValue response_dict;
    response_dict.SetString("error", error);
    std::string response;
    base::JSONWriter::Write(&response_dict, &response);
    return response;
  }


  void SetResponse(const std::string& headers, const std::string& response) {
    SetResponseURLRequestContext* context =
        static_cast<SetResponseURLRequestContext*>(
            request_context_getter_->GetURLRequestContext());
    context->SetResponse(headers, response);
  }

  base::MessageLoop message_loop_;
  scoped_refptr<RsaKeyPair> key_pair_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_ptr<TokenValidatorFactoryImpl> token_validator_factory_;
  scoped_ptr<protocol::TokenValidator> token_validator_;
};

TEST_F(TokenValidatorFactoryImplTest, Success) {
  token_validator_ = token_validator_factory_->CreateTokenValidator(
      kLocalJid, kRemoteJid);

  SetResponse(net::URLRequestTestJob::test_headers(),
              CreateResponse(token_validator_->token_scope()));

  token_validator_->ValidateThirdPartyToken(
      kToken, base::Bind(&TokenValidatorFactoryImplTest::SuccessCallback,
                             base::Unretained(this)));
  message_loop_.Run();
}

TEST_F(TokenValidatorFactoryImplTest, BadToken) {
  token_validator_ = token_validator_factory_->CreateTokenValidator(
      kLocalJid, kRemoteJid);

  SetResponse(net::URLRequestTestJob::test_error_headers(), std::string());

  token_validator_->ValidateThirdPartyToken(
      kToken, base::Bind(&TokenValidatorFactoryImplTest::FailureCallback,
                             base::Unretained(this)));
  message_loop_.Run();
}

TEST_F(TokenValidatorFactoryImplTest, BadScope) {
  token_validator_ = token_validator_factory_->CreateTokenValidator(
      kLocalJid, kRemoteJid);

  SetResponse(net::URLRequestTestJob::test_headers(),
              CreateResponse(kBadScope));

  token_validator_->ValidateThirdPartyToken(
      kToken, base::Bind(&TokenValidatorFactoryImplTest::FailureCallback,
                         base::Unretained(this)));
  message_loop_.Run();
}

TEST_F(TokenValidatorFactoryImplTest, DeleteOnFailure) {
  token_validator_ = token_validator_factory_->CreateTokenValidator(
      kLocalJid, kRemoteJid);

  SetResponse(net::URLRequestTestJob::test_error_headers(), std::string());

  token_validator_->ValidateThirdPartyToken(
      kToken, base::Bind(
          &TokenValidatorFactoryImplTest::DeleteOnFailureCallback,
          base::Unretained(this)));
  message_loop_.Run();
}

}  // namespace remoting
