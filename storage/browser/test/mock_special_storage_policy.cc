// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_special_storage_policy.h"

#include "base/stl_util.h"

namespace content {

MockSpecialStoragePolicy::MockSpecialStoragePolicy() : all_unlimited_(false) {}

bool MockSpecialStoragePolicy::IsStorageProtected(const GURL& origin) {
  return base::ContainsKey(protected_, origin);
}

bool MockSpecialStoragePolicy::IsStorageUnlimited(const GURL& origin) {
  if (all_unlimited_)
    return true;
  return base::ContainsKey(unlimited_, origin);
}

bool MockSpecialStoragePolicy::IsStorageSessionOnly(const GURL& origin) {
  return base::ContainsKey(session_only_, origin);
}

bool MockSpecialStoragePolicy::ShouldDeleteCookieOnExit(const GURL& origin) {
  return base::ContainsKey(session_only_, origin);
}

bool MockSpecialStoragePolicy::HasIsolatedStorage(const GURL& origin) {
  return base::ContainsKey(isolated_, origin);
}

bool MockSpecialStoragePolicy::HasSessionOnlyOrigins() {
  return !session_only_.empty();
}

bool MockSpecialStoragePolicy::IsStorageDurable(const GURL& origin) {
  return base::ContainsKey(durable_, origin);
}

MockSpecialStoragePolicy::~MockSpecialStoragePolicy() = default;

}  // namespace content
