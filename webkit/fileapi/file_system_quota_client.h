// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_CLIENT_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_CLIENT_H_

#include <set>
#include <string>
#include <utility>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "webkit/fileapi/file_system_quota_util.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/quota/quota_client.h"
#include "webkit/storage/webkit_storage_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace fileapi {

class FileSystemContext;

// An instance of this class is created per-profile.  This class
// is self-destructed and will delete itself when OnQuotaManagerDestroyed
// is called.
// All of the public methods of this class are called by the quota manager
// (except for the constructor/destructor).
class WEBKIT_STORAGE_EXPORT_PRIVATE FileSystemQuotaClient
    : public NON_EXPORTED_BASE(quota::QuotaClient) {
 public:
  FileSystemQuotaClient(
      FileSystemContext* file_system_context,
      bool is_incognito);
  virtual ~FileSystemQuotaClient();

  // QuotaClient methods.
  virtual quota::QuotaClient::ID id() const OVERRIDE;
  virtual void OnQuotaManagerDestroyed() OVERRIDE;
  virtual void GetOriginUsage(const GURL& origin_url,
                              quota::StorageType type,
                              const GetUsageCallback& callback) OVERRIDE;
  virtual void GetOriginsForType(
      quota::StorageType type,
      const GetOriginsCallback& callback) OVERRIDE;
  virtual void GetOriginsForHost(
      quota::StorageType type,
      const std::string& host,
      const GetOriginsCallback& callback) OVERRIDE;
  virtual void DeleteOriginData(
      const GURL& origin,
      quota::StorageType type,
      const DeletionCallback& callback) OVERRIDE;

 private:
  base::SequencedTaskRunner* file_task_runner() const;

  scoped_refptr<FileSystemContext> file_system_context_;

  bool is_incognito_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemQuotaClient);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_CLIENT_H_
