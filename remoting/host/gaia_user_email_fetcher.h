// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_GAIA_USER_EMAIL_FETCHER_H_
#define REMOTING_HOST_GAIA_USER_EMAIL_FETCHER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

// A helper class to get a user's email, given an OAuth access token.
// TODO(simonmorris): Consider moving this to google_apis/gaia.
namespace remoting {

class GaiaUserEmailFetcher {
 public:
  class Delegate {
   public:
    virtual ~Delegate() { }

    // Invoked on a successful response to the GetUserEmail request.
    virtual void OnGetUserEmailResponse(const std::string& user_email) = 0;
    // Invoked when there is a network error or upon receiving an
    // invalid response.
    virtual void OnNetworkError(int response_code) = 0;
  };

  GaiaUserEmailFetcher(net::URLRequestContextGetter* context_getter);
  ~GaiaUserEmailFetcher();

  void GetUserEmail(const std::string& oauth_access_token,
                    Delegate* delegate);

 private:
  // The guts of the implementation live in this class.
  class Core;
  scoped_refptr<Core> core_;
  DISALLOW_COPY_AND_ASSIGN(GaiaUserEmailFetcher);
};

}  // namespace remoting

#endif  // REMOTING_HOST_GAIA_USER_EMAIL_FETCHER_H_
