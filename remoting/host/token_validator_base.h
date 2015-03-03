// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_TOKEN_VALIDATOR_BASE_H_
#define REMOTING_HOST_TOKEN_VALIDATOR_BASE_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/host/third_party_auth_config.h"
#include "remoting/protocol/token_validator.h"
#include "url/gurl.h"

namespace net {
class ClientCertStore;
typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;
}

namespace remoting {

class TokenValidatorBase
    : public net::URLRequest::Delegate,
      public protocol::TokenValidator {
 public:
  TokenValidatorBase(
      const ThirdPartyAuthConfig& third_party_auth_config,
      const std::string& token_scope,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter);
  ~TokenValidatorBase() override;

  // TokenValidator interface.
  void ValidateThirdPartyToken(
      const std::string& token,
      const base::Callback<void(const std::string& shared_secret)>&
          on_token_validated) override;

  const GURL& token_url() const override;
  const std::string& token_scope() const override;

  // URLRequest::Delegate interface.
  void OnResponseStarted(net::URLRequest* source) override;
  void OnReadCompleted(net::URLRequest* source, int bytes_read) override;
  void OnCertificateRequested(
      net::URLRequest* source,
      net::SSLCertRequestInfo* cert_request_info) override;

 protected:
  void OnCertificatesSelected(net::CertificateList* selected_certs,
                              net::ClientCertStore* unused);

  virtual void StartValidateRequest(const std::string& token) = 0;
  virtual bool IsValidScope(const std::string& token_scope);
  std::string ProcessResponse();

  // Constructor parameters.
  ThirdPartyAuthConfig third_party_auth_config_;
  std::string token_scope_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // URLRequest related fields.
  scoped_ptr<net::URLRequest> request_;
  scoped_refptr<net::IOBuffer> buffer_;
  std::string data_;

  base::Callback<void(const std::string& shared_secret)> on_token_validated_;

  base::WeakPtrFactory<TokenValidatorBase> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TokenValidatorBase);
};

}  // namespace remoting

#endif  // REMOTING_HOST_TOKEN_VALIDATOR_BASE_H
