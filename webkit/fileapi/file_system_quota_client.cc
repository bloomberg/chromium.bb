// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_quota_client.h"

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
#include "webkit/fileapi/file_system_quota_util.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_system_util.h"

using base::MessageLoopProxy;
using quota::QuotaThreadTask;
using quota::StorageType;

namespace fileapi {

class FileSystemQuotaClient::GetOriginUsageTask : public QuotaThreadTask {
 public:
  GetOriginUsageTask(
      FileSystemQuotaClient* quota_client,
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
  }

  virtual ~GetOriginUsageTask() {}

 protected:
  virtual void RunOnTargetThread() OVERRIDE {
    FileSystemQuotaUtil* quota_util = file_system_context_->GetQuotaUtil(type_);
    if (quota_util)
      fs_usage_ = quota_util->GetOriginUsageOnFileThread(origin_url_, type_);
  }

  virtual void Completed() OVERRIDE {
    quota_client_->DidGetOriginUsage(type_, origin_url_, fs_usage_);
  }

  FileSystemQuotaClient* quota_client_;
  scoped_refptr<FileSystemContext> file_system_context_;
  GURL origin_url_;
  FileSystemType type_;
  int64 fs_usage_;
};

class FileSystemQuotaClient::GetOriginsForTypeTask : public QuotaThreadTask {
 public:
  GetOriginsForTypeTask(
      FileSystemQuotaClient* quota_client,
      scoped_refptr<MessageLoopProxy> file_message_loop,
      FileSystemType type)
      : QuotaThreadTask(quota_client, file_message_loop),
        quota_client_(quota_client),
        type_(type) {
    DCHECK(quota_client_);
    file_system_context_ = quota_client_->file_system_context_;
  }
  virtual ~GetOriginsForTypeTask() {}

 protected:
  virtual void RunOnTargetThread() OVERRIDE {
    FileSystemQuotaUtil* quota_util = file_system_context_->GetQuotaUtil(type_);
    if (quota_util)
      quota_util->GetOriginsForTypeOnFileThread(type_, &origins_);
  }

  virtual void Completed() OVERRIDE {
    quota_client_->DidGetOriginsForType(type_, origins_);
  }

 private:
  FileSystemQuotaClient* quota_client_;
  scoped_refptr<FileSystemContext> file_system_context_;
  std::set<GURL> origins_;
  FileSystemType type_;
};

class FileSystemQuotaClient::GetOriginsForHostTask : public QuotaThreadTask {
 public:
  GetOriginsForHostTask(
      FileSystemQuotaClient* quota_client,
      scoped_refptr<MessageLoopProxy> file_message_loop,
      FileSystemType type,
      const std::string& host)
      : QuotaThreadTask(quota_client, file_message_loop),
        quota_client_(quota_client),
        type_(type),
        host_(host) {
    DCHECK(quota_client_);
    file_system_context_ = quota_client_->file_system_context_;
  }
  virtual ~GetOriginsForHostTask() {}

 protected:
  virtual void RunOnTargetThread() OVERRIDE {
    FileSystemQuotaUtil* quota_util = file_system_context_->GetQuotaUtil(type_);
    if (quota_util)
      quota_util->GetOriginsForHostOnFileThread(type_, host_, &origins_);
  }

  virtual void Completed() OVERRIDE {
    quota_client_->DidGetOriginsForHost(std::make_pair(type_, host_), origins_);
  }

 private:
  FileSystemQuotaClient* quota_client_;
  scoped_refptr<FileSystemContext> file_system_context_;
  std::set<GURL> origins_;
  FileSystemType type_;
  std::string host_;
};

class FileSystemQuotaClient::DeleteOriginTask
    : public QuotaThreadTask {
 public:
  DeleteOriginTask(
      FileSystemQuotaClient* quota_client,
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
    if (file_system_context_->DeleteDataForOriginAndTypeOnFileThread(
            origin_, type_))
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

FileSystemQuotaClient::FileSystemQuotaClient(
    scoped_refptr<base::MessageLoopProxy> file_message_loop,
    FileSystemContext* file_system_context,
    bool is_incognito)
    : file_message_loop_(file_message_loop),
      file_system_context_(file_system_context),
      is_incognito_(is_incognito) {
  DCHECK(file_message_loop);
}

FileSystemQuotaClient::~FileSystemQuotaClient() {
}

quota::QuotaClient::ID FileSystemQuotaClient::id() const {
  return quota::QuotaClient::kFileSystem;
}

void FileSystemQuotaClient::OnQuotaManagerDestroyed() {
  delete this;
}

void FileSystemQuotaClient::GetOriginUsage(
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

void FileSystemQuotaClient::GetOriginsForType(
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

void FileSystemQuotaClient::GetOriginsForHost(
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

void FileSystemQuotaClient::DeleteOriginData(const GURL& origin,
                                             StorageType type,
                                             DeletionCallback* callback) {
  FileSystemType fs_type = QuotaStorageTypeToFileSystemType(type);
  DCHECK(fs_type != kFileSystemTypeUnknown);
  scoped_refptr<DeleteOriginTask> task(
      new DeleteOriginTask(this, file_message_loop_,
                           origin, fs_type, callback));
  task->Start();
}

void FileSystemQuotaClient::DidGetOriginUsage(
    FileSystemType type, const GURL& origin_url, int64 usage) {
  TypeAndHostOrOrigin type_and_origin(std::make_pair(
      type, origin_url.spec()));
  DCHECK(pending_usage_callbacks_.HasCallbacks(type_and_origin));
  pending_usage_callbacks_.Run(type_and_origin, usage);
}

void FileSystemQuotaClient::DidGetOriginsForType(
    FileSystemType type, const std::set<GURL>& origins) {
  DCHECK(pending_origins_for_type_callbacks_.HasCallbacks(type));
  pending_origins_for_type_callbacks_.Run(type, origins);
}

void FileSystemQuotaClient::DidGetOriginsForHost(
    const TypeAndHostOrOrigin& type_and_host, const std::set<GURL>& origins) {
  DCHECK(pending_origins_for_host_callbacks_.HasCallbacks(type_and_host));
  pending_origins_for_host_callbacks_.Run(type_and_host, origins);
}

}  // namespace fileapi
