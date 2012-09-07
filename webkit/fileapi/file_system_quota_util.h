// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_UTIL_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_UTIL_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/fileapi_export.h"
#include "webkit/fileapi/file_system_types.h"

namespace base {
class SequencedTaskRunner;
}

namespace quota {
class QuotaManagerProxy;
}

namespace fileapi {

class FileSystemContext;

// An abstract interface that provides common quota-related utility functions
// for file_system_quota_client.
// All the methods of this class are synchronous and need to be called on
// the thread that the method name implies.
class FILEAPI_EXPORT FileSystemQuotaUtil {
 public:
  virtual ~FileSystemQuotaUtil() {}

  virtual void GetOriginsForTypeOnFileThread(fileapi::FileSystemType type,
                                             std::set<GURL>* origins) = 0;

  virtual void GetOriginsForHostOnFileThread(fileapi::FileSystemType type,
                                             const std::string& host,
                                             std::set<GURL>* origins) = 0;

  // Returns the amount of data used for the origin for usage tracking.
  virtual int64 GetOriginUsageOnFileThread(
      fileapi::FileSystemContext* file_system_context,
      const GURL& origin_url,
      fileapi::FileSystemType type) = 0;

  virtual void InvalidateUsageCache(const GURL& origin_url,
                                    fileapi::FileSystemType type) = 0;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_UTIL_H_
