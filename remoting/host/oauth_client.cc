// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/oauth_client.h"

#include "base/bind.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/url_request_context.h"

namespace remoting {

OAuthClient::OAuthClient()
    : network_thread_("OAuthNetworkThread"),
      file_thread_("OAuthFileThread"),
      delegate_(NULL) {
  network_thread_.StartWithOptions(
      base::Thread::Options(MessageLoop::TYPE_IO, 0));
  file_thread_.StartWithOptions(
      base::Thread::Options(MessageLoop::TYPE_IO, 0));
  url_request_context_getter_ = new URLRequestContextGetter(
      network_thread_.message_loop(), file_thread_.message_loop());
  gaia_oauth_client_.reset(
      new GaiaOAuthClient(kGaiaOAuth2Url, url_request_context_getter_.get()));
}

OAuthClient::~OAuthClient() {
}

void OAuthClient::Start(const std::string& refresh_token,
                        OAuthClient::Delegate* delegate,
                        base::MessageLoopProxy* message_loop) {
  refresh_token_ = refresh_token;
  delegate_ = delegate;
  message_loop_ = message_loop;
  RefreshToken();
}

void OAuthClient::OnGetTokensResponse(const std::string& refresh_token,
                                      const std::string& access_token,
                                      int expires_in_seconds) {
  NOTREACHED();
}

void OAuthClient::OnRefreshTokenResponse(const std::string& access_token,
                                         int expires_in_seconds) {
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&OAuthClient::Delegate::OnRefreshTokenResponse,
                 base::Unretained(delegate_),
                 access_token, expires_in_seconds));
  // Queue a token exchange for 1 minute before this one expires.
  message_loop_->PostDelayedTask(FROM_HERE,
                                 base::Bind(&OAuthClient::RefreshToken,
                                            base::Unretained(this)),
                                 base::TimeDelta::FromSeconds(
                                     expires_in_seconds - 60));
}

void OAuthClient::OnOAuthError() {
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&OAuthClient::Delegate::OnOAuthError,
                 base::Unretained(delegate_)));
}

void OAuthClient::OnNetworkError(int response_code) {
  // TODO(jamiewalch): Set a sensible retry limit and integrate with
  // SignalingConnector to restart the reconnection process (crbug.com/118928).
  NOTREACHED();
}

void OAuthClient::RefreshToken() {
#ifdef OFFICIAL_BUILD
  OAuthClientInfo client_info = {
    "440925447803-avn2sj1kc099s0r7v62je5s339mu0am1.apps.googleusercontent.com",
    "Bgur6DFiOMM1h8x-AQpuTQlK"
  };
#else
  OAuthClientInfo client_info = {
    "440925447803-2pi3v45bff6tp1rde2f7q6lgbor3o5uj.apps.googleusercontent.com",
    "W2ieEsG-R1gIA4MMurGrgMc_"
  };
#endif
  gaia_oauth_client_->RefreshToken(client_info, refresh_token_, -1, this);
}

}  // namespace remoting
