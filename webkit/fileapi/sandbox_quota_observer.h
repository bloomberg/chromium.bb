// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SANDBOX_QUOTA_OBSERVER_H_
#define WEBKIT_FILEAPI_SANDBOX_QUOTA_OBSERVER_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "webkit/fileapi/file_observers.h"
#include "webkit/fileapi/file_system_url.h"

namespace base {
class SequencedTaskRunner;
}

namespace quota {
class QuotaManagerProxy;
}

namespace fileapi {

class FileSystemURL;
class ObfuscatedFileUtil;

class SandboxQuotaObserver
    : public FileUpdateObserver,
      public FileAccessObserver {
 public:
  typedef std::map<base::FilePath, int64> PendingUpdateNotificationMap;

  SandboxQuotaObserver(
      quota::QuotaManagerProxy* quota_manager_proxy,
      base::SequencedTaskRunner* update_notify_runner,
      ObfuscatedFileUtil* sandbox_file_util);
  virtual ~SandboxQuotaObserver();

  // FileUpdateObserver overrides.
  virtual void OnStartUpdate(const FileSystemURL& url) OVERRIDE;
  virtual void OnUpdate(const FileSystemURL& url, int64 delta) OVERRIDE;
  virtual void OnEndUpdate(const FileSystemURL& url) OVERRIDE;

  // FileAccessObserver overrides.
  virtual void OnAccess(const FileSystemURL& url) OVERRIDE;

 private:
  void ApplyPendingUsageUpdate();
  void UpdateUsageCacheFile(const base::FilePath& usage_file_path, int64 delta);

  base::FilePath GetUsageCachePath(const FileSystemURL& url);

  scoped_refptr<quota::QuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<base::SequencedTaskRunner> update_notify_runner_;

  // Not owned; sandbox_file_util_ should have identical lifetime with this.
  ObfuscatedFileUtil* sandbox_file_util_;

  PendingUpdateNotificationMap pending_update_notification_;
  bool running_delayed_cache_update_;

  base::WeakPtrFactory<SandboxQuotaObserver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SandboxQuotaObserver);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SANDBOX_QUOTA_OBSERVER_H_
