// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/sandbox_quota_client.h"

#include <algorithm>
#include <set>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/task.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"

using base::MessageLoopProxy;
using quota::QuotaThreadTask;
using quota::StorageType;

namespace fileapi {

class SandboxQuotaClient::GetOriginUsageTask : public QuotaThreadTask {
 public:
  GetOriginUsageTask(
      SandboxQuotaClient* quota_client,
      scoped_refptr<MessageLoopProxy> file_message_loop,
      const GURL& origin_url,
      FileSystemType type)
      : QuotaThreadTask(quota_client, file_message_loop),
        quota_client_(quota_client),
        origin_url_(origin_url),
        type_(type),
        fs_usage_(0) {
    DCHECK(quota_client_);
    file_system_context_ = quota_client_->file_system_context_;
    visited_ = (quota_client_->visited_origins_.find(origin_url) !=
                quota_client_->visited_origins_.end());
  }

  virtual ~GetOriginUsageTask() {}

 protected:
  virtual void RunOnTargetThread() OVERRIDE {
    FilePath base_path =
        file_system_context_->path_manager()->sandbox_provider()->
            GetBaseDirectoryForOriginAndType(origin_url_, type_);
    if (!file_util::DirectoryExists(base_path)) {
      fs_usage_ = 0;
    } else {
      FilePath usage_file_path = base_path.AppendASCII(
          FileSystemUsageCache::kUsageFileName);
      int32 dirty_status = FileSystemUsageCache::GetDirty(usage_file_path);
      if (dirty_status == 0 || (dirty_status > 0 && visited_)) {
        // The usage cache is clean (dirty == 0) or the origin has already
        // initialized and running.  Read the cache file to get the usage.
        fs_usage_ = FileSystemUsageCache::GetUsage(usage_file_path);
      } else {
        // The usage cache has not been initialized or the cache is dirty.
        // Get the directory size now and update the cache.
        if (FileSystemUsageCache::Exists(usage_file_path))
          FileSystemUsageCache::Delete(usage_file_path);
        int64 usage = file_util::ComputeDirectorySize(base_path);
        // The result of ComputeDirectorySize does not include .usage file size.
        usage += FileSystemUsageCache::kUsageFileSize;
        // This clears the dirty flag too.
        FileSystemUsageCache::UpdateUsage(usage_file_path, usage);
        fs_usage_ = usage;
      }
    }
  }

  virtual void Completed() OVERRIDE {
    quota_client_->DidGetOriginUsage(type_, origin_url_, fs_usage_);
  }

  SandboxQuotaClient* quota_client_;
  scoped_refptr<FileSystemContext> file_system_context_;
  GURL origin_url_;
  FileSystemType type_;
  int64 fs_usage_;
  bool visited_;
};

class SandboxQuotaClient::GetOriginsTaskBase : public QuotaThreadTask {
 public:
  GetOriginsTaskBase(
      SandboxQuotaClient* quota_client,
      scoped_refptr<MessageLoopProxy> file_message_loop,
      FileSystemType type)
      : QuotaThreadTask(quota_client, file_message_loop),
        quota_client_(quota_client),
        type_(type) {
    DCHECK(quota_client_);
    file_system_context_ = quota_client_->file_system_context_;
  }
  virtual ~GetOriginsTaskBase() {}

 protected:
  virtual bool ShouldAddThisOrigin(const GURL& origin) const = 0;

  virtual void RunOnTargetThread() OVERRIDE {
    scoped_ptr<SandboxMountPointProvider::OriginEnumerator> enumerator(
        sandbox_provider()->CreateOriginEnumerator());
    GURL origin;
    while (!(origin = enumerator->Next()).is_empty()) {
      if (ShouldAddThisOrigin(origin) && enumerator->HasFileSystemType(type_))
        origins_.insert(origin);
    }
  }

  SandboxQuotaClient* quota_client() const { return quota_client_; }
  const std::set<GURL>& origins() const { return origins_; }
  FileSystemType type() const { return type_; }
  SandboxMountPointProvider* sandbox_provider() const {
    return file_system_context_->path_manager()->sandbox_provider();
  }

 private:
  SandboxQuotaClient * quota_client_;
  scoped_refptr<FileSystemContext> file_system_context_;
  std::set<GURL> origins_;
  FileSystemType type_;
};

class SandboxQuotaClient::GetOriginsForTypeTask
    : public SandboxQuotaClient::GetOriginsTaskBase {
 public:
  GetOriginsForTypeTask(
      SandboxQuotaClient* quota_client,
      scoped_refptr<MessageLoopProxy> file_message_loop,
      FileSystemType type)
      : GetOriginsTaskBase(quota_client, file_message_loop, type) {}
  virtual ~GetOriginsForTypeTask() {}

 protected:
  virtual bool ShouldAddThisOrigin(const GURL& origin) const OVERRIDE {
    return true;
  }

  virtual void Completed() OVERRIDE {
    quota_client()->DidGetOriginsForType(type(), origins());
  }
};

quota::QuotaClient::ID SandboxQuotaClient::id() const {
  return quota::QuotaClient::kFileSystem;
}

void SandboxQuotaClient::OnQuotaManagerDestroyed() {
  delete this;
}

class SandboxQuotaClient::GetOriginsForHostTask
    : public SandboxQuotaClient::GetOriginsTaskBase {
 public:
  GetOriginsForHostTask(
      SandboxQuotaClient* quota_client,
      scoped_refptr<MessageLoopProxy> file_message_loop,
      FileSystemType type,
      const std::string& host)
      : GetOriginsTaskBase(quota_client, file_message_loop, type),
        host_(host) {}
  virtual ~GetOriginsForHostTask() {}

 protected:
  virtual bool ShouldAddThisOrigin(const GURL& origin) const OVERRIDE {
    return (host_ == net::GetHostOrSpecFromURL(origin));
  }

  virtual void Completed() OVERRIDE {
    quota_client()->DidGetOriginsForHost(std::make_pair(type(), host_),
                                         origins());
  }

 private:
  std::string host_;
};

class SandboxQuotaClient::DeleteOriginTask
    : public QuotaThreadTask {
 public:
  DeleteOriginTask(
      SandboxQuotaClient* quota_client,
      scoped_refptr<MessageLoopProxy> file_message_loop,
      const GURL& origin,
      FileSystemType type,
      DeletionCallback* callback)
      : QuotaThreadTask(quota_client, file_message_loop),
        file_system_context_(quota_client->file_system_context_),
        origin_(origin),
        type_(type),
        status_(quota::kQuotaStatusUnknown),
        callback_(callback) {
  }

  virtual ~DeleteOriginTask() {}

  virtual void RunOnTargetThread() OVERRIDE {
    if (file_system_context_->
            DeleteDataForOriginAndTypeOnFileThread(origin_, type_))
      status_ = quota::kQuotaStatusOk;
    else
      status_ = quota::kQuotaErrorInvalidModification;
  }

  virtual void Completed() OVERRIDE {
    callback_->Run(status_);
  }
 private:
  FileSystemContext* file_system_context_;
  GURL origin_;
  FileSystemType type_;
  quota::QuotaStatusCode status_;
  scoped_ptr<DeletionCallback> callback_;
};

SandboxQuotaClient::SandboxQuotaClient(
    scoped_refptr<base::MessageLoopProxy> file_message_loop,
    FileSystemContext* file_system_context,
    bool is_incognito)
    : file_message_loop_(file_message_loop),
      file_system_context_(file_system_context),
      is_incognito_(is_incognito) {
  DCHECK(file_message_loop);
}

SandboxQuotaClient::~SandboxQuotaClient() {
}

void SandboxQuotaClient::GetOriginUsage(
    const GURL& origin_url,
    StorageType storage_type,
    GetUsageCallback* callback_ptr) {
  DCHECK(callback_ptr);
  scoped_ptr<GetUsageCallback> callback(callback_ptr);

  if (is_incognito_) {
    // We don't support FileSystem in incognito mode yet.
    callback->Run(0);
    return;
  }

  FileSystemType type = QuotaStorageTypeToFileSystemType(storage_type);
  DCHECK(type != kFileSystemTypeUnknown);

  if (pending_usage_callbacks_.Add(
          std::make_pair(type, origin_url.spec()), callback.release())) {
    scoped_refptr<GetOriginUsageTask> task(
        new GetOriginUsageTask(this, file_message_loop_, origin_url, type));
    task->Start();
  }
}

void SandboxQuotaClient::GetOriginsForType(
    StorageType storage_type,
    GetOriginsCallback* callback_ptr) {
  std::set<GURL> origins;
  scoped_ptr<GetOriginsCallback> callback(callback_ptr);
  if (is_incognito_) {
    // We don't support FileSystem in incognito mode yet.
    callback->Run(origins);
    return;
  }

  FileSystemType type = QuotaStorageTypeToFileSystemType(storage_type);
  DCHECK(type != kFileSystemTypeUnknown);

  if (pending_origins_for_type_callbacks_.Add(type, callback.release())) {
    scoped_refptr<GetOriginsForTypeTask> task(
        new GetOriginsForTypeTask(this, file_message_loop_, type));
    task->Start();
  }
}

void SandboxQuotaClient::GetOriginsForHost(
    StorageType storage_type,
    const std::string& host,
    GetOriginsCallback* callback_ptr) {
  std::set<GURL> origins;
  scoped_ptr<GetOriginsCallback> callback(callback_ptr);
  if (is_incognito_) {
    // We don't support FileSystem in incognito mode yet.
    callback->Run(origins);
    return;
  }

  FileSystemType type = QuotaStorageTypeToFileSystemType(storage_type);
  DCHECK(type != kFileSystemTypeUnknown);

  if (pending_origins_for_host_callbacks_.Add(
          std::make_pair(type, host), callback.release())) {
    scoped_refptr<GetOriginsForHostTask> task(
        new GetOriginsForHostTask(this, file_message_loop_,
                                  type, host));
    task->Start();
  }
}

void SandboxQuotaClient::DeleteOriginData(const GURL& origin,
                                          StorageType type,
                                          DeletionCallback* callback) {
  FileSystemType fs_type = QuotaStorageTypeToFileSystemType(type);
  DCHECK(fs_type != kFileSystemTypeUnknown);
  scoped_refptr<DeleteOriginTask> task(
      new DeleteOriginTask(this, file_message_loop_,
                           origin, fs_type, callback));
  task->Start();
}

void SandboxQuotaClient::DidGetOriginUsage(
    FileSystemType type, const GURL& origin_url, int64 usage) {
  visited_origins_.insert(origin_url);
  TypeAndHostOrOrigin type_and_origin(std::make_pair(
      type, origin_url.spec()));
  DCHECK(pending_usage_callbacks_.HasCallbacks(type_and_origin));
  pending_usage_callbacks_.Run(type_and_origin, usage);
}

void SandboxQuotaClient::DidGetOriginsForType(
    FileSystemType type, const std::set<GURL>& origins) {
  DCHECK(pending_origins_for_type_callbacks_.HasCallbacks(type));
  pending_origins_for_type_callbacks_.Run(type, origins);
}

void SandboxQuotaClient::DidGetOriginsForHost(
    const TypeAndHostOrOrigin& type_and_host, const std::set<GURL>& origins) {
  DCHECK(pending_origins_for_host_callbacks_.HasCallbacks(type_and_host));
  pending_origins_for_host_callbacks_.Run(type_and_host, origins);
}

}  // namespace fileapi
