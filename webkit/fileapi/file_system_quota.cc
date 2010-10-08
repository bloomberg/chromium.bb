// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_quota.h"

#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/ref_counted.h"
#include "base/scoped_callback_factory.h"

namespace fileapi {

const int64 FileSystemQuota::kUnknownSize = -1;

bool FileSystemQuota::CheckOriginQuota(const GURL& origin, int64) {
  return CheckIfOriginGrantedUnlimitedQuota(origin);
}

void FileSystemQuota::SetOriginQuotaUnlimited(const GURL& origin) {
  DCHECK(origin == origin.GetOrigin());
  unlimited_quota_origins_.insert(origin);
}

void FileSystemQuota::ResetOriginQuotaUnlimited(const GURL& origin) {
  DCHECK(origin == origin.GetOrigin());
  unlimited_quota_origins_.erase(origin);
}

bool FileSystemQuota::CheckIfOriginGrantedUnlimitedQuota(const GURL& origin) {
  std::set<GURL>::const_iterator found = unlimited_quota_origins_.find(origin);
  return (found != unlimited_quota_origins_.end());
}

}  // namespace fileapi
