// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class implements the OAuth2 client for the Chromoting host.

#ifndef REMOTING_HOST_OAUTH_CLIENT_H_
#define REMOTING_HOST_OAUTH_CLIENT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "remoting/host/gaia_oauth_client.h"
#include "remoting/host/url_request_context.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace remoting {

class OAuthClient : public GaiaOAuthClient::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() { }
    // Invoked on a successful response to the RefreshToken request.
    virtual void OnRefreshTokenResponse(const std::string& access_token,
                                        int expires_in_seconds) = 0;
    // Invoked when there is an OAuth error with one of the requests.
    virtual void OnOAuthError() = 0;
  };

  OAuthClient();
  virtual ~OAuthClient();

  // Notify the delegate on the speficied message loop when an access token is
  // available. As long as the refresh is successful, another will be queued,
  // timed just before the access token expires. As soon as an error occurs,
  // this automatic refresh behaviour is cancelled.
  //
  // The delegate is accessed on the specified message loop, and must out-live
  // it.
  void Start(const std::string& refresh_token,
             Delegate* delegate,
             base::MessageLoopProxy* message_loop);

  // Overridden from GaiaOAuthClient::Delegate
  virtual void OnGetTokensResponse(const std::string& refresh_token,
                                   const std::string& access_token,
                                   int expires_in_seconds) OVERRIDE;
  virtual void OnRefreshTokenResponse(const std::string& access_token,
                                      int expires) OVERRIDE;
  virtual void OnOAuthError() OVERRIDE;
  virtual void OnNetworkError(int response_code) OVERRIDE;

 private:
  void RefreshToken();

  // TODO(jamiewalch): Move these to the ChromotingHostContext class so
  // that the URLRequestContextGetter is available for other purposes.
  base::Thread network_thread_;
  base::Thread file_thread_;
  scoped_refptr<URLRequestContextGetter> url_request_context_getter_;
  scoped_ptr<GaiaOAuthClient> gaia_oauth_client_;
  std::string refresh_token_;
  OAuthClient::Delegate* delegate_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;

  DISALLOW_COPY_AND_ASSIGN(OAuthClient);
};

}  // namespace remoting

#endif  // REMOTING_HOST_OAUTH_CLIENT_H_
