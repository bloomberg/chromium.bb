// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_FACADE_IOS_OAUTH_TOKEN_GETTER_H_
#define REMOTING_IOS_FACADE_IOS_OAUTH_TOKEN_GETTER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/oauth_token_getter.h"

namespace remoting {

// The OAuthTokenGetter implementation on iOS client that uses
// RemotingService.instance.authentication to authenticate. Please use it only
// on one thread.
class IosOauthTokenGetter : public OAuthTokenGetter {
 public:
  IosOauthTokenGetter();
  ~IosOauthTokenGetter() override;

  // OAuthTokenGetter overrides.
  void CallWithToken(TokenCallback on_access_token) override;
  void InvalidateCache() override;

  base::WeakPtr<IosOauthTokenGetter> GetWeakPtr();

 private:
  base::WeakPtr<IosOauthTokenGetter> weak_ptr_;
  base::WeakPtrFactory<IosOauthTokenGetter> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(IosOauthTokenGetter);
};

}  // namespace remoting

#endif  // REMOTING_IOS_FACADE_IOS_OAUTH_TOKEN_GETTER_H_
