// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_OAUTH_CLIENT
#define REMOTING_HOST_SETUP_OAUTH_CLIENT

#include <queue>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "net/url_request/url_request_context_getter.h"

namespace net {
class URLRequestContext;
}

namespace remoting {

// A wrapper around GaiaOAuthClient that provides a more convenient interface,
// with queueing of requests and a callback rather than a delegate.
class OAuthClient : public gaia::GaiaOAuthClient::Delegate {
 public:
  // Called when GetCredentialsFromAuthCode is completed, with the |user_email|
  // and |refresh_token| that correspond to the given |auth_code|, or with empty
  // strings on error.
  typedef base::Callback<void(
      const std::string& user_email,
      const std::string& refresh_token)> CompletionCallback;

  OAuthClient(
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);

  virtual ~OAuthClient();

  // Redeems |auth_code| using |oauth_client_info| to obtain |refresh_token| and
  // |access_token|, then uses the userinfo endpoint to obtain |user_email|.
  // Calls CompletionCallback with |user_email| and |refresh_token| when done,
  // or with empty strings on error.
  // If a request is received while another one is  being processed, it is
  // enqueued and processed after the first one is finished.
  void GetCredentialsFromAuthCode(
      const gaia::OAuthClientInfo& oauth_client_info,
      const std::string& auth_code,
      CompletionCallback on_done);

  // gaia::GaiaOAuthClient::Delegate
  virtual void OnGetTokensResponse(const std::string& refresh_token,
                                 const std::string& access_token,
                                 int expires_in_seconds) OVERRIDE;
  virtual void OnRefreshTokenResponse(const std::string& access_token,
                                      int expires_in_seconds) OVERRIDE;
  virtual void OnGetUserEmailResponse(const std::string& user_email) OVERRIDE;

  virtual void OnOAuthError() OVERRIDE;
  virtual void OnNetworkError(int response_code) OVERRIDE;

 private:
  struct Request {
    Request(const gaia::OAuthClientInfo& oauth_client_info,
            const std::string& auth_code,
            CompletionCallback on_done);
    virtual ~Request();
    gaia::OAuthClientInfo oauth_client_info;
    std::string auth_code;
    CompletionCallback on_done;
  };

  void SendResponse(const std::string& user_email,
                    const std::string& refresh_token);

  std::queue<Request> pending_requests_;
  gaia::GaiaOAuthClient gaia_oauth_client_;
  std::string refresh_token_;
  CompletionCallback on_done_;

  DISALLOW_COPY_AND_ASSIGN(OAuthClient);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_OAUTH_CLIENT
