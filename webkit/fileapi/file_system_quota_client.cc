// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_quota_client.h"

#include <algorithm>
#include <set>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_quota_util.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"

using quota::StorageType;

namespace fileapi {

namespace {

void GetOriginsForTypeOnFileThread(
    FileSystemContext* context,
    StorageType storage_type,
    std::set<GURL>* origins_ptr) {
  FileSystemType type = QuotaStorageTypeToFileSystemType(storage_type);
  DCHECK(type != kFileSystemTypeUnknown);

  FileSystemQuotaUtil* quota_util = context->GetQuotaUtil(type);
  if (!quota_util)
    return;
  quota_util->GetOriginsForTypeOnFileThread(type, origins_ptr);
}

void GetOriginsForHostOnFileThread(
    FileSystemContext* context,
    StorageType storage_type,
    const std::string& host,
    std::set<GURL>* origins_ptr) {
  FileSystemType type = QuotaStorageTypeToFileSystemType(storage_type);
  DCHECK(type != kFileSystemTypeUnknown);

  FileSystemQuotaUtil* quota_util = context->GetQuotaUtil(type);
  if (!quota_util)
    return;
  quota_util->GetOriginsForHostOnFileThread(type, host, origins_ptr);
}

void DidGetOrigins(
    const quota::QuotaClient::GetOriginsCallback& callback,
    std::set<GURL>* origins_ptr,
    StorageType storage_type) {
  callback.Run(*origins_ptr, storage_type);
}

quota::QuotaStatusCode DeleteOriginOnFileThread(
    FileSystemContext* context,
    const GURL& origin,
    FileSystemType type) {
  base::PlatformFileError result =
      context->sandbox_provider()->DeleteOriginDataOnFileThread(
          context, context->quota_manager_proxy(), origin, type);
  if (result == base::PLATFORM_FILE_OK)
    return quota::kQuotaStatusOk;
  return quota::kQuotaErrorInvalidModification;
}

}  // namespace

FileSystemQuotaClient::FileSystemQuotaClient(
    FileSystemContext* file_system_context,
    bool is_incognito)
    : file_system_context_(file_system_context),
      is_incognito_(is_incognito) {
}

FileSystemQuotaClient::~FileSystemQuotaClient() {}

quota::QuotaClient::ID FileSystemQuotaClient::id() const {
  return quota::QuotaClient::kFileSystem;
}

void FileSystemQuotaClient::OnQuotaManagerDestroyed() {
  delete this;
}

void FileSystemQuotaClient::GetOriginUsage(
    const GURL& origin_url,
    StorageType storage_type,
    const GetUsageCallback& callback) {
  DCHECK(!callback.is_null());

  if (is_incognito_) {
    // We don't support FileSystem in incognito mode yet.
    callback.Run(0);
    return;
  }

  FileSystemType type = QuotaStorageTypeToFileSystemType(storage_type);
  DCHECK(type != kFileSystemTypeUnknown);

  FileSystemQuotaUtil* quota_util = file_system_context_->GetQuotaUtil(type);
  if (!quota_util) {
    callback.Run(0);
    return;
  }

  base::PostTaskAndReplyWithResult(
      file_task_runner(),
      FROM_HERE,
      // It is safe to pass Unretained(quota_util) since context owns it.
      base::Bind(&FileSystemQuotaUtil::GetOriginUsageOnFileThread,
                 base::Unretained(quota_util),
                 file_system_context_,
                 origin_url,
                 type),
      callback);
}

void FileSystemQuotaClient::GetOriginsForType(
    StorageType storage_type,
    const GetOriginsCallback& callback) {
  DCHECK(!callback.is_null());

  if (is_incognito_) {
    // We don't support FileSystem in incognito mode yet.
    std::set<GURL> origins;
    callback.Run(origins, storage_type);
    return;
  }

  std::set<GURL>* origins_ptr = new std::set<GURL>();
  file_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GetOriginsForTypeOnFileThread,
                 file_system_context_,
                 storage_type,
                 base::Unretained(origins_ptr)),
      base::Bind(&DidGetOrigins,
                 callback,
                 base::Owned(origins_ptr),
                 storage_type));
}

void FileSystemQuotaClient::GetOriginsForHost(
    StorageType storage_type,
    const std::string& host,
    const GetOriginsCallback& callback) {
  DCHECK(!callback.is_null());

  if (is_incognito_) {
    // We don't support FileSystem in incognito mode yet.
    std::set<GURL> origins;
    callback.Run(origins, storage_type);
    return;
  }

  std::set<GURL>* origins_ptr = new std::set<GURL>();
  file_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GetOriginsForHostOnFileThread,
                 file_system_context_,
                 storage_type,
                 host,
                 base::Unretained(origins_ptr)),
      base::Bind(&DidGetOrigins,
                 callback,
                 base::Owned(origins_ptr),
                 storage_type));
}

void FileSystemQuotaClient::DeleteOriginData(
    const GURL& origin,
    StorageType type,
    const DeletionCallback& callback) {
  FileSystemType fs_type = QuotaStorageTypeToFileSystemType(type);
  DCHECK(fs_type != kFileSystemTypeUnknown);

  base::PostTaskAndReplyWithResult(
      file_task_runner(),
      FROM_HERE,
      base::Bind(&DeleteOriginOnFileThread,
                 file_system_context_,
                 origin,
                 fs_type),
      callback);
}

base::SequencedTaskRunner* FileSystemQuotaClient::file_task_runner() const {
  return file_system_context_->task_runners()->file_task_runner();
}

}  // namespace fileapi
