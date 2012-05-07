// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_UTIL_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_UTIL_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_types.h"

namespace base {
class SequencedTaskRunner;
}

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
  // Methods of this class can be called on any thread.
  class Proxy : public base::RefCountedThreadSafe<Proxy> {
   public:
    void UpdateOriginUsage(quota::QuotaManagerProxy* proxy,
                           const GURL& origin_url,
                           fileapi::FileSystemType type,
                           int64 delta);
    void StartUpdateOrigin(const GURL& origin_url,
                           fileapi::FileSystemType type);
    void EndUpdateOrigin(const GURL& origin_url,
                         fileapi::FileSystemType type);

   private:
    friend class FileSystemQuotaUtil;
    friend class base::RefCountedThreadSafe<Proxy>;
    Proxy(FileSystemQuotaUtil* quota_handler,
          base::SequencedTaskRunner* file_task_runner);
    ~Proxy();

    FileSystemQuotaUtil* quota_util_;  // Accessed only on the FILE thread.
    scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

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
  // Called before a write operation modifies the origin's storage data.
  virtual void StartUpdateOriginOnFileThread(const GURL& origin_url,
                                             fileapi::FileSystemType type) = 0;

  // Called by quota_file_util or file_writer_delegate.
  // Called after a write operation modifies the origin's storage data.
  virtual void EndUpdateOriginOnFileThread(const GURL& origin_url,
                                           fileapi::FileSystemType type) = 0;

  virtual void InvalidateUsageCache(const GURL& origin_url,
                                    fileapi::FileSystemType type) = 0;

  Proxy* proxy() { return proxy_.get(); }

 protected:
  explicit FileSystemQuotaUtil(base::SequencedTaskRunner* file_task_runner);
  virtual ~FileSystemQuotaUtil();

 private:
  scoped_refptr<Proxy> proxy_;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_UTIL_H_
