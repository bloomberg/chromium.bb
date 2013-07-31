// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/sandbox_file_system_backend.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "net/base/net_util.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/async_file_util_adapter.h"
#include "webkit/browser/fileapi/copy_or_move_file_validator.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_file_stream_reader.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_options.h"
#include "webkit/browser/fileapi/file_system_task_runners.h"
#include "webkit/browser/fileapi/file_system_usage_cache.h"
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

// Restricted names.
// http://dev.w3.org/2009/dap/file-system/file-dir-sys.html#naming-restrictions
const base::FilePath::CharType* const kRestrictedNames[] = {
  FILE_PATH_LITERAL("."), FILE_PATH_LITERAL(".."),
};

// Restricted chars.
const base::FilePath::CharType kRestrictedChars[] = {
  FILE_PATH_LITERAL('/'), FILE_PATH_LITERAL('\\'),
};

class ObfuscatedOriginEnumerator
    : public SandboxFileSystemBackend::OriginEnumerator {
 public:
  explicit ObfuscatedOriginEnumerator(ObfuscatedFileUtil* file_util) {
    enum_.reset(file_util->CreateOriginEnumerator());
  }
  virtual ~ObfuscatedOriginEnumerator() {}

  virtual GURL Next() OVERRIDE {
    return enum_->Next();
  }

  virtual bool HasFileSystemType(fileapi::FileSystemType type) const OVERRIDE {
    return enum_->HasFileSystemType(type);
  }

 private:
  scoped_ptr<ObfuscatedFileUtil::AbstractOriginEnumerator> enum_;
};

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
    SandboxContext* sandbox_context,
    const FileSystemOptions& file_system_options)
    : file_system_options_(file_system_options),
      sandbox_context_(sandbox_context),
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
  if (sandbox_context_->is_usage_tracking_enabled()) {
    update_observers_ = update_observers_.AddObserver(
        sandbox_context_->quota_observer(),
        sandbox_context_->file_task_runner());
    access_observers_ = access_observers_.AddObserver(
        sandbox_context_->quota_observer(), NULL);
  }

  syncable_update_observers_ = update_observers_;

  if (!sandbox_context_->file_task_runner()->RunsTasksOnCurrentThread()) {
    // Post prepopulate task only if it's not already running on
    // file_task_runner (which implies running in tests).
    sandbox_context_->file_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ObfuscatedFileUtil::MaybePrepopulateDatabase,
                  base::Unretained(sandbox_sync_file_util())));
  }
}

void SandboxFileSystemBackend::OpenFileSystem(
    const GURL& origin_url,
    fileapi::FileSystemType type,
    OpenFileSystemMode mode,
    const OpenFileSystemCallback& callback) {
  if (file_system_options_.is_incognito() &&
      !(type == kFileSystemTypeTemporary &&
        enable_temporary_file_system_in_incognito_)) {
    // TODO(kinuko): return an isolated temporary directory.
    callback.Run(GURL(), std::string(), base::PLATFORM_FILE_ERROR_SECURITY);
    return;
  }

  if (!IsAllowedScheme(origin_url)) {
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
                 sandbox_sync_file_util(),
                 origin_url, type, mode,
                 base::Unretained(error_ptr)),
      base::Bind(&DidOpenFileSystem,
                 weak_factory_.GetWeakPtr(),
                 base::Bind(callback, root_url, name),
                 base::Owned(error_ptr)));

  if (sandbox_context_->is_usage_tracking_enabled())
    return;

  // Schedule full usage recalculation on the next launch without
  // --disable-file-system-usage-tracking.
  sandbox_context_->file_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SandboxFileSystemBackend::InvalidateUsageCacheOnFileThread,
                 sandbox_sync_file_util(), origin_url, type, usage_cache()));
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
  if (!IsAccessValid(url)) {
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
  if (!IsAccessValid(url))
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
  if (!IsAccessValid(url))
    return scoped_ptr<fileapi::FileStreamWriter>();
  return scoped_ptr<fileapi::FileStreamWriter>(
      new SandboxFileStreamWriter(context, url, offset, update_observers_));
}

FileSystemQuotaUtil* SandboxFileSystemBackend::GetQuotaUtil() {
  return this;
}

SandboxFileSystemBackend::OriginEnumerator*
SandboxFileSystemBackend::CreateOriginEnumerator() {
  return new ObfuscatedOriginEnumerator(sandbox_sync_file_util());
}

base::FilePath SandboxFileSystemBackend::GetBaseDirectoryForOriginAndType(
    const GURL& origin_url, fileapi::FileSystemType type, bool create) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath path = sandbox_sync_file_util()->GetDirectoryForOriginAndType(
      origin_url, type, create, &error);
  if (error != base::PLATFORM_FILE_OK)
    return base::FilePath();
  return path;
}

base::PlatformFileError
SandboxFileSystemBackend::DeleteOriginDataOnFileThread(
    FileSystemContext* file_system_context,
    QuotaManagerProxy* proxy,
    const GURL& origin_url,
    fileapi::FileSystemType type) {
  int64 usage = GetOriginUsageOnFileThread(file_system_context,
                                           origin_url, type);

  usage_cache()->CloseCacheFiles();
  bool result = sandbox_sync_file_util()->DeleteDirectoryForOriginAndType(
          origin_url, type);
  if (result && proxy) {
    proxy->NotifyStorageModified(
        quota::QuotaClient::kFileSystem,
        origin_url,
        FileSystemTypeToQuotaStorageType(type),
        -usage);
  }

  if (result)
    return base::PLATFORM_FILE_OK;
  return base::PLATFORM_FILE_ERROR_FAILED;
}

void SandboxFileSystemBackend::GetOriginsForTypeOnFileThread(
    fileapi::FileSystemType type, std::set<GURL>* origins) {
  DCHECK(CanHandleType(type));
  DCHECK(origins);
  scoped_ptr<OriginEnumerator> enumerator(CreateOriginEnumerator());
  GURL origin;
  while (!(origin = enumerator->Next()).is_empty()) {
    if (enumerator->HasFileSystemType(type))
      origins->insert(origin);
  }
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
  DCHECK(origins);
  scoped_ptr<OriginEnumerator> enumerator(CreateOriginEnumerator());
  GURL origin;
  while (!(origin = enumerator->Next()).is_empty()) {
    if (host == net::GetHostOrSpecFromURL(origin) &&
        enumerator->HasFileSystemType(type))
      origins->insert(origin);
  }
}

int64 SandboxFileSystemBackend::GetOriginUsageOnFileThread(
    FileSystemContext* file_system_context,
    const GURL& origin_url,
    fileapi::FileSystemType type) {
  DCHECK(CanHandleType(type));
  if (!sandbox_context_->is_usage_tracking_enabled())
    return 0;

  // Don't use usage cache and return recalculated usage for sticky invalidated
  // origins.
  if (ContainsKey(sticky_dirty_origins_, std::make_pair(origin_url, type)))
    return RecalculateUsage(file_system_context, origin_url, type);

  base::FilePath base_path =
      GetBaseDirectoryForOriginAndType(origin_url, type, false);
  if (base_path.empty() || !base::DirectoryExists(base_path))
    return 0;
  base::FilePath usage_file_path =
      base_path.Append(FileSystemUsageCache::kUsageFileName);

  bool is_valid = usage_cache()->IsValid(usage_file_path);
  uint32 dirty_status = 0;
  bool dirty_status_available =
      usage_cache()->GetDirty(usage_file_path, &dirty_status);
  bool visited = !visited_origins_.insert(origin_url).second;
  if (is_valid && (dirty_status == 0 || (dirty_status_available && visited))) {
    // The usage cache is clean (dirty == 0) or the origin is already
    // initialized and running.  Read the cache file to get the usage.
    int64 usage = 0;
    return usage_cache()->GetUsage(usage_file_path, &usage) ? usage : -1;
  }
  // The usage cache has not been initialized or the cache is dirty.
  // Get the directory size now and update the cache.
  usage_cache()->Delete(usage_file_path);

  int64 usage = RecalculateUsage(file_system_context, origin_url, type);

  // This clears the dirty flag too.
  usage_cache()->UpdateUsage(usage_file_path, usage);
  return usage;
}

void SandboxFileSystemBackend::InvalidateUsageCache(
    const GURL& origin,
    fileapi::FileSystemType type) {
  DCHECK(CanHandleType(type));
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath usage_file_path = GetUsageCachePathForOriginAndType(
      sandbox_sync_file_util(), origin, type, &error);
  if (error != base::PLATFORM_FILE_OK)
    return;
  usage_cache()->IncrementDirty(usage_file_path);
}

void SandboxFileSystemBackend::StickyInvalidateUsageCache(
    const GURL& origin,
    fileapi::FileSystemType type) {
  DCHECK(CanHandleType(type));
  sticky_dirty_origins_.insert(std::make_pair(origin, type));
  sandbox_context_->quota_observer()->SetUsageCacheEnabled(origin, type, false);
  InvalidateUsageCache(origin, type);
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

bool SandboxFileSystemBackend::IsAccessValid(
    const FileSystemURL& url) const {
  if (!IsAllowedScheme(url.origin()))
    return false;

  if (!CanHandleType(url.type()))
    return false;

  if (url.path().ReferencesParent())
    return false;

  // Return earlier if the path is '/', because VirtualPath::BaseName()
  // returns '/' for '/' and we fail the "basename != '/'" check below.
  // (We exclude '.' because it's disallowed by spec.)
  if (VirtualPath::IsRootPath(url.path()) &&
      url.path() != base::FilePath(base::FilePath::kCurrentDirectory))
    return true;

  // Restricted names specified in
  // http://dev.w3.org/2009/dap/file-system/file-dir-sys.html#naming-restrictions
  base::FilePath filename = VirtualPath::BaseName(url.path());
  // See if the name is allowed to create.
  for (size_t i = 0; i < arraysize(kRestrictedNames); ++i) {
    if (filename.value() == kRestrictedNames[i])
      return false;
  }
  for (size_t i = 0; i < arraysize(kRestrictedChars); ++i) {
    if (filename.value().find(kRestrictedChars[i]) !=
        base::FilePath::StringType::npos)
      return false;
  }

  return true;
}

base::FilePath SandboxFileSystemBackend::GetUsageCachePathForOriginAndType(
    const GURL& origin_url,
    FileSystemType type) {
  base::PlatformFileError error;
  base::FilePath path = GetUsageCachePathForOriginAndType(
      sandbox_sync_file_util(), origin_url, type, &error);
  if (error != base::PLATFORM_FILE_OK)
    return base::FilePath();
  return path;
}

// static
base::FilePath SandboxFileSystemBackend::GetUsageCachePathForOriginAndType(
    ObfuscatedFileUtil* sandbox_file_util,
    const GURL& origin_url,
    fileapi::FileSystemType type,
    base::PlatformFileError* error_out) {
  DCHECK(error_out);
  *error_out = base::PLATFORM_FILE_OK;
  base::FilePath base_path = sandbox_file_util->GetDirectoryForOriginAndType(
      origin_url, type, false /* create */, error_out);
  if (*error_out != base::PLATFORM_FILE_OK)
    return base::FilePath();
  return base_path.Append(FileSystemUsageCache::kUsageFileName);
}

bool SandboxFileSystemBackend::IsAllowedScheme(const GURL& url) const {
  // Basically we only accept http or https. We allow file:// URLs
  // only if --allow-file-access-from-files flag is given.
  if (url.SchemeIs("http") || url.SchemeIs("https"))
    return true;
  if (url.SchemeIsFileSystem())
    return url.inner_url() && IsAllowedScheme(*url.inner_url());

  for (size_t i = 0;
       i < file_system_options_.additional_allowed_schemes().size();
       ++i) {
    if (url.SchemeIs(
            file_system_options_.additional_allowed_schemes()[i].c_str()))
      return true;
  }
  return false;
}

ObfuscatedFileUtil* SandboxFileSystemBackend::sandbox_sync_file_util() {
  DCHECK(sandbox_context_);
  return sandbox_context_->sync_file_util();
}

FileSystemUsageCache* SandboxFileSystemBackend::usage_cache() {
  DCHECK(sandbox_context_);
  return sandbox_context_->usage_cache();
}

// static
void SandboxFileSystemBackend::InvalidateUsageCacheOnFileThread(
    ObfuscatedFileUtil* file_util,
    const GURL& origin,
    FileSystemType type,
    FileSystemUsageCache* usage_cache) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath usage_cache_path = GetUsageCachePathForOriginAndType(
      file_util, origin, type, &error);
  if (error == base::PLATFORM_FILE_OK)
    usage_cache->IncrementDirty(usage_cache_path);
}

int64 SandboxFileSystemBackend::RecalculateUsage(FileSystemContext* context,
                                                  const GURL& origin,
                                                  FileSystemType type) {
  FileSystemOperationContext operation_context(context);
  FileSystemURL url = context->CreateCrackedFileSystemURL(
      origin, type, base::FilePath());
  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> enumerator(
      sandbox_sync_file_util()->CreateFileEnumerator(
          &operation_context, url, true));

  base::FilePath file_path_each;
  int64 usage = 0;

  while (!(file_path_each = enumerator->Next()).empty()) {
    usage += enumerator->Size();
    usage += ObfuscatedFileUtil::ComputeFilePathCost(file_path_each);
  }

  return usage;
}

}  // namespace fileapi
