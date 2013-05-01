// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/token_validator_factory_impl.h"

#include <set>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/string_util.h"
#include "base/values.h"
#include "crypto/random.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "remoting/base/rsa_key_pair.h"

namespace {

// Length in bytes of the cryptographic nonce used to salt the token scope.
const size_t kNonceLength = 16;  // 128 bits.

}

namespace remoting {

class TokenValidatorImpl
    : public net::URLFetcherDelegate,
      public protocol::ThirdPartyHostAuthenticator::TokenValidator {
 public:
  TokenValidatorImpl(
      const GURL& token_url,
      const GURL& token_validation_url,
      scoped_refptr<RsaKeyPair> key_pair,
      const std::string& local_jid,
      const std::string& remote_jid,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter)
      : token_url_(token_url),
        token_validation_url_(token_validation_url),
        key_pair_(key_pair),
        request_context_getter_(request_context_getter) {
    DCHECK(token_url_.is_valid());
    DCHECK(token_validation_url_.is_valid());
    DCHECK(key_pair_);
    token_scope_ = CreateScope(local_jid, remote_jid);
  }

  virtual ~TokenValidatorImpl() {
  }

  // TokenValidator interface.
  virtual void ValidateThirdPartyToken(
      const std::string& token,
      const base::Callback<void(
          const std::string& shared_secret)>& on_token_validated) OVERRIDE {
    DCHECK(!request_);
    DCHECK(!on_token_validated.is_null());

    on_token_validated_ = on_token_validated;

    std::string post_body =
        "code=" + net::EscapeUrlEncodedData(token, true) +
        "&client_id=" + net::EscapeUrlEncodedData(
            key_pair_->GetPublicKey(), true) +
        "&client_secret=" + net::EscapeUrlEncodedData(
            key_pair_->SignMessage(token), true) +
        "&grant_type=authorization_code";
    request_.reset(net::URLFetcher::Create(
        0, token_validation_url_, net::URLFetcher::POST, this));
    request_->SetUploadData("application/x-www-form-urlencoded", post_body);
    request_->SetRequestContext(request_context_getter_);
    request_->Start();
  }

  virtual const GURL& token_url() const OVERRIDE {
    return token_url_;
  }

  virtual const std::string& token_scope() const OVERRIDE {
    return token_scope_;
  }

  // URLFetcherDelegate interface.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
    DCHECK_EQ(request_.get(), source);
    std::string shared_token = ProcessResponse();
    request_.reset();
    on_token_validated_.Run(shared_token);
  }

 private:
  bool IsValidScope(const std::string& token_scope) {
    // TODO(rmsousa): Deal with reordering/subsets/supersets/aliases/etc.
    return token_scope == token_scope_;
  }

  static std::string CreateScope(const std::string& local_jid,
                                 const std::string& remote_jid) {
    std::string nonce_bytes;
    crypto::RandBytes(WriteInto(&nonce_bytes, kNonceLength + 1), kNonceLength);
    std::string nonce;
    bool success = base::Base64Encode(nonce_bytes, &nonce);
    DCHECK(success);
    return "client:" + remote_jid + " host:" + local_jid + " nonce:" + nonce;
  }

  std::string ProcessResponse() {
    // Verify that we got a successful response.
    net::URLRequestStatus status = request_->GetStatus();
    if (!status.is_success()) {
      LOG(ERROR) << "Error validating token, status=" << status.status()
                 << " err=" << status.error();
      return std::string();
    }

    int response = request_->GetResponseCode();
    std::string data;
    request_->GetResponseAsString(&data);
    if (response != 200) {
      LOG(ERROR)
          << "Error " << response << " validating token: '" << data << "'";
      return std::string();
    }

    // Decode the JSON data from the response.
    scoped_ptr<base::Value> value(base::JSONReader::Read(data));
    DictionaryValue* dict;
    if (!value.get() || value->GetType() != base::Value::TYPE_DICTIONARY ||
        !value->GetAsDictionary(&dict)) {
      LOG(ERROR) << "Invalid token validation response: '" << data << "'";
      return std::string();
    }

    std::string token_scope;
    dict->GetStringWithoutPathExpansion("scope", &token_scope);
    if (!IsValidScope(token_scope)) {
      LOG(ERROR) << "Invalid scope: '" << token_scope
          << "', expected: '" << token_scope_ <<"'.";
      return std::string();
    }

    std::string shared_secret;
    // Everything is valid, so return the shared secret to the caller.
    dict->GetStringWithoutPathExpansion("access_token", &shared_secret);
    return shared_secret;
  }

  scoped_ptr<net::URLFetcher> request_;
  GURL token_url_;
  GURL token_validation_url_;
  scoped_refptr<RsaKeyPair> key_pair_;
  std::string token_scope_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  base::Callback<void(const std::string& shared_secret)> on_token_validated_;

  DISALLOW_COPY_AND_ASSIGN(TokenValidatorImpl);
};

TokenValidatorFactoryImpl::TokenValidatorFactoryImpl(
    const GURL& token_url,
    const GURL& token_validation_url,
    scoped_refptr<RsaKeyPair> key_pair,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter)
    : token_url_(token_url),
      token_validation_url_(token_validation_url),
      key_pair_(key_pair),
      request_context_getter_(request_context_getter) {
}

TokenValidatorFactoryImpl::~TokenValidatorFactoryImpl() {
}

scoped_ptr<protocol::ThirdPartyHostAuthenticator::TokenValidator>
TokenValidatorFactoryImpl::CreateTokenValidator(
    const std::string& local_jid,
    const std::string& remote_jid) {
  return scoped_ptr<protocol::ThirdPartyHostAuthenticator::TokenValidator>(
      new TokenValidatorImpl(token_url_, token_validation_url_, key_pair_,
                             local_jid, remote_jid,
                             request_context_getter_));
}

}  // namespace remoting
