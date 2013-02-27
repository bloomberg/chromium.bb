// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/sandbox_mount_point_provider.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/task_runner_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "webkit/fileapi/async_file_util_adapter.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_stream_reader.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_options.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/obfuscated_file_util.h"
#include "webkit/fileapi/sandbox_file_stream_writer.h"
#include "webkit/fileapi/sandbox_quota_observer.h"
#include "webkit/fileapi/syncable/syncable_file_system_operation.h"
#include "webkit/quota/quota_manager.h"

using quota::QuotaManagerProxy;

namespace fileapi {

namespace {

const char kChromeScheme[] = "chrome";
const char kExtensionScheme[] = "chrome-extension";

const char kOpenFileSystemLabel[] = "FileSystem.OpenFileSystem";
const char kOpenFileSystemDetailLabel[] = "FileSystem.OpenFileSystemDetail";
const char kOpenFileSystemDetailNonThrottledLabel[] =
    "FileSystem.OpenFileSystemDetailNonthrottled";
int64 kMinimumStatsCollectionIntervalHours = 1;

// A command switch to enable syncing directory operations in Sync FileSystem
// API. (http://crbug.com/161442)
// TODO(kinuko): this command-line switch should be temporary.
const char kEnableSyncDirectoryOperation[]  = "enable-sync-directory-operation";

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
    : public SandboxMountPointProvider::OriginEnumerator {
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

void DidValidateFileSystemRoot(
    base::WeakPtr<SandboxMountPointProvider> mount_point_provider,
    const FileSystemMountPointProvider::ValidateFileSystemCallback& callback,
    base::PlatformFileError* error) {
  if (mount_point_provider.get())
    mount_point_provider.get()->CollectOpenFileSystemMetrics(*error);
  callback.Run(*error);
}

void ValidateRootOnFileThread(
    ObfuscatedFileUtil* file_util,
    const GURL& origin_url,
    FileSystemType type,
    bool create,
    base::PlatformFileError* error_ptr) {
  DCHECK(error_ptr);

  base::FilePath root_path =
      file_util->GetDirectoryForOriginAndType(
          origin_url, type, create, error_ptr);
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

const base::FilePath::CharType SandboxMountPointProvider::kFileSystemDirectory[] =
    FILE_PATH_LITERAL("File System");

// static
bool SandboxMountPointProvider::CanHandleType(FileSystemType type) {
  return type == kFileSystemTypeTemporary ||
         type == kFileSystemTypePersistent ||
         type == kFileSystemTypeSyncable;
}

SandboxMountPointProvider::SandboxMountPointProvider(
    quota::QuotaManagerProxy* quota_manager_proxy,
    base::SequencedTaskRunner* file_task_runner,
    const base::FilePath& profile_path,
    const FileSystemOptions& file_system_options)
    : file_task_runner_(file_task_runner),
      profile_path_(profile_path),
      file_system_options_(file_system_options),
      sandbox_file_util_(
          new AsyncFileUtilAdapter(
              new ObfuscatedFileUtil(
                  profile_path.Append(kFileSystemDirectory)))),
      quota_observer_(new SandboxQuotaObserver(
                      quota_manager_proxy,
                      file_task_runner,
                      sandbox_sync_file_util())),
      enable_sync_directory_operation_(
          CommandLine::ForCurrentProcess()->HasSwitch(
              kEnableSyncDirectoryOperation)),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  // Set quota observers.
  UpdateObserverList::Source update_observers_src;
  AccessObserverList::Source access_observers_src;

  update_observers_src.AddObserver(quota_observer_.get(), file_task_runner_);
  access_observers_src.AddObserver(quota_observer_.get(), NULL);

  update_observers_ = UpdateObserverList(update_observers_src);
  access_observers_ = AccessObserverList(access_observers_src);
  syncable_update_observers_ = UpdateObserverList(update_observers_src);
}

SandboxMountPointProvider::~SandboxMountPointProvider() {
  if (!file_task_runner_->RunsTasksOnCurrentThread()) {
    AsyncFileUtilAdapter* sandbox_file_util = sandbox_file_util_.release();
    SandboxQuotaObserver* quota_observer = quota_observer_.release();
    if (!file_task_runner_->DeleteSoon(FROM_HERE, sandbox_file_util))
      delete sandbox_file_util;
    if (!file_task_runner_->DeleteSoon(FROM_HERE, quota_observer))
      delete quota_observer;
  }
}

void SandboxMountPointProvider::ValidateFileSystemRoot(
    const GURL& origin_url, fileapi::FileSystemType type, bool create,
    const ValidateFileSystemCallback& callback) {
  if (file_system_options_.is_incognito()) {
    // TODO(kinuko): return an isolated temporary directory.
    callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
    UMA_HISTOGRAM_ENUMERATION(kOpenFileSystemLabel,
                              kIncognito,
                              kFileSystemErrorMax);
    return;
  }

  if (!IsAllowedScheme(origin_url)) {
    callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
    UMA_HISTOGRAM_ENUMERATION(kOpenFileSystemLabel,
                              kInvalidSchemeError,
                              kFileSystemErrorMax);
    return;
  }

  base::PlatformFileError* error_ptr = new base::PlatformFileError;
  file_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&ValidateRootOnFileThread,
                 sandbox_sync_file_util(),
                 origin_url, type, create,
                 base::Unretained(error_ptr)),
      base::Bind(&DidValidateFileSystemRoot,
                 weak_factory_.GetWeakPtr(),
                 callback, base::Owned(error_ptr)));
};

base::FilePath
SandboxMountPointProvider::GetFileSystemRootPathOnFileThread(
    const FileSystemURL& url,
    bool create) {
  if (file_system_options_.is_incognito())
    // TODO(kinuko): return an isolated temporary directory.
    return base::FilePath();

  if (!IsAllowedScheme(url.origin()))
    return base::FilePath();

  return GetBaseDirectoryForOriginAndType(url.origin(), url.type(), create);
}

FileSystemFileUtil* SandboxMountPointProvider::GetFileUtil(
    FileSystemType type) {
  DCHECK(sandbox_file_util_.get());
  return sandbox_file_util_->sync_file_util();
}

AsyncFileUtil* SandboxMountPointProvider::GetAsyncFileUtil(
    FileSystemType type) {
  return sandbox_file_util_.get();
}

FilePermissionPolicy SandboxMountPointProvider::GetPermissionPolicy(
    const FileSystemURL& url, int permissions) const {
  if (!CanHandleType(url.type()) || !IsAllowedScheme(url.origin()))
    return FILE_PERMISSION_ALWAYS_DENY;

  if (url.path().ReferencesParent())
    return FILE_PERMISSION_ALWAYS_DENY;

  // Any write access is disallowed on the root path.
  if ((url.path().empty() || VirtualPath::DirName(url.path()) == url.path())
      && (permissions & ~kReadFilePermissions))
    return FILE_PERMISSION_ALWAYS_DENY;

  if ((permissions & kCreateFilePermissions) == kCreateFilePermissions) {
    base::FilePath filename = VirtualPath::BaseName(url.path());
    // See if the name is allowed to create.
    for (size_t i = 0; i < arraysize(kRestrictedNames); ++i) {
      if (filename.value() == kRestrictedNames[i])
        return FILE_PERMISSION_ALWAYS_DENY;
    }
    for (size_t i = 0; i < arraysize(kRestrictedChars); ++i) {
      if (filename.value().find(kRestrictedChars[i]) !=
          base::FilePath::StringType::npos)
        return FILE_PERMISSION_ALWAYS_DENY;
    }
  }

  // Access to the sandbox directory (and only to the directory) should be
  // always allowed.
  return FILE_PERMISSION_ALWAYS_ALLOW;
}

FileSystemOperation* SandboxMountPointProvider::CreateFileSystemOperation(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::PlatformFileError* error_code) const {
  scoped_ptr<FileSystemOperationContext> operation_context(
      new FileSystemOperationContext(context));

  // Copy the observer lists (assuming we only have small number of observers).
  if (url.type() == kFileSystemTypeSyncable) {
    operation_context->set_update_observers(syncable_update_observers_);
    operation_context->set_change_observers(syncable_change_observers_);
    operation_context->set_access_observers(access_observers_);
    return new sync_file_system::SyncableFileSystemOperation(
        context,
        new LocalFileSystemOperation(context, operation_context.Pass()));
  }

  // For regular sandboxed types.
  operation_context->set_update_observers(update_observers_);
  operation_context->set_access_observers(access_observers_);
  return new LocalFileSystemOperation(context, operation_context.Pass());
}

webkit_blob::FileStreamReader*
SandboxMountPointProvider::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const {
  return new FileSystemFileStreamReader(
      context, url, offset, expected_modification_time);
}

fileapi::FileStreamWriter* SandboxMountPointProvider::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context) const {
  return new SandboxFileStreamWriter(
      context, url, offset, update_observers_);
}

FileSystemQuotaUtil* SandboxMountPointProvider::GetQuotaUtil() {
  return this;
}

void SandboxMountPointProvider::DeleteFileSystem(
    const GURL& origin_url,
    FileSystemType type,
    FileSystemContext* context,
    const DeleteFileSystemCallback& callback) {
  base::PostTaskAndReplyWithResult(
      context->task_runners()->file_task_runner(),
      FROM_HERE,
      // It is safe to pass Unretained(this) since context owns it.
      base::Bind(&SandboxMountPointProvider::DeleteOriginDataOnFileThread,
                 base::Unretained(this),
                 make_scoped_refptr(context),
                 base::Unretained(context->quota_manager_proxy()),
                 origin_url,
                 type),
      callback);
}

SandboxMountPointProvider::OriginEnumerator*
SandboxMountPointProvider::CreateOriginEnumerator() {
  return new ObfuscatedOriginEnumerator(sandbox_sync_file_util());
}

base::FilePath SandboxMountPointProvider::GetBaseDirectoryForOriginAndType(
    const GURL& origin_url, fileapi::FileSystemType type, bool create) {

  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath path = sandbox_sync_file_util()->GetDirectoryForOriginAndType(
      origin_url, type, create, &error);
  if (error != base::PLATFORM_FILE_OK)
    return base::FilePath();
  return path;
}

base::PlatformFileError
SandboxMountPointProvider::DeleteOriginDataOnFileThread(
    FileSystemContext* file_system_context,
    QuotaManagerProxy* proxy,
    const GURL& origin_url,
    fileapi::FileSystemType type) {

  int64 usage = GetOriginUsageOnFileThread(file_system_context,
                                           origin_url, type);

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

void SandboxMountPointProvider::GetOriginsForTypeOnFileThread(
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

void SandboxMountPointProvider::GetOriginsForHostOnFileThread(
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

int64 SandboxMountPointProvider::GetOriginUsageOnFileThread(
    FileSystemContext* file_system_context,
    const GURL& origin_url,
    fileapi::FileSystemType type) {
  DCHECK(CanHandleType(type));
  base::FilePath base_path =
      GetBaseDirectoryForOriginAndType(origin_url, type, false);
  if (base_path.empty() || !file_util::DirectoryExists(base_path)) return 0;
  base::FilePath usage_file_path =
      base_path.Append(FileSystemUsageCache::kUsageFileName);

  bool is_valid = FileSystemUsageCache::IsValid(usage_file_path);
  int32 dirty_status = FileSystemUsageCache::GetDirty(usage_file_path);
  bool visited = (visited_origins_.find(origin_url) != visited_origins_.end());
  visited_origins_.insert(origin_url);
  if (is_valid && (dirty_status == 0 || (dirty_status > 0 && visited))) {
    // The usage cache is clean (dirty == 0) or the origin is already
    // initialized and running.  Read the cache file to get the usage.
    return FileSystemUsageCache::GetUsage(usage_file_path);
  }
  // The usage cache has not been initialized or the cache is dirty.
  // Get the directory size now and update the cache.
  FileSystemUsageCache::Delete(usage_file_path);

  FileSystemOperationContext context(file_system_context);
  FileSystemURL url = file_system_context->CreateCrackedFileSystemURL(
      origin_url, type, base::FilePath());
  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> enumerator(
      sandbox_sync_file_util()->CreateFileEnumerator(&context, url, true));

  base::FilePath file_path_each;
  int64 usage = 0;

  while (!(file_path_each = enumerator->Next()).empty()) {
    usage += enumerator->Size();
    usage += ObfuscatedFileUtil::ComputeFilePathCost(file_path_each);
  }
  // This clears the dirty flag too.
  FileSystemUsageCache::UpdateUsage(usage_file_path, usage);
  return usage;
}

void SandboxMountPointProvider::InvalidateUsageCache(
    const GURL& origin_url, fileapi::FileSystemType type) {
  DCHECK(CanHandleType(type));
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath usage_file_path = GetUsageCachePathForOriginAndType(
      sandbox_sync_file_util(), origin_url, type, &error);
  if (error != base::PLATFORM_FILE_OK)
    return;
  FileSystemUsageCache::IncrementDirty(usage_file_path);
}

void SandboxMountPointProvider::CollectOpenFileSystemMetrics(
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

const UpdateObserverList* SandboxMountPointProvider::GetUpdateObservers(
    FileSystemType type) const {
  DCHECK(CanHandleType(type));
  if (type == kFileSystemTypeSyncable)
    return &syncable_update_observers_;
  return &update_observers_;
}

void SandboxMountPointProvider::AddSyncableFileUpdateObserver(
    FileUpdateObserver* observer,
    base::SequencedTaskRunner* task_runner) {
  UpdateObserverList::Source observer_source =
      syncable_update_observers_.source();
  observer_source.AddObserver(observer, task_runner);
  syncable_update_observers_ = UpdateObserverList(observer_source);
}

void SandboxMountPointProvider::AddSyncableFileChangeObserver(
    FileChangeObserver* observer,
    base::SequencedTaskRunner* task_runner) {
  ChangeObserverList::Source observer_source =
      syncable_change_observers_.source();
  observer_source.AddObserver(observer, task_runner);
  syncable_change_observers_ = ChangeObserverList(observer_source);
}

LocalFileSystemOperation*
SandboxMountPointProvider::CreateFileSystemOperationForSync(
    FileSystemContext* file_system_context) {
  scoped_ptr<FileSystemOperationContext> operation_context(
      new FileSystemOperationContext(file_system_context));
  operation_context->set_update_observers(update_observers_);
  operation_context->set_access_observers(access_observers_);
  return new LocalFileSystemOperation(file_system_context,
                                      operation_context.Pass());
}

base::FilePath SandboxMountPointProvider::GetUsageCachePathForOriginAndType(
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
base::FilePath SandboxMountPointProvider::GetUsageCachePathForOriginAndType(
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

bool SandboxMountPointProvider::IsAllowedScheme(const GURL& url) const {
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

ObfuscatedFileUtil* SandboxMountPointProvider::sandbox_sync_file_util() {
  DCHECK(sandbox_file_util_.get());
  return static_cast<ObfuscatedFileUtil*>(sandbox_file_util_->sync_file_util());
}

}  // namespace fileapi
