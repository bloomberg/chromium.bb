// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTIFIER_GCM_NETWORK_CHANNEL_DELEGATE_H_
#define SYNC_NOTIFIER_GCM_NETWORK_CHANNEL_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "google_apis/gcm/gcm_client.h"

class GoogleServiceAuthError;

namespace syncer {

// Delegate for GCMNetworkChannel.
// GCMNetworkChannel needs Register to register with GCM client and obtain gcm
// registration id. This id is used for building URL to cache invalidation
// endpoint.
// It needs RequestToken and InvalidateToken to get access token to include it
// in HTTP message to server.
// GCMNetworkChannel lives on IO thread therefore calls will be made on IO
// thread and callbacks should be invoked there as well.
class GCMNetworkChannelDelegate {
 public:
  typedef base::Callback<void(const GoogleServiceAuthError& error,
                              const std::string& token)> RequestTokenCallback;
  typedef base::Callback<void(const std::string& registration_id,
                              gcm::GCMClient::Result result)> RegisterCallback;

  virtual ~GCMNetworkChannelDelegate() {}

  virtual void Initialize() = 0;
  // Request access token. Callback should be called either with access token or
  // error code.
  virtual void RequestToken(RequestTokenCallback callback) = 0;
  // Invalidate access token that was rejected by server.
  virtual void InvalidateToken(const std::string& token) = 0;

  // Register with GCMProfileService. Callback should be called with either
  // registration id or error code.
  virtual void Register(RegisterCallback callback) = 0;
};
}  // namespace syncer

#endif  // SYNC_NOTIFIER_GCM_NETWORK_CHANNEL_DELEGATE_H_
