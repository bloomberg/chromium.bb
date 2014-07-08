// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/oauth_client.h"

#include "base/logging.h"

namespace {
const int kMaxGaiaRetries = 3;
}  // namespace

namespace remoting {

OAuthClient::OAuthClient(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter)
    : gaia_oauth_client_(url_request_context_getter) {
}

OAuthClient::~OAuthClient() {
}

void OAuthClient::GetCredentialsFromAuthCode(
    const gaia::OAuthClientInfo& oauth_client_info,
    const std::string& auth_code,
    CompletionCallback on_done) {

  if (!on_done_.is_null()) {
    pending_requests_.push(Request(oauth_client_info, auth_code, on_done));
    return;
  }

  on_done_ = on_done;
  // Map the authorization code to refresh and access tokens.
  gaia_oauth_client_.GetTokensFromAuthCode(oauth_client_info, auth_code,
                                           kMaxGaiaRetries, this);
}

void OAuthClient::OnGetTokensResponse(
    const std::string& refresh_token,
    const std::string& access_token,
    int expires_in_seconds) {
  refresh_token_ = refresh_token;
  // Get the email corresponding to the access token.
  gaia_oauth_client_.GetUserEmail(access_token, kMaxGaiaRetries, this);
}

void OAuthClient::OnRefreshTokenResponse(
    const std::string& access_token,
    int expires_in_seconds) {
  // We never request a refresh token, so this call is not expected.
  NOTREACHED();
}

void OAuthClient::SendResponse(const std::string& user_email,
                               const std::string& refresh_token) {
  CompletionCallback on_done = on_done_;
  on_done_.Reset();
  on_done.Run(user_email, refresh_token);

  // Process the next request in the queue.
  if (pending_requests_.size()) {
    Request request = pending_requests_.front();
    pending_requests_.pop();
    // GetCredentialsFromAuthCode is asynchronous, so it's safe to call it here.
    GetCredentialsFromAuthCode(
        request.oauth_client_info, request.auth_code, request.on_done);
  }
}

void OAuthClient::OnGetUserEmailResponse(const std::string& user_email) {
  SendResponse(user_email, refresh_token_);
}

void OAuthClient::OnOAuthError() {
  SendResponse("", "");
}

void OAuthClient::OnNetworkError(int response_code) {
  SendResponse("", "");
}

OAuthClient::Request::Request(
    const gaia::OAuthClientInfo& oauth_client_info,
    const std::string& auth_code,
    CompletionCallback on_done) {
  this->oauth_client_info = oauth_client_info;
  this->auth_code = auth_code;
  this->on_done = on_done;
}

OAuthClient::Request::~Request() {
}

}  // namespace remoting
