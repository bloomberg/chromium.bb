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
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "crypto/random.h"
#include "net/base/escape.h"
#include "net/base/io_buffer.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/ssl/client_cert_store.h"
#if defined(USE_NSS)
#include "net/ssl/client_cert_store_nss.h"
#elif defined(OS_WIN)
#include "net/ssl/client_cert_store_win.h"
#elif defined(OS_MACOSX)
#include "net/ssl/client_cert_store_mac.h"
#endif
#include "net/ssl/ssl_cert_request_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"
#include "remoting/base/rsa_key_pair.h"
#include "url/gurl.h"

namespace {

// Length in bytes of the cryptographic nonce used to salt the token scope.
const size_t kNonceLength = 16;  // 128 bits.
const int kBufferSize = 4096;
const char kCertIssuerWildCard[] = "*";

}  // namespace

namespace remoting {

class TokenValidatorImpl
    : public net::URLRequest::Delegate,
      public protocol::ThirdPartyHostAuthenticator::TokenValidator {
 public:
  TokenValidatorImpl(
      const ThirdPartyAuthConfig& third_party_auth_config,
      scoped_refptr<RsaKeyPair> key_pair,
      const std::string& local_jid,
      const std::string& remote_jid,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter);
  virtual ~TokenValidatorImpl();

  // TokenValidator interface.
  virtual const GURL& token_url() const OVERRIDE;
  virtual const std::string& token_scope() const OVERRIDE;
  virtual void ValidateThirdPartyToken(
      const std::string& token,
      const base::Callback<void(
          const std::string& shared_secret)>& on_token_validated) OVERRIDE;

  // URLFetcherDelegate interface.
  virtual void OnResponseStarted(net::URLRequest* source) OVERRIDE;
  virtual void OnReadCompleted(net::URLRequest* source,
                               int bytes_read) OVERRIDE;
  virtual void OnCertificateRequested(
      net::URLRequest* source,
      net::SSLCertRequestInfo* cert_request_info) OVERRIDE;

 private:
  static std::string CreateScope(const std::string& local_jid,
                                 const std::string& remote_jid);

  void OnCertificatesSelected(net::CertificateList* selected_certs,
                              net::ClientCertStore* unused);
  bool IsValidScope(const std::string& token_scope);
  std::string ProcessResponse();

  std::string post_body_;
  scoped_ptr<net::URLRequest> request_;
  scoped_refptr<net::IOBuffer> buffer_;
  std::string data_;
  ThirdPartyAuthConfig third_party_auth_config_;
  scoped_refptr<RsaKeyPair> key_pair_;
  std::string token_scope_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  base::Callback<void(const std::string& shared_secret)> on_token_validated_;

  base::WeakPtrFactory<TokenValidatorImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TokenValidatorImpl);
};

TokenValidatorImpl::TokenValidatorImpl(
    const ThirdPartyAuthConfig& third_party_auth_config,
    scoped_refptr<RsaKeyPair> key_pair,
    const std::string& local_jid,
    const std::string& remote_jid,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter)
    : buffer_(new net::IOBuffer(kBufferSize)),
      third_party_auth_config_(third_party_auth_config),
      key_pair_(key_pair),
      request_context_getter_(request_context_getter),
      weak_factory_(this) {
  DCHECK(third_party_auth_config_.token_url.is_valid());
  DCHECK(third_party_auth_config_.token_validation_url.is_valid());
  DCHECK(key_pair_.get());
  token_scope_ = CreateScope(local_jid, remote_jid);
}

TokenValidatorImpl::~TokenValidatorImpl() {
}

// TokenValidator interface.
void TokenValidatorImpl::ValidateThirdPartyToken(
    const std::string& token,
    const base::Callback<void(
        const std::string& shared_secret)>& on_token_validated) {
  DCHECK(!request_);
  DCHECK(!on_token_validated.is_null());

  on_token_validated_ = on_token_validated;

  post_body_ = "code=" + net::EscapeUrlEncodedData(token, true) +
      "&client_id=" + net::EscapeUrlEncodedData(
          key_pair_->GetPublicKey(), true) +
      "&client_secret=" + net::EscapeUrlEncodedData(
          key_pair_->SignMessage(token), true) +
      "&grant_type=authorization_code";

  request_ = request_context_getter_->GetURLRequestContext()->CreateRequest(
      third_party_auth_config_.token_validation_url, net::DEFAULT_PRIORITY,
      this);
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

const GURL& TokenValidatorImpl::token_url() const {
  return third_party_auth_config_.token_url;
}

const std::string& TokenValidatorImpl::token_scope() const {
  return token_scope_;
}

// URLFetcherDelegate interface.
void TokenValidatorImpl::OnResponseStarted(net::URLRequest* source) {
  DCHECK_EQ(request_.get(), source);

  int bytes_read = 0;
  request_->Read(buffer_.get(), kBufferSize, &bytes_read);
  OnReadCompleted(request_.get(), bytes_read);
}

void TokenValidatorImpl::OnReadCompleted(net::URLRequest* source,
                                         int bytes_read) {
  DCHECK_EQ(request_.get(), source);

  do {
    if (!request_->status().is_success() || bytes_read <= 0)
      break;

    data_.append(buffer_->data(), bytes_read);
  } while (request_->Read(buffer_.get(), kBufferSize, &bytes_read));

  const net::URLRequestStatus status = request_->status();

  if (!status.is_io_pending()) {
    std::string shared_token = ProcessResponse();
    request_.reset();
    on_token_validated_.Run(shared_token);
  }
}

void TokenValidatorImpl::OnCertificateRequested(
    net::URLRequest* source,
    net::SSLCertRequestInfo* cert_request_info) {
  DCHECK_EQ(request_.get(), source);

  net::ClientCertStore* client_cert_store;
#if defined(USE_NSS)
  client_cert_store = new net::ClientCertStoreNSS(
      net::ClientCertStoreNSS::PasswordDelegateFactory());
#elif defined(OS_WIN)
  client_cert_store = new net::ClientCertStoreWin();
#elif defined(OS_MACOSX)
  client_cert_store = new net::ClientCertStoreMac();
#else
#error Unknown platform.
#endif
  // The callback is uncancellable, and GetClientCert requires selected_certs
  // and client_cert_store to stay alive until the callback is called. So we
  // must give it a WeakPtr for |this|, and ownership of the other parameters.
  net::CertificateList* selected_certs(new net::CertificateList());
  client_cert_store->GetClientCerts(
      *cert_request_info, selected_certs,
      base::Bind(&TokenValidatorImpl::OnCertificatesSelected,
                 weak_factory_.GetWeakPtr(), base::Owned(selected_certs),
                 base::Owned(client_cert_store)));
}

void TokenValidatorImpl::OnCertificatesSelected(
    net::CertificateList* selected_certs,
    net::ClientCertStore* unused) {
  const std::string& issuer =
      third_party_auth_config_.token_validation_cert_issuer;
  if (request_) {
    for (size_t i = 0; i < selected_certs->size(); ++i) {
      if (issuer == kCertIssuerWildCard ||
          issuer == (*selected_certs)[i]->issuer().common_name) {
        request_->ContinueWithCertificate((*selected_certs)[i]);
        return;
      }
    }
    request_->ContinueWithCertificate(NULL);
  }
}

bool TokenValidatorImpl::IsValidScope(const std::string& token_scope) {
  // TODO(rmsousa): Deal with reordering/subsets/supersets/aliases/etc.
  return token_scope == token_scope_;
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

std::string TokenValidatorImpl::ProcessResponse() {
  // Verify that we got a successful response.
  net::URLRequestStatus status = request_->status();
  if (!status.is_success()) {
    LOG(ERROR) << "Error validating token, status=" << status.status()
               << " err=" << status.error();
    return std::string();
  }

  int response = request_->GetResponseCode();
  if (response != 200) {
    LOG(ERROR)
        << "Error " << response << " validating token: '" << data_ << "'";
    return std::string();
  }

  // Decode the JSON data from the response.
  scoped_ptr<base::Value> value(base::JSONReader::Read(data_));
  base::DictionaryValue* dict;
  if (!value.get() || value->GetType() != base::Value::TYPE_DICTIONARY ||
      !value->GetAsDictionary(&dict)) {
    LOG(ERROR) << "Invalid token validation response: '" << data_ << "'";
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

scoped_ptr<protocol::ThirdPartyHostAuthenticator::TokenValidator>
TokenValidatorFactoryImpl::CreateTokenValidator(
    const std::string& local_jid,
    const std::string& remote_jid) {
  return scoped_ptr<protocol::ThirdPartyHostAuthenticator::TokenValidator>(
      new TokenValidatorImpl(third_party_auth_config_,
                             key_pair_, local_jid, remote_jid,
                             request_context_getter_));
}

}  // namespace remoting
