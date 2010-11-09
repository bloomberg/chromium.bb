// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_MANAGER_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_MANAGER_H_

#include <set>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"

namespace fileapi {

// A quota manager for FileSystem. For now it has little implementation
// and just allows unlimited quota for apps.
class FileSystemQuotaManager {
 public:
  static const int64 kUnknownSize;

  // If |allow_file_access_from_files| is true, unlimited access is granted
  // for file:/// URLs.
  // If |unlimited_quota| is true, unlimited access is granted for every
  // origin.  This flag must be used only for testing.
  FileSystemQuotaManager(bool allow_file_access_from_files,
                         bool unlimited_quota);
  ~FileSystemQuotaManager();

  // Checks if the origin can grow its usage by |growth| bytes.
  // This only performs in-memory check and returns immediately.
  // For now it just returns false for any origins (regardless of the size)
  // that are not in the in-memory unlimited_quota_origins map.
  bool CheckOriginQuota(const GURL& origin, int64 growth);

  // Maintains origins in memory that are allowed to have unlimited quota.
  void SetOriginQuotaUnlimited(const GURL& origin);
  void ResetOriginQuotaUnlimited(const GURL& origin);
  bool CheckIfOriginGrantedUnlimitedQuota(const GURL& origin);

 private:
  // For some extensions/apps we allow unlimited quota.
  std::set<GURL> unlimited_quota_origins_;

  const bool allow_file_access_from_files_;
  const bool unlimited_quota_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemQuotaManager);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_MANAGER_H_
