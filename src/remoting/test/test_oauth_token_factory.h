// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_TEST_OAUTH_TOKEN_FACTORY_H_
#define REMOTING_TEST_TEST_OAUTH_TOKEN_FACTORY_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "remoting/base/oauth_token_getter.h"

namespace network {
class TransitionalURLLoaderFactoryOwner;
}  // namespace network

namespace remoting {

// The factory object must outlive all OAuthTokenGetters it creates.
class TestOAuthTokenGetterFactory {
 public:
  TestOAuthTokenGetterFactory();
  ~TestOAuthTokenGetterFactory();

  static std::string GetAuthorizationCodeUri();

  std::unique_ptr<OAuthTokenGetter> CreateFromIntermediateCredentials(
      const std::string& auth_code,
      const OAuthTokenGetter::CredentialsUpdatedCallback&
          on_credentials_update);

 private:
  std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner_;
  DISALLOW_COPY_AND_ASSIGN(TestOAuthTokenGetterFactory);
};

}  // namespace remoting

#endif  // REMOTING_TEST_TEST_OAUTH_TOKEN_FACTORY_H_
