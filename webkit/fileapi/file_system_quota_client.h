// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_CLIENT_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_CLIENT_H_

#include <set>
#include <string>
#include <utility>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/file_system_quota_util.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/quota/quota_client.h"
#include "webkit/quota/quota_task.h"

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
class FileSystemQuotaClient : public quota::QuotaClient,
                              public quota::QuotaTaskObserver {
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
  class GetOriginUsageTask;
  class GetOriginsTaskBase;
  class GetOriginsForTypeTask;
  class GetOriginsForHostTask;
  class DeleteOriginTask;

  typedef std::pair<fileapi::FileSystemType, std::string> TypeAndHostOrOrigin;
  typedef quota::CallbackQueueMap1<GetUsageCallback,
                                   TypeAndHostOrOrigin,
                                   int64
                                   > UsageCallbackMap;
  typedef quota::CallbackQueueMap2<GetOriginsCallback,
                                   fileapi::FileSystemType,
                                   const std::set<GURL>&,
                                   quota::StorageType
                                   > OriginsForTypeCallbackMap;
  typedef quota::CallbackQueueMap2<GetOriginsCallback,
                                   TypeAndHostOrOrigin,
                                   const std::set<GURL>&,
                                   quota::StorageType
                                   > OriginsForHostCallbackMap;

  void DidGetOriginUsage(fileapi::FileSystemType type,
                         const GURL& origin, int64 usage);
  void DidGetOriginsForType(fileapi::FileSystemType type,
                            const std::set<GURL>& origins);
  void DidGetOriginsForHost(const TypeAndHostOrOrigin& type_and_host,
                            const std::set<GURL>& origins);

  base::SequencedTaskRunner* file_task_runner() const;

  scoped_refptr<FileSystemContext> file_system_context_;

  bool is_incognito_;

  // Pending callbacks.
  UsageCallbackMap pending_usage_callbacks_;
  OriginsForTypeCallbackMap pending_origins_for_type_callbacks_;
  OriginsForHostCallbackMap pending_origins_for_host_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemQuotaClient);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_QUOTA_CLIENT_H_
