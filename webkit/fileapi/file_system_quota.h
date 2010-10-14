// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_H_

#include <set>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"

namespace fileapi {

// A quota manager for FileSystem. For now it has little implementation
// and just allows unlimited quota for apps.
class FileSystemQuota {
 public:
  static const int64 kUnknownSize;

  FileSystemQuota();
  ~FileSystemQuota();

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

  DISALLOW_COPY_AND_ASSIGN(FileSystemQuota);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_H_
