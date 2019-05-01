// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_OAUTH_TOKEN_EXCHANGER_H_
#define REMOTING_BASE_OAUTH_TOKEN_EXCHANGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "remoting/base/oauth_token_getter.h"

namespace remoting {

class OAuthTokenExchanger : public gaia::GaiaOAuthClient::Delegate {
 public:
  typedef base::OnceCallback<void(OAuthTokenGetter::Status status,
                                  const std::string& access_token)>
      TokenCallback;

  explicit OAuthTokenExchanger(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~OAuthTokenExchanger() override;

  void ExchangeToken(const std::string& access_token,
                     TokenCallback on_new_token);

  // gaia::GaiaOAuthClient::Delegate interface.
  void OnGetTokenInfoResponse(
      std::unique_ptr<base::DictionaryValue> token_info) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

 private:
  void NotifyCallbacks(OAuthTokenGetter::Status status,
                       const std::string& access_token);

  std::unique_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;
  base::queue<TokenCallback> pending_callbacks_;
  std::string oauth_access_token_;

  // True if the OAuth refresh token is lacking required scopes and the
  // token-exchange service is needed to provide a new access-token.
  // False if the refresh token is up-to-date with required scopes.
  // Unset if the scopes are unknown and the tokeninfo endpoint needs to be
  // queried.

  // TODO(lambroslambrou): Activate tokeninfo fetch by removing '= false'.
  base::Optional<bool> need_token_exchange_ = false;
  DISALLOW_COPY_AND_ASSIGN(OAuthTokenExchanger);
};

}  // namespace remoting

#endif  // REMOTING_BASE_OAUTH_TOKEN_EXCHANGER_H_
