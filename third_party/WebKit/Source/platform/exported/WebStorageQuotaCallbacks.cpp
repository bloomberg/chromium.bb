// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebStorageQuotaCallbacks.h"

#include "platform/StorageQuotaCallbacks.h"

namespace blink {

WebStorageQuotaCallbacks::WebStorageQuotaCallbacks(
    StorageQuotaCallbacks* callbacks)
    : private_(callbacks) {}

void WebStorageQuotaCallbacks::Reset() {
  private_.Reset();
}

void WebStorageQuotaCallbacks::Assign(const WebStorageQuotaCallbacks& other) {
  private_ = other.private_;
}

void WebStorageQuotaCallbacks::DidQueryStorageUsageAndQuota(
    unsigned long long usage_in_bytes,
    unsigned long long quota_in_bytes) {
  ASSERT(!private_.IsNull());
  private_->DidQueryStorageUsageAndQuota(usage_in_bytes, quota_in_bytes);
  private_.Reset();
}

void WebStorageQuotaCallbacks::DidGrantStorageQuota(
    unsigned long long usage_in_bytes,
    unsigned long long granted_quota_in_bytes) {
  ASSERT(!private_.IsNull());
  private_->DidGrantStorageQuota(usage_in_bytes, granted_quota_in_bytes);
  private_.Reset();
}

void WebStorageQuotaCallbacks::DidFail(WebStorageQuotaError error) {
  ASSERT(!private_.IsNull());
  private_->DidFail(error);
  private_.Reset();
}

}  // namespace blink
