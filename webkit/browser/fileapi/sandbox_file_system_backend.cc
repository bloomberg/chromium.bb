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
#include "webkit/browser/fileapi/file_system_options.h"
#include "webkit/browser/fileapi/file_system_task_runners.h"
#include "webkit/browser/fileapi/local_file_system_operation.h"
#include "webkit/browser/fileapi/obfuscated_file_util.h"
#include "webkit/browser/fileapi/sandbox_context.h"
#include "webkit/browser/fileapi/sandbox_file_stream_writer.h"
#include "webkit/browser/fileapi/sandbox_quota_observer.h"
#include "webkit/browser/fileapi/syncable/syncable_file_system_operation.h"
#include "webkit/browser/fileapi/syncable/syncable_file_system_util.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"

using quota::QuotaManagerProxy;
using quota::SpecialStoragePolicy;

namespace fileapi {

namespace {

const char kOpenFileSystemLabel[] = "FileSystem.OpenFileSystem";
const char kOpenFileSystemDetailLabel[] = "FileSystem.OpenFileSystemDetail";
const char kOpenFileSystemDetailNonThrottledLabel[] =
    "FileSystem.OpenFileSystemDetailNonthrottled";
int64 kMinimumStatsCollectionIntervalHours = 1;

enum FileSystemError {
  kOK = 0,
  kIncognito,
  kInvalidSchemeError,
  kCreateDirectoryError,
  kNotFound,
  kUnknownError,
  kFileSystemErrorMax,
};

const char kTemporaryOriginsCountLabel[] = "FileSystem.TemporaryOriginsCount";
const char kPersistentOriginsCountLabel[] = "FileSystem.PersistentOriginsCount";

void DidOpenFileSystem(
    base::WeakPtr<SandboxFileSystemBackend> sandbox_backend,
    const base::Callback<void(base::PlatformFileError error)>& callback,
    base::PlatformFileError* error) {
  if (sandbox_backend.get())
    sandbox_backend.get()->CollectOpenFileSystemMetrics(*error);
  callback.Run(*error);
}

void OpenFileSystemOnFileThread(
    ObfuscatedFileUtil* file_util,
    const GURL& origin_url,
    FileSystemType type,
    OpenFileSystemMode mode,
    base::PlatformFileError* error_ptr) {
  DCHECK(error_ptr);
  const bool create = (mode == OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT);
  file_util->GetDirectoryForOriginAndType(origin_url, type, create, error_ptr);
  if (*error_ptr != base::PLATFORM_FILE_OK) {
    UMA_HISTOGRAM_ENUMERATION(kOpenFileSystemLabel,
                              kCreateDirectoryError,
                              kFileSystemErrorMax);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kOpenFileSystemLabel, kOK, kFileSystemErrorMax);
  }
  // The reference of file_util will be derefed on the FILE thread
  // when the storage of this callback gets deleted regardless of whether
  // this method is called or not.
}

}  // anonymous namespace

SandboxFileSystemBackend::SandboxFileSystemBackend(
    SandboxContext* sandbox_context)
    : sandbox_context_(sandbox_context),
      enable_temporary_file_system_in_incognito_(false),
      weak_factory_(this) {
}

SandboxFileSystemBackend::~SandboxFileSystemBackend() {
}

bool SandboxFileSystemBackend::CanHandleType(FileSystemType type) const {
  return type == kFileSystemTypeTemporary ||
         type == kFileSystemTypePersistent ||
         type == kFileSystemTypeSyncable ||
         type == kFileSystemTypeSyncableForInternalSync;
}

void SandboxFileSystemBackend::Initialize(FileSystemContext* context) {
  // Set quota observers.
  update_observers_ = update_observers_.AddObserver(
      sandbox_context_->quota_observer(),
      sandbox_context_->file_task_runner());
  access_observers_ = access_observers_.AddObserver(
      sandbox_context_->quota_observer(), NULL);

  syncable_update_observers_ = update_observers_;

  if (!sandbox_context_->file_task_runner()->RunsTasksOnCurrentThread()) {
    // Post prepopulate task only if it's not already running on
    // file_task_runner (which implies running in tests).
    sandbox_context_->file_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ObfuscatedFileUtil::MaybePrepopulateDatabase,
                  base::Unretained(sandbox_context_->sync_file_util())));
  }
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

  if (!sandbox_context_->IsAllowedScheme(origin_url)) {
    callback.Run(GURL(), std::string(), base::PLATFORM_FILE_ERROR_SECURITY);
    return;
  }

  // TODO(nhiroki): Factor out SyncFS related code to SyncFileSystemBackend we
  // plan to introduce. (http://crbug.com/242422/)
  GURL root_url = (type == kFileSystemTypeSyncable)
      ? sync_file_system::GetSyncableFileSystemRootURI(origin_url)
      : GetFileSystemRootURI(origin_url, type);
  std::string name = GetFileSystemName(origin_url, type);

  base::PlatformFileError* error_ptr = new base::PlatformFileError;
  sandbox_context_->file_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&OpenFileSystemOnFileThread,
                 sandbox_context_->sync_file_util(),
                 origin_url, type, mode,
                 base::Unretained(error_ptr)),
      base::Bind(&DidOpenFileSystem,
                 weak_factory_.GetWeakPtr(),
                 base::Bind(callback, root_url, name),
                 base::Owned(error_ptr)));
};

FileSystemFileUtil* SandboxFileSystemBackend::GetFileUtil(
    FileSystemType type) {
  DCHECK(sandbox_context_);
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

  // Copy the observer lists (assuming we only have small number of observers).
  if (url.type() == kFileSystemTypeSyncable) {
    operation_context->set_update_observers(syncable_update_observers_);
    operation_context->set_change_observers(syncable_change_observers_);
    return new sync_file_system::SyncableFileSystemOperation(
        url, context, operation_context.Pass());
  }

  // For regular sandboxed types.
  operation_context->set_update_observers(update_observers_);
  operation_context->set_change_observers(change_observers_);

  SpecialStoragePolicy* policy = sandbox_context_->special_storage_policy();
  if (policy && policy->IsStorageUnlimited(url.origin()))
    operation_context->set_quota_limit_type(quota::kQuotaLimitTypeUnlimited);
  else
    operation_context->set_quota_limit_type(quota::kQuotaLimitTypeLimited);

  return new LocalFileSystemOperation(url, context, operation_context.Pass());
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
  if (type == kFileSystemTypeSyncable)
    list = &syncable_update_observers_;
  *list = list->AddObserver(observer, task_runner);
}

void SandboxFileSystemBackend::AddFileChangeObserver(
    FileSystemType type,
    FileChangeObserver* observer,
    base::SequencedTaskRunner* task_runner) {
  DCHECK(CanHandleType(type));
  ChangeObserverList* list = &change_observers_;
  if (type == kFileSystemTypeSyncable)
    list = &syncable_change_observers_;
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
  if (type == kFileSystemTypeSyncable)
    return &syncable_update_observers_;
  return &update_observers_;
}

const ChangeObserverList* SandboxFileSystemBackend::GetChangeObservers(
    FileSystemType type) const {
  DCHECK(CanHandleType(type));
  if (type == kFileSystemTypeSyncable)
    return &syncable_change_observers_;
  return &change_observers_;
}

const AccessObserverList* SandboxFileSystemBackend::GetAccessObservers(
    FileSystemType type) const {
  DCHECK(CanHandleType(type));
  return &access_observers_;
}

void SandboxFileSystemBackend::CollectOpenFileSystemMetrics(
    base::PlatformFileError error_code) {
  base::Time now = base::Time::Now();
  bool throttled = now < next_release_time_for_open_filesystem_stat_;
  if (!throttled) {
    next_release_time_for_open_filesystem_stat_ =
        now + base::TimeDelta::FromHours(kMinimumStatsCollectionIntervalHours);
  }

#define REPORT(report_value)                                            \
  UMA_HISTOGRAM_ENUMERATION(kOpenFileSystemDetailLabel,                 \
                            (report_value),                             \
                            kFileSystemErrorMax);                       \
  if (!throttled) {                                                     \
    UMA_HISTOGRAM_ENUMERATION(kOpenFileSystemDetailNonThrottledLabel,   \
                              (report_value),                           \
                              kFileSystemErrorMax);                     \
  }

  switch (error_code) {
    case base::PLATFORM_FILE_OK:
      REPORT(kOK);
      break;
    case base::PLATFORM_FILE_ERROR_INVALID_URL:
      REPORT(kInvalidSchemeError);
      break;
    case base::PLATFORM_FILE_ERROR_NOT_FOUND:
      REPORT(kNotFound);
      break;
    case base::PLATFORM_FILE_ERROR_FAILED:
    default:
      REPORT(kUnknownError);
      break;
  }
#undef REPORT
}

}  // namespace fileapi
