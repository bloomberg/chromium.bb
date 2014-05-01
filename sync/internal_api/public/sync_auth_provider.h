// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SYNC_AUTH_PROVIDER_H_
#define SYNC_INTERNAL_API_PUBLIC_SYNC_AUTH_PROVIDER_H_

#include "base/callback.h"

class GoogleServiceAuthError;

namespace syncer {

// SyncAuthProvider is interface to access token related functions from sync
// engine.
class SyncAuthProvider {
 public:
  typedef base::Callback<void(const GoogleServiceAuthError& error,
                              const std::string& token)> RequestTokenCallback;

  virtual ~SyncAuthProvider() {}

  // Request access token for sync. Callback will be called with error and
  // access token. If error is anything other than NONE then token is invalid.
  virtual void RequestAccessToken(const RequestTokenCallback& callback) = 0;

  // Invalidate access token that was rejected by sync server.
  virtual void InvalidateAccessToken(const std::string& token) = 0;
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_SYNC_AUTH_PROVIDER_H_
