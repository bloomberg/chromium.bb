// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_OAUTH_TOKEN_GETTER_H_
#define REMOTING_HOST_OAUTH_TOKEN_GETTER_H_

#include <string>

#include "base/callback.h"


namespace remoting {

// OAuthTokenGetter caches OAuth access tokens and refreshes them as needed.
class OAuthTokenGetter {
 public:
  // Status of the refresh token attempt.
  enum Status {
    // Success, credentials in user_email/access_token.
    SUCCESS,
    // Network failure (caller may retry).
    NETWORK_ERROR,
    // Authentication failure (permanent).
    AUTH_ERROR,
  };

  typedef base::Callback<void(Status status,
                              const std::string& user_email,
                              const std::string& access_token)> TokenCallback;

  // This structure contains information required to perform
  // authentication to OAuth2.
  struct OAuthCredentials {
    // |is_service_account| should be True if the OAuth refresh token is for a
    // service account, False for a user account, to allow the correct client-ID
    // to be used.
    OAuthCredentials(const std::string& login,
                     const std::string& refresh_token,
                     bool is_service_account);

    // The user's account name (i.e. their email address).
    std::string login;

    // Token delegating authority to us to act as the user.
    std::string refresh_token;

    // Whether these credentials belong to a service account.
    bool is_service_account;
  };

  OAuthTokenGetter() {}
  virtual ~OAuthTokenGetter() {}

  // Call |on_access_token| with an access token, or the failure status.
  virtual void CallWithToken(
      const OAuthTokenGetter::TokenCallback& on_access_token) = 0;

  DISALLOW_COPY_AND_ASSIGN(OAuthTokenGetter);
};

}  // namespace remoting

#endif  // REMOTING_HOST_OAUTH_TOKEN_GETTER_H_
