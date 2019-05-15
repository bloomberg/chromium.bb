// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/oauth_token_exchanger.h"

#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

// Maximum number of retries on network/500 errors.
const int kMaxRetries = 3;

const char API_TACHYON[] = "https://www.googleapis.com/auth/tachyon";
const char API_CHROMOTING_ME2ME_HOST[] =
    "https://www.googleapis.com/auth/chromoting.me2me.host";

const char TOKENINFO_SCOPE_KEY[] = "scope";

// Returns whether the "scopes" part of the OAuth tokeninfo response
// contains all the needed scopes.
bool HasNeededScopes(const std::string& scopes) {
  std::vector<base::StringPiece> scopes_list =
      base::SplitStringPiece(scopes, base::kWhitespaceASCII,
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return base::ContainsValue(scopes_list, API_TACHYON) &&
         base::ContainsValue(scopes_list, API_CHROMOTING_ME2ME_HOST);
}

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
    RequestNewToken();
    return;
  }

  // Return the original token, as it already has required scopes.
  NotifyCallbacks(OAuthTokenGetter::SUCCESS, oauth_access_token_);
}

void OAuthTokenExchanger::OnGetTokenInfoResponse(
    std::unique_ptr<base::DictionaryValue> token_info) {
  base::Value* scopes_value = token_info->FindKey(TOKENINFO_SCOPE_KEY);
  if (!scopes_value || !scopes_value->is_string()) {
    NotifyCallbacks(OAuthTokenGetter::AUTH_ERROR, std::string());
    return;
  }
  std::string scopes = scopes_value->GetString();
  VLOG(1) << "Token scopes: " << scopes;
  need_token_exchange_ = !HasNeededScopes(scopes);
  VLOG(1) << "Need exchange: " << (need_token_exchange_.value() ? "yes" : "no");

  if (need_token_exchange_.value()) {
    RequestNewToken();
  } else {
    NotifyCallbacks(OAuthTokenGetter::SUCCESS, oauth_access_token_);
  }
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

void OAuthTokenExchanger::RequestNewToken() {
  NOTIMPLEMENTED() << "Token exchange not yet implemented.";
  NotifyCallbacks(OAuthTokenGetter::SUCCESS, oauth_access_token_);
}

}  // namespace remoting
