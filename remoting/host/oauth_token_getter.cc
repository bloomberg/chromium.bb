// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/oauth_token_getter_impl.h"

namespace remoting {

OAuthTokenGetter::OAuthCredentials::OAuthCredentials(
    const std::string& login,
    const std::string& refresh_token,
    bool is_service_account)
    : login(login),
      refresh_token(refresh_token),
      is_service_account(is_service_account) {
}

}  // namespace remoting
