// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/oauth_token_exchanger.h"

#include <utility>

#include "base/logging.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

// Maximum number of retries on network/500 errors.
const int kMaxRetries = 3;

}  // namespace

OAuthTokenExchanger::OAuthTokenExchanger(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : gaia_oauth_client_(std::make_unique<gaia::GaiaOAuthClient>(
          std::move(url_loader_factory))) {}

OAuthTokenExchanger::~OAuthTokenExchanger() = default;

void OAuthTokenExchanger::ExchangeToken(const std::string& access_token,
                                        TokenCallback on_new_token) {
  oauth_access_token_ = access_token;
  pending_callbacks_.push(std::move(on_new_token));

  if (!need_token_exchange_.has_value()) {
    gaia_oauth_client_->GetTokenInfo(access_token, kMaxRetries, this);
    return;
  }

  if (need_token_exchange_.value()) {
    // TODO(lambroslambrou): Implement token-exchange.
    NOTIMPLEMENTED();
    return;
  }

  // Return the original token, as it already has required scopes.
  NotifyCallbacks(OAuthTokenGetter::SUCCESS, oauth_access_token_);
}

void OAuthTokenExchanger::OnGetTokenInfoResponse(
    std::unique_ptr<base::DictionaryValue> token_info) {
  VLOG(1) << "GetTokenInfoResponse: " << *token_info;
  base::Value* scopes_value = token_info->FindKey("scope");
  if (!scopes_value || !scopes_value->is_string()) {
    NotifyCallbacks(OAuthTokenGetter::AUTH_ERROR, std::string());
    return;
  }
  std::string scopes = scopes_value->GetString();

  // TODO(lambroslambrou): Check the scopes (one time only, cache the result),
  // and either return the current token or try to exchange it.
  VLOG(1) << "Token scopes: " << scopes;
  NotifyCallbacks(OAuthTokenGetter::SUCCESS, oauth_access_token_);
}

void OAuthTokenExchanger::OnOAuthError() {
  LOG(ERROR) << "OAuth error.";
  NotifyCallbacks(OAuthTokenGetter::AUTH_ERROR, std::string());
}

void OAuthTokenExchanger::OnNetworkError(int response_code) {
  LOG(ERROR) << "Network error: " << response_code;
  NotifyCallbacks(OAuthTokenGetter::NETWORK_ERROR, std::string());
}

void OAuthTokenExchanger::NotifyCallbacks(OAuthTokenGetter::Status status,
                                          const std::string& access_token) {
  // Protect against recursion by moving the callbacks into a temporary list.
  base::queue<TokenCallback> callbacks;
  callbacks.swap(pending_callbacks_);
  while (!callbacks.empty()) {
    std::move(callbacks.front()).Run(status, access_token);
    callbacks.pop();
  }
}

}  // namespace remoting
