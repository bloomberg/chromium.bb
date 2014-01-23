// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_TOKEN_VALIDATOR_FACTORY_IMPL_H_
#define REMOTING_HOST_TOKEN_VALIDATOR_FACTORY_IMPL_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/protocol/third_party_host_authenticator.h"

namespace remoting {

struct ThirdPartyAuthConfig {
  inline bool is_empty() const {
    return token_url.is_empty() && token_validation_url.is_empty();
  }

  inline bool is_valid() const {
    return token_url.is_valid() && token_validation_url.is_valid();
  }

  GURL token_url;
  GURL token_validation_url;
  std::string token_validation_cert_issuer;
};

// This class dispenses |TokenValidator| implementations that use a UrlFetcher
// to contact a |token_validation_url| and exchange the |token| for a
// |shared_secret|.
class TokenValidatorFactoryImpl
    : public protocol::ThirdPartyHostAuthenticator::TokenValidatorFactory {
 public:
  // Creates a new factory. |token_url| and |token_validation_url| are the
  // third party authentication service URLs, obtained via policy. |key_pair_|
  // is used by the host to authenticate with the service by signing the token.
  TokenValidatorFactoryImpl(
      const ThirdPartyAuthConfig& third_party_auth_config,
      scoped_refptr<RsaKeyPair> key_pair,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter);

  virtual ~TokenValidatorFactoryImpl();

  // TokenValidatorFactory interface.
  virtual scoped_ptr<protocol::ThirdPartyHostAuthenticator::TokenValidator>
      CreateTokenValidator(const std::string& local_jid,
                           const std::string& remote_jid) OVERRIDE;

 private:
  ThirdPartyAuthConfig third_party_auth_config_;
  scoped_refptr<RsaKeyPair> key_pair_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(TokenValidatorFactoryImpl);
};

}  // namespace remoting

#endif  // REMOTING_HOST_URL_FETCHER_TOKEN_VALIDATOR_FACTORY_H_
