// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/sandbox_file_system_backend.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/task_runner_util.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/async_file_util_adapter.h"
#include "webkit/browser/fileapi/copy_or_move_file_validator.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_file_stream_reader.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_operation_impl.h"
#include "webkit/browser/fileapi/file_system_options.h"
#include "webkit/browser/fileapi/file_system_usage_cache.h"
#include "webkit/browser/fileapi/obfuscated_file_util.h"
#include "webkit/browser/fileapi/sandbox_context.h"
#include "webkit/browser/fileapi/sandbox_file_stream_writer.h"
#include "webkit/browser/fileapi/sandbox_quota_observer.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"

using quota::QuotaManagerProxy;
using quota::SpecialStoragePolicy;

namespace fileapi {

namespace {

const char kTemporaryOriginsCountLabel[] = "FileSystem.TemporaryOriginsCount";
const char kPersistentOriginsCountLabel[] = "FileSystem.PersistentOriginsCount";

}  // anonymous namespace

SandboxFileSystemBackend::SandboxFileSystemBackend(
    SandboxContext* sandbox_context)
    : sandbox_context_(sandbox_context),
      enable_temporary_file_system_in_incognito_(false) {
}

SandboxFileSystemBackend::~SandboxFileSystemBackend() {
}

bool SandboxFileSystemBackend::CanHandleType(FileSystemType type) const {
  return type == kFileSystemTypeTemporary ||
         type == kFileSystemTypePersistent;
}

void SandboxFileSystemBackend::Initialize(FileSystemContext* context) {
  // Set quota observers.
  update_observers_ = update_observers_.AddObserver(
      sandbox_context_->quota_observer(),
      sandbox_context_->file_task_runner());
  access_observers_ = access_observers_.AddObserver(
      sandbox_context_->quota_observer(), NULL);
}

void SandboxFileSystemBackend::OpenFileSystem(
    const GURL& origin_url,
    fileapi::FileSystemType type,
    OpenFileSystemMode mode,
    const OpenFileSystemCallback& callback) {
  DCHECK(CanHandleType(type));
  DCHECK(sandbox_context_);
  if (sandbox_context_->file_system_options().is_incognito() &&
      !(type == kFileSystemTypeTemporary &&
        enable_temporary_file_system_in_incognito_)) {
    // TODO(kinuko): return an isolated temporary directory.
    callback.Run(GURL(), std::string(), base::PLATFORM_FILE_ERROR_SECURITY);
    return;
  }

  sandbox_context_->OpenFileSystem(
      origin_url, type, mode, callback,
      GetFileSystemRootURI(origin_url, type));
}

FileSystemFileUtil* SandboxFileSystemBackend::GetFileUtil(
    FileSystemType type) {
  return sandbox_context_->sync_file_util();
}

AsyncFileUtil* SandboxFileSystemBackend::GetAsyncFileUtil(
    FileSystemType type) {
  DCHECK(sandbox_context_);
  return sandbox_context_->file_util();
}

CopyOrMoveFileValidatorFactory*
SandboxFileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    FileSystemType type,
    base::PlatformFileError* error_code) {
  DCHECK(error_code);
  *error_code = base::PLATFORM_FILE_OK;
  return NULL;
}

FileSystemOperation* SandboxFileSystemBackend::CreateFileSystemOperation(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::PlatformFileError* error_code) const {
  DCHECK(CanHandleType(url.type()));
  DCHECK(sandbox_context_);
  if (!sandbox_context_->IsAccessValid(url)) {
    *error_code = base::PLATFORM_FILE_ERROR_SECURITY;
    return NULL;
  }

  scoped_ptr<FileSystemOperationContext> operation_context(
      new FileSystemOperationContext(context));
  operation_context->set_update_observers(update_observers_);
  operation_context->set_change_observers(change_observers_);

  SpecialStoragePolicy* policy = sandbox_context_->special_storage_policy();
  if (policy && policy->IsStorageUnlimited(url.origin()))
    operation_context->set_quota_limit_type(quota::kQuotaLimitTypeUnlimited);
  else
    operation_context->set_quota_limit_type(quota::kQuotaLimitTypeLimited);

  return new FileSystemOperationImpl(url, context, operation_context.Pass());
}

scoped_ptr<webkit_blob::FileStreamReader>
SandboxFileSystemBackend::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const {
  DCHECK(CanHandleType(url.type()));
  DCHECK(sandbox_context_);
  if (!sandbox_context_->IsAccessValid(url))
    return scoped_ptr<webkit_blob::FileStreamReader>();
  return scoped_ptr<webkit_blob::FileStreamReader>(
      new FileSystemFileStreamReader(
          context, url, offset, expected_modification_time));
}

scoped_ptr<fileapi::FileStreamWriter>
SandboxFileSystemBackend::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context) const {
  DCHECK(CanHandleType(url.type()));
  DCHECK(sandbox_context_);
  if (!sandbox_context_->IsAccessValid(url))
    return scoped_ptr<fileapi::FileStreamWriter>();
  return scoped_ptr<fileapi::FileStreamWriter>(
      new SandboxFileStreamWriter(context, url, offset, update_observers_));
}

FileSystemQuotaUtil* SandboxFileSystemBackend::GetQuotaUtil() {
  return this;
}

SandboxContext::OriginEnumerator*
SandboxFileSystemBackend::CreateOriginEnumerator() {
  DCHECK(sandbox_context_);
  return sandbox_context_->CreateOriginEnumerator();
}

base::PlatformFileError
SandboxFileSystemBackend::DeleteOriginDataOnFileThread(
    FileSystemContext* file_system_context,
    QuotaManagerProxy* proxy,
    const GURL& origin_url,
    fileapi::FileSystemType type) {
  DCHECK(CanHandleType(type));
  DCHECK(sandbox_context_);
  return sandbox_context_->DeleteOriginDataOnFileThread(
      file_system_context, proxy, origin_url, type);
}

void SandboxFileSystemBackend::GetOriginsForTypeOnFileThread(
    fileapi::FileSystemType type, std::set<GURL>* origins) {
  DCHECK(CanHandleType(type));
  DCHECK(sandbox_context_);
  sandbox_context_->GetOriginsForTypeOnFileThread(type, origins);
  switch (type) {
    case kFileSystemTypeTemporary:
      UMA_HISTOGRAM_COUNTS(kTemporaryOriginsCountLabel, origins->size());
      break;
    case kFileSystemTypePersistent:
      UMA_HISTOGRAM_COUNTS(kPersistentOriginsCountLabel, origins->size());
      break;
    default:
      break;
  }
}

void SandboxFileSystemBackend::GetOriginsForHostOnFileThread(
    fileapi::FileSystemType type, const std::string& host,
    std::set<GURL>* origins) {
  DCHECK(CanHandleType(type));
  DCHECK(sandbox_context_);
  sandbox_context_->GetOriginsForHostOnFileThread(type, host, origins);
}

int64 SandboxFileSystemBackend::GetOriginUsageOnFileThread(
    FileSystemContext* file_system_context,
    const GURL& origin_url,
    fileapi::FileSystemType type) {
  DCHECK(CanHandleType(type));
  DCHECK(sandbox_context_);
  return sandbox_context_->GetOriginUsageOnFileThread(
      file_system_context, origin_url, type);
}

void SandboxFileSystemBackend::InvalidateUsageCache(
    const GURL& origin,
    fileapi::FileSystemType type) {
  DCHECK(CanHandleType(type));
  DCHECK(sandbox_context_);
  sandbox_context_->InvalidateUsageCache(origin, type);
}

void SandboxFileSystemBackend::StickyInvalidateUsageCache(
    const GURL& origin,
    fileapi::FileSystemType type) {
  DCHECK(CanHandleType(type));
  DCHECK(sandbox_context_);
  sandbox_context_->StickyInvalidateUsageCache(origin, type);
}

void SandboxFileSystemBackend::AddFileUpdateObserver(
    FileSystemType type,
    FileUpdateObserver* observer,
    base::SequencedTaskRunner* task_runner) {
  DCHECK(CanHandleType(type));
  UpdateObserverList* list = &update_observers_;
  *list = list->AddObserver(observer, task_runner);
}

void SandboxFileSystemBackend::AddFileChangeObserver(
    FileSystemType type,
    FileChangeObserver* observer,
    base::SequencedTaskRunner* task_runner) {
  DCHECK(CanHandleType(type));
  ChangeObserverList* list = &change_observers_;
  *list = list->AddObserver(observer, task_runner);
}

void SandboxFileSystemBackend::AddFileAccessObserver(
    FileSystemType type,
    FileAccessObserver* observer,
    base::SequencedTaskRunner* task_runner) {
  DCHECK(CanHandleType(type));
  access_observers_ = access_observers_.AddObserver(observer, task_runner);
}

const UpdateObserverList* SandboxFileSystemBackend::GetUpdateObservers(
    FileSystemType type) const {
  DCHECK(CanHandleType(type));
  return &update_observers_;
}

const ChangeObserverList* SandboxFileSystemBackend::GetChangeObservers(
    FileSystemType type) const {
  DCHECK(CanHandleType(type));
  return &change_observers_;
}

const AccessObserverList* SandboxFileSystemBackend::GetAccessObservers(
    FileSystemType type) const {
  DCHECK(CanHandleType(type));
  return &access_observers_;
}

}  // namespace fileapi
