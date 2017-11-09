// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_FACADE_IOS_OAUTH_TOKEN_GETTER_H_
#define REMOTING_IOS_FACADE_IOS_OAUTH_TOKEN_GETTER_H_

#include "base/macros.h"
#include "remoting/base/oauth_token_getter.h"

namespace remoting {

// The OAuthTokenGetter implementation on iOS client that uses
// RemotingService.instance.authentication to authenticate. Depending on the
// RemotingAuthentication implementation, this class may be single-threaded.
class IosOauthTokenGetter : public OAuthTokenGetter {
 public:
  IosOauthTokenGetter();
  ~IosOauthTokenGetter() override;

  // OAuthTokenGetter overrides.
  void CallWithToken(const TokenCallback& on_access_token) override;
  void InvalidateCache() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(IosOauthTokenGetter);
};

}  // namespace remoting

#endif  // REMOTING_IOS_FACADE_IOS_OAUTH_TOKEN_GETTER_H_
