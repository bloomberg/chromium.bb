// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_SANDBOX_CONTEXT_H_
#define WEBKIT_BROWSER_FILEAPI_SANDBOX_CONTEXT_H_

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/browser/fileapi/file_system_quota_util.h"
#include "webkit/browser/webkit_storage_browser_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace quota {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

namespace fileapi {

class AsyncFileUtilAdapter;
class FileSystemUsageCache;
class ObfuscatedFileUtil;
class SandboxFileSystemBackend;
class SandboxFileSystemTestHelper;
class SandboxQuotaObserver;

// This class keeps and provides a sandbox file system context.
// An instance of this class is created and owned by FileSystemContext.
class WEBKIT_STORAGE_BROWSER_EXPORT SandboxContext {
 public:
  // The FileSystem directory name.
  static const base::FilePath::CharType kFileSystemDirectory[];

  SandboxContext(
      quota::QuotaManagerProxy* quota_manager_proxy,
      base::SequencedTaskRunner* file_task_runner,
      const base::FilePath& profile_path,
      quota::SpecialStoragePolicy* special_storage_policy);

  ~SandboxContext();

  base::SequencedTaskRunner* file_task_runner() {
    return file_task_runner_.get();
  }

  AsyncFileUtilAdapter* file_util() { return sandbox_file_util_.get(); }
  FileSystemUsageCache* usage_cache() { return file_system_usage_cache_.get(); }
  SandboxQuotaObserver* quota_observer() { return quota_observer_.get(); };

  quota::SpecialStoragePolicy* special_storage_policy() {
    return special_storage_policy_.get();
  }

  ObfuscatedFileUtil* sync_file_util();

  bool is_usage_tracking_enabled() { return is_usage_tracking_enabled_; }

 private:
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  scoped_ptr<AsyncFileUtilAdapter> sandbox_file_util_;
  scoped_ptr<FileSystemUsageCache> file_system_usage_cache_;
  scoped_ptr<SandboxQuotaObserver> quota_observer_;

  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;

  // Indicates if the usage tracking for FileSystem is enabled or not.
  // The usage tracking is enabled by default and can be disabled by
  // a command-line switch (--disable-file-system-usage-tracking).
  bool is_usage_tracking_enabled_;

  DISALLOW_COPY_AND_ASSIGN(SandboxContext);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_SANDBOX_CONTEXT_H_
