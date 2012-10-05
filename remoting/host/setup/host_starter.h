// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_STARTER
#define REMOTING_HOST_HOST_STARTER

#include <string>

#include "base/callback.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "remoting/host/gaia_user_email_fetcher.h"
#include "remoting/host/host_key_pair.h"
#include "remoting/host/service_client.h"
#include "remoting/host/setup/daemon_controller.h"
#include "remoting/host/url_request_context.h"

namespace remoting {

// A helper class that registers and starts a host.
class HostStarter :
    public gaia::GaiaOAuthClient::Delegate,
    public remoting::GaiaUserEmailFetcher::Delegate,
    public remoting::ServiceClient::Delegate {
 public:
  enum Result{
    START_IN_PROGRESS,
    START_COMPLETE,
    NETWORK_ERROR,
    OAUTH_ERROR,
    START_ERROR,
  };

  typedef base::Callback<void(Result)> CompletionCallback;

  virtual ~HostStarter();

  // Creates a HostStarter.
  static scoped_ptr<HostStarter> Create(
      net::URLRequestContextGetter* url_request_context_getter);

  // Registers a new host with the Chromoting service, and starts it.
  // |auth_code| must be a valid OAuth2 authorization code, typically acquired
  // from a browser. This method uses that code to get an OAuth2 refresh token.
  void StartHost(const std::string& host_name, const std::string& host_pin,
                 bool consent_to_data_collection, const std::string& auth_code,
                 CompletionCallback on_done);

  // gaia::GaiaOAuthClient::Delegate
  virtual void OnGetTokensResponse(const std::string& refresh_token,
                                   const std::string& access_token,
                                   int expires_in_seconds) OVERRIDE;
  virtual void OnRefreshTokenResponse(const std::string& access_token,
                                      int expires_in_seconds) OVERRIDE;
  virtual void OnOAuthError() OVERRIDE;
  virtual void OnNetworkError(int response_code) OVERRIDE;

  // remoting::GaiaUserEmailFetcher::Delegate
  virtual void OnGetUserEmailResponse(const std::string& user_email) OVERRIDE;

  // remoting::ServiceClient::Delegate
  virtual void OnHostRegistered() OVERRIDE;

 private:
  HostStarter(scoped_ptr<gaia::GaiaOAuthClient> oauth_client,
              scoped_ptr<remoting::GaiaUserEmailFetcher> user_email_fetcher,
              scoped_ptr<remoting::ServiceClient> service_client,
              scoped_ptr<remoting::DaemonController> daemon_controller);

  void OnHostStarted(DaemonController::AsyncResult result);

  scoped_ptr<gaia::GaiaOAuthClient> oauth_client_;
  scoped_ptr<remoting::GaiaUserEmailFetcher> user_email_fetcher_;
  scoped_ptr<remoting::ServiceClient> service_client_;
  scoped_ptr<remoting::DaemonController> daemon_controller_;
  bool in_progress_;
  gaia::OAuthClientInfo oauth_client_info_;
  std::string host_name_;
  std::string host_pin_;
  bool consent_to_data_collection_;
  CompletionCallback on_done_;
  std::string refresh_token_;
  std::string access_token_;
  std::string user_email_;
  remoting::HostKeyPair key_pair_;
  std::string host_id_;

  DISALLOW_COPY_AND_ASSIGN(HostStarter);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_STARTER
