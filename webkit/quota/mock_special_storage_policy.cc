// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/quota/mock_special_storage_policy.h"

namespace quota {

MockSpecialStoragePolicy::MockSpecialStoragePolicy()
    : all_unlimited_(false) {
}

bool MockSpecialStoragePolicy::IsStorageProtected(const GURL& origin) {
  return protected_.find(origin) != protected_.end();
}

bool MockSpecialStoragePolicy::IsStorageUnlimited(const GURL& origin) {
  if (all_unlimited_)
    return true;
  return unlimited_.find(origin) != unlimited_.end();
}

bool MockSpecialStoragePolicy::IsStorageSessionOnly(const GURL& origin) {
  return session_only_.find(origin) != session_only_.end();
}

bool MockSpecialStoragePolicy::CanQueryDiskSize(const GURL& origin) {
  return can_query_disk_size_.find(origin) != can_query_disk_size_.end();
}

bool MockSpecialStoragePolicy::IsFileHandler(const std::string& extension_id) {
  return file_handlers_.find(extension_id) != file_handlers_.end();
}

bool MockSpecialStoragePolicy::HasSessionOnlyOrigins() {
  return !session_only_.empty();
}

MockSpecialStoragePolicy::~MockSpecialStoragePolicy() {}

}  // namespace quota
