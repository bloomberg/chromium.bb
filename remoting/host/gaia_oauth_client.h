// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_GAIA_OAUTH_CLIENT_H_
#define REMOTING_HOST_GAIA_OAUTH_CLIENT_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

// A helper class to get and refresh OAuth tokens given an authorization code.
//
// TODO(jamiewalch): This is copied from chrome/common/net to avoid a dependency
// on chrome. It would be better for this class to be moved into net to avoid
// this duplication.
namespace remoting {

// TODO(jamiewalch): Make this configurable if we ever support other providers.
static const char kGaiaOAuth2Url[] =
    "https://accounts.google.com/o/oauth2/token";

struct OAuthClientInfo {
  std::string client_id;
  std::string client_secret;
};

class GaiaOAuthClient {
 public:
  class Delegate {
   public:
    virtual ~Delegate() { }

    // Invoked on a successful response to the RefreshToken request.
    virtual void OnRefreshTokenResponse(const std::string& access_token,
                                        int expires_in_seconds) = 0;
    // Invoked when there is an OAuth error with one of the requests.
    virtual void OnOAuthError() = 0;
    // Invoked when there is a network error or upon receiving an
    // invalid response.
    virtual void OnNetworkError(int response_code) = 0;
  };
  GaiaOAuthClient(const std::string& gaia_url,
                  net::URLRequestContextGetter* context_getter);
  ~GaiaOAuthClient();

  void RefreshToken(const OAuthClientInfo& oauth_client_info,
                    const std::string& refresh_token,
                    Delegate* delegate);

 private:
  // The guts of the implementation live in this class.
  class Core;
  scoped_refptr<Core> core_;
  DISALLOW_COPY_AND_ASSIGN(GaiaOAuthClient);
};

}  // namespace remoting

#endif  // CHROME_COMMON_NET_GAIA_GAIA_OAUTH_CLIENT_H_
