// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_quota_manager.h"

#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/ref_counted.h"
#include "base/scoped_callback_factory.h"
#include "webkit/quota/special_storage_policy.h"

namespace fileapi {

const int64 FileSystemQuotaManager::kUnknownSize = -1;

FileSystemQuotaManager::FileSystemQuotaManager(
    bool allow_file_access_from_files,
    bool unlimited_quota,
    quota::SpecialStoragePolicy* special_storage_policy)
    : allow_file_access_from_files_(allow_file_access_from_files),
      unlimited_quota_(unlimited_quota),
      special_storage_policy_(special_storage_policy) {
}

FileSystemQuotaManager::~FileSystemQuotaManager() {}

bool FileSystemQuotaManager::CheckOriginQuota(const GURL& origin, int64) {
  // If allow-file-access-from-files flag is explicitly given and the scheme
  // is file, or if unlimited quota for this process was explicitly requested,
  // return true.
  return unlimited_quota_ ||
      (allow_file_access_from_files_ && origin.SchemeIsFile()) ||
      (special_storage_policy_.get() &&
          special_storage_policy_->IsStorageUnlimited(origin));
}

}  // namespace fileapi
