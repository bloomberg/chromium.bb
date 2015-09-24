// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_OAUTH_TOKEN_GETTER_IMPL_H_
#define REMOTING_HOST_OAUTH_TOKEN_GETTER_IMPL_H_

#include <queue>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "remoting/host/oauth_token_getter.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace remoting {

// OAuthTokenGetter caches OAuth access tokens and refreshes them as needed.
class OAuthTokenGetterImpl : public OAuthTokenGetter,
                             public base::NonThreadSafe,
                             public gaia::GaiaOAuthClient::Delegate {
 public:
  OAuthTokenGetterImpl(scoped_ptr<OAuthCredentials> oauth_credentials,
                       const scoped_refptr<net::URLRequestContextGetter>&
                           url_request_context_getter,
                       bool auto_refresh);
  ~OAuthTokenGetterImpl() override;

  // OAuthTokenGetter interface.
  void CallWithToken(
      const OAuthTokenGetter::TokenCallback& on_access_token) override;

 private:
  // gaia::GaiaOAuthClient::Delegate interface.
  void OnGetTokensResponse(const std::string& user_email,
                           const std::string& access_token,
                           int expires_seconds) override;
  void OnRefreshTokenResponse(const std::string& access_token,
                              int expires_in_seconds) override;
  void OnGetUserEmailResponse(const std::string& user_email) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

  void NotifyCallbacks(Status status,
                       const std::string& user_email,
                       const std::string& access_token);
  void RefreshOAuthToken();

  scoped_ptr<OAuthCredentials> oauth_credentials_;
  scoped_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  bool refreshing_oauth_token_ = false;
  bool email_verified_ = false;
  std::string oauth_access_token_;
  base::Time auth_token_expiry_time_;
  std::queue<OAuthTokenGetter::TokenCallback> pending_callbacks_;
  scoped_ptr<base::OneShotTimer> refresh_timer_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_OAUTH_TOKEN_GETTER_IMPL_H_
