// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/token_validator_factory_impl.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "crypto/random.h"
#include "net/base/escape.h"
#include "net/base/io_buffer.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/host/token_validator_base.h"
#include "url/gurl.h"

namespace {

// Length in bytes of the cryptographic nonce used to salt the token scope.
const size_t kNonceLength = 16;  // 128 bits.

}  // namespace

namespace remoting {


class TokenValidatorImpl : public TokenValidatorBase {
 public:
  TokenValidatorImpl(
      const ThirdPartyAuthConfig& third_party_auth_config,
      scoped_refptr<RsaKeyPair> key_pair,
      const std::string& local_jid,
      const std::string& remote_jid,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter);

 protected:
  virtual void StartValidateRequest(const std::string& token) OVERRIDE;

 private:
  static std::string CreateScope(const std::string& local_jid,
                                 const std::string& remote_jid);

  std::string post_body_;
  scoped_refptr<RsaKeyPair> key_pair_;

  DISALLOW_COPY_AND_ASSIGN(TokenValidatorImpl);
};

TokenValidatorImpl::TokenValidatorImpl(
    const ThirdPartyAuthConfig& third_party_auth_config,
    scoped_refptr<RsaKeyPair> key_pair,
    const std::string& local_jid,
    const std::string& remote_jid,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter)
    : TokenValidatorBase(third_party_auth_config,
                         CreateScope(local_jid, remote_jid),
                         request_context_getter),
      key_pair_(key_pair) {
  DCHECK(key_pair_.get());
  token_scope_ = CreateScope(local_jid, remote_jid);
}

// TokenValidator interface.
void TokenValidatorImpl::StartValidateRequest(const std::string& token) {
  post_body_ = "code=" + net::EscapeUrlEncodedData(token, true) +
      "&client_id=" + net::EscapeUrlEncodedData(
          key_pair_->GetPublicKey(), true) +
      "&client_secret=" + net::EscapeUrlEncodedData(
          key_pair_->SignMessage(token), true) +
      "&grant_type=authorization_code";

  request_ = request_context_getter_->GetURLRequestContext()->CreateRequest(
      third_party_auth_config_.token_validation_url, net::DEFAULT_PRIORITY,
      this, NULL);
  request_->SetExtraRequestHeaderByName(
      net::HttpRequestHeaders::kContentType,
      "application/x-www-form-urlencoded", true);
  request_->set_method("POST");
  scoped_ptr<net::UploadElementReader> reader(
      new net::UploadBytesElementReader(
          post_body_.data(), post_body_.size()));
  request_->set_upload(make_scoped_ptr(
      net::UploadDataStream::CreateWithReader(reader.Pass(), 0)));
  request_->Start();
}

std::string TokenValidatorImpl::CreateScope(
    const std::string& local_jid,
    const std::string& remote_jid) {
  std::string nonce_bytes;
  crypto::RandBytes(WriteInto(&nonce_bytes, kNonceLength + 1), kNonceLength);
  std::string nonce;
  base::Base64Encode(nonce_bytes, &nonce);
  return "client:" + remote_jid + " host:" + local_jid + " nonce:" + nonce;
}

TokenValidatorFactoryImpl::TokenValidatorFactoryImpl(
    const ThirdPartyAuthConfig& third_party_auth_config,
    scoped_refptr<RsaKeyPair> key_pair,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter)
    : third_party_auth_config_(third_party_auth_config),
      key_pair_(key_pair),
      request_context_getter_(request_context_getter) {
}

TokenValidatorFactoryImpl::~TokenValidatorFactoryImpl() {
}

scoped_ptr<protocol::TokenValidator>
TokenValidatorFactoryImpl::CreateTokenValidator(
    const std::string& local_jid,
    const std::string& remote_jid) {
  return scoped_ptr<protocol::TokenValidator>(
      new TokenValidatorImpl(third_party_auth_config_,
                             key_pair_, local_jid, remote_jid,
                             request_context_getter_));
}

}  // namespace remoting
