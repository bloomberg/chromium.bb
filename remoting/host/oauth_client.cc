// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/oauth_client.h"

#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/url_request_context.h"

namespace remoting {

OAuthClient::OAuthClient()
    : network_thread_("OAuthNetworkThread"),
      file_thread_("OAuthFileThread") {
  network_thread_.StartWithOptions(
      base::Thread::Options(MessageLoop::TYPE_IO, 0));
  file_thread_.StartWithOptions(
      base::Thread::Options(MessageLoop::TYPE_IO, 0));
  url_request_context_getter_.reset(new URLRequestContextGetter(
      network_thread_.message_loop(), file_thread_.message_loop()));
  gaia_oauth_client_.reset(
      new GaiaOAuthClient(kGaiaOAuth2Url, url_request_context_getter_.get()));
}

OAuthClient::~OAuthClient() {
}

void OAuthClient::GetAccessToken(const std::string& refresh_token,
                                 GaiaOAuthClient::Delegate* delegate) {
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
  gaia_oauth_client_->RefreshToken(client_info, refresh_token, -1, delegate);
}

}  // namespace remoting
