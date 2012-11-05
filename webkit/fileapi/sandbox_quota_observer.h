// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SANDBOX_QUOTA_OBSERVER_H_
#define WEBKIT_FILEAPI_SANDBOX_QUOTA_OBSERVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/file_observers.h"

namespace base {
class SequencedTaskRunner;
}

namespace quota {
class QuotaManagerProxy;
}

namespace fileapi {

class FileSystemURL;
class SandboxMountPointProvider;

class SandboxQuotaObserver
    : public FileUpdateObserver,
      public FileAccessObserver {
 public:
  SandboxQuotaObserver(
      quota::QuotaManagerProxy* quota_manager_proxy,
      base::SequencedTaskRunner* update_notify_runner,
      SandboxMountPointProvider* sandbox_provider);
  virtual ~SandboxQuotaObserver();

  // FileUpdateObserver overrides.
  virtual void OnStartUpdate(const FileSystemURL& url) OVERRIDE;
  virtual void OnUpdate(const FileSystemURL& url, int64 delta) OVERRIDE;
  virtual void OnEndUpdate(const FileSystemURL& url) OVERRIDE;

  // FileAccessObserver overrides.
  virtual void OnAccess(const FileSystemURL& url) OVERRIDE;

 private:
  FilePath GetUsageCachePath(const FileSystemURL& url);

  scoped_refptr<quota::QuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<base::SequencedTaskRunner> update_notify_runner_;

  // Not owned; provider owns this.
  SandboxMountPointProvider* sandbox_provider_;

  DISALLOW_COPY_AND_ASSIGN(SandboxQuotaObserver);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SANDBOX_QUOTA_OBSERVER_H_
