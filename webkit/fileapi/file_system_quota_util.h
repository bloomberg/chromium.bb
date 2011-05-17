// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_UTIL_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_UTIL_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_types.h"

namespace quota {
class QuotaManagerProxy;
}

namespace fileapi {

// An abstract interface that provides common quota-related utility functions
// for internal filesystem modules.  The main consumer of this class is
// file_system_quota_client and quota_file_util.
// All the methods of this class are synchronous and need to be called on
// the thread that the method name implies.
class FileSystemQuotaUtil {
 public:
  // Called by quota client.
  virtual void GetOriginsForTypeOnFileThread(fileapi::FileSystemType type,
                                             std::set<GURL>* origins) = 0;

  // Called by quota client.
  virtual void GetOriginsForHostOnFileThread(fileapi::FileSystemType type,
                                             const std::string& host,
                                             std::set<GURL>* origins) = 0;

  // Called by quota client.
  // Returns the amount of data used for the origin for usage tracking.
  virtual int64 GetOriginUsageOnFileThread(const GURL& origin_url,
                                           fileapi::FileSystemType type) = 0;

  // Called by quota file util.
  // Returns the amount of data used for the origin for usage tracking.
  virtual void UpdateOriginUsageOnFileThread(quota::QuotaManagerProxy* proxy,
                                             const GURL& origin_url,
                                             fileapi::FileSystemType type,
                                             int64 delta) = 0;

  // Called by file_system_operation.
  // Called when a read operation accesses the origin's storage data.
  virtual void NotifyOriginWasAccessedOnIOThread(
      quota::QuotaManagerProxy* proxy,
      const GURL& origin_url,
      fileapi::FileSystemType type) = 0;

  // Called by quota_file_util or file_writer_delegate.
  // Called before an write operation modifies the origin's storage data.
  virtual void StartUpdateOriginOnFileThread(const GURL& origin_url,
                                             fileapi::FileSystemType type) = 0;

  // Called by quota_file_util or file_writer_delegate.
  // Called after an write operation modifies the origin's storage data.
  virtual void EndUpdateOriginOnFileThread(const GURL& origin_url,
                                           fileapi::FileSystemType type) = 0;

 protected:
  virtual ~FileSystemQuotaUtil() {}
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_UTIL_H_
