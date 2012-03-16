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

namespace remoting {

class OAuthClient {
 public:
  OAuthClient();
  ~OAuthClient();

  void GetAccessToken(const std::string& refresh_token,
                      GaiaOAuthClient::Delegate* delegate);

 private:
  // TODO(jamiewalch): Move these to the ChromotingHostContext class so
  // that the URLRequestContextGetter is available for other purposes.
  base::Thread network_thread_;
  base::Thread file_thread_;
  scoped_ptr<URLRequestContextGetter> url_request_context_getter_;
  scoped_ptr<GaiaOAuthClient> gaia_oauth_client_;

  DISALLOW_COPY_AND_ASSIGN(OAuthClient);
};

}  // namespace remoting

#endif  // REMOTING_HOST_OAUTH_CLIENT_H_
