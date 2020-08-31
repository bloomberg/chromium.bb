// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "net/base/url_util.h"
#include "storage/browser/file_system/async_file_util_adapter.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/file_system_usage_cache.h"
#include "storage/browser/file_system/obfuscated_file_util.h"
#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"
#include "storage/browser/file_system/quota/quota_backend_impl.h"
#include "storage/browser/file_system/quota/quota_reservation.h"
#include "storage/browser/file_system/quota/quota_reservation_manager.h"
#include "storage/browser/file_system/sandbox_file_stream_writer.h"
#include "storage/browser/file_system/sandbox_file_system_backend.h"
#include "storage/browser/file_system/sandbox_quota_observer.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/file_system/file_system_util.h"
#include "url/origin.h"

namespace storage {

namespace {

const char kTemporaryOriginsCountLabel[] = "FileSystem.TemporaryOriginsCount";
const char kPersistentOriginsCountLabel[] = "FileSystem.PersistentOriginsCount";

const char kOpenFileSystemLabel[] = "FileSystem.OpenFileSystem";
const char kOpenFileSystemDetailLabel[] = "FileSystem.OpenFileSystemDetail";
const char kOpenFileSystemDetailNonThrottledLabel[] =
    "FileSystem.OpenFileSystemDetailNonthrottled";
int64_t kMinimumStatsCollectionIntervalHours = 1;

// For type directory names in ObfuscatedFileUtil.
// TODO(kinuko,nhiroki): Each type string registration should be done
// via its own backend.
const char kTemporaryDirectoryName[] = "t";
const char kPersistentDirectoryName[] = "p";
const char kSyncableDirectoryName[] = "s";

const char* const kPrepopulateTypes[] = {kPersistentDirectoryName,
                                         kTemporaryDirectoryName};

enum FileSystemError {
  kOK = 0,
  kIncognito,
  kInvalidSchemeError,
  kCreateDirectoryError,
  kNotFound,
  kUnknownError,
  kFileSystemErrorMax,
};

// Restricted names.
// http://dev.w3.org/2009/dap/file-system/file-dir-sys.html#naming-restrictions
const base::FilePath::CharType* const kRestrictedNames[] = {
    FILE_PATH_LITERAL("."),
    FILE_PATH_LITERAL(".."),
};

// Restricted chars.
const base::FilePath::CharType kRestrictedChars[] = {
    FILE_PATH_LITERAL('/'),
    FILE_PATH_LITERAL('\\'),
};

std::string GetTypeStringForURL(const FileSystemURL& url) {
  return SandboxFileSystemBackendDelegate::GetTypeString(url.type());
}

std::set<std::string> GetKnownTypeStrings() {
  std::set<std::string> known_type_strings;
  known_type_strings.insert(kTemporaryDirectoryName);
  known_type_strings.insert(kPersistentDirectoryName);
  known_type_strings.insert(kSyncableDirectoryName);
  return known_type_strings;
}

class SandboxObfuscatedOriginEnumerator
    : public SandboxFileSystemBackendDelegate::OriginEnumerator {
 public:
  explicit SandboxObfuscatedOriginEnumerator(ObfuscatedFileUtil* file_util) {
    enum_ = file_util->CreateOriginEnumerator();
  }
  ~SandboxObfuscatedOriginEnumerator() override = default;

  base::Optional<url::Origin> Next() override { return enum_->Next(); }

  bool HasFileSystemType(FileSystemType type) const override {
    return enum_->HasTypeDirectory(
        SandboxFileSystemBackendDelegate::GetTypeString(type));
  }

 private:
  std::unique_ptr<ObfuscatedFileUtil::AbstractOriginEnumerator> enum_;
};

void OpenSandboxFileSystemOnFileTaskRunner(ObfuscatedFileUtil* file_util,
                                           const GURL& origin_url,
                                           FileSystemType type,
                                           OpenFileSystemMode mode,
                                           base::File::Error* error_ptr) {
  DCHECK(error_ptr);
  const bool create = (mode == OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT);
  file_util->GetDirectoryForOriginAndType(
      url::Origin::Create(origin_url),
      SandboxFileSystemBackendDelegate::GetTypeString(type), create, error_ptr);
  if (*error_ptr != base::File::FILE_OK) {
    UMA_HISTOGRAM_ENUMERATION(kOpenFileSystemLabel, kCreateDirectoryError,
                              kFileSystemErrorMax);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kOpenFileSystemLabel, kOK, kFileSystemErrorMax);
  }
  // The reference of file_util will be derefed on the FILE thread
  // when the storage of this callback gets deleted regardless of whether
  // this method is called or not.
}

void DidOpenFileSystem(
    base::WeakPtr<SandboxFileSystemBackendDelegate> delegate,
    base::OnceClosure quota_callback,
    base::OnceCallback<void(base::File::Error error)> callback,
    base::File::Error* error) {
  if (delegate)
    delegate->CollectOpenFileSystemMetrics(*error);
  if (*error == base::File::FILE_OK)
    std::move(quota_callback).Run();
  std::move(callback).Run(*error);
}

template <typename T>
void DeleteSoon(base::SequencedTaskRunner* runner, T* ptr) {
  if (!runner->DeleteSoon(FROM_HERE, ptr))
    delete ptr;
}

}  // namespace

const base::FilePath::CharType
    SandboxFileSystemBackendDelegate::kFileSystemDirectory[] =
        FILE_PATH_LITERAL("File System");

// static
std::string SandboxFileSystemBackendDelegate::GetTypeString(
    FileSystemType type) {
  switch (type) {
    case kFileSystemTypeTemporary:
      return kTemporaryDirectoryName;
    case kFileSystemTypePersistent:
      return kPersistentDirectoryName;
    case kFileSystemTypeSyncable:
    case kFileSystemTypeSyncableForInternalSync:
      return kSyncableDirectoryName;
    case kFileSystemTypeUnknown:
    default:
      NOTREACHED() << "Unknown filesystem type requested:" << type;
      return std::string();
  }
}

SandboxFileSystemBackendDelegate::SandboxFileSystemBackendDelegate(
    QuotaManagerProxy* quota_manager_proxy,
    base::SequencedTaskRunner* file_task_runner,
    const base::FilePath& profile_path,
    SpecialStoragePolicy* special_storage_policy,
    const FileSystemOptions& file_system_options,
    leveldb::Env* env_override)
    : file_task_runner_(file_task_runner),
      quota_manager_proxy_(quota_manager_proxy),
      sandbox_file_util_(std::make_unique<AsyncFileUtilAdapter>(
          new ObfuscatedFileUtil(special_storage_policy,
                                 profile_path.Append(kFileSystemDirectory),
                                 env_override,
                                 base::BindRepeating(&GetTypeStringForURL),
                                 GetKnownTypeStrings(),
                                 this,
                                 file_system_options.is_incognito()))),
      file_system_usage_cache_(std::make_unique<FileSystemUsageCache>(
          file_system_options.is_incognito())),
      quota_observer_(new SandboxQuotaObserver(quota_manager_proxy,
                                               file_task_runner,
                                               obfuscated_file_util(),
                                               usage_cache())),
      quota_reservation_manager_(new QuotaReservationManager(
          std::make_unique<QuotaBackendImpl>(file_task_runner_.get(),
                                             obfuscated_file_util(),
                                             usage_cache(),
                                             quota_manager_proxy))),
      special_storage_policy_(special_storage_policy),
      file_system_options_(file_system_options),
      is_filesystem_opened_(false) {
  // Prepopulate database only if it can run asynchronously (i.e. the current
  // sequence is not file_task_runner). Usually this is the case but may not
  // in test code.
  if (!file_system_options.is_incognito() &&
      !file_task_runner_->RunsTasksInCurrentSequence()) {
    std::vector<std::string> types_to_prepopulate(
        &kPrepopulateTypes[0],
        &kPrepopulateTypes[base::size(kPrepopulateTypes)]);
    file_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ObfuscatedFileUtil::MaybePrepopulateDatabase,
                                  base::Unretained(obfuscated_file_util()),
                                  types_to_prepopulate));
  }
}

SandboxFileSystemBackendDelegate::~SandboxFileSystemBackendDelegate() {
  DETACH_FROM_THREAD(io_thread_checker_);

  if (!file_task_runner_->RunsTasksInCurrentSequence()) {
    DeleteSoon(file_task_runner_.get(), quota_reservation_manager_.release());
    DeleteSoon(file_task_runner_.get(), sandbox_file_util_.release());
    DeleteSoon(file_task_runner_.get(), quota_observer_.release());
    DeleteSoon(file_task_runner_.get(), file_system_usage_cache_.release());
  }
}

SandboxFileSystemBackendDelegate::OriginEnumerator*
SandboxFileSystemBackendDelegate::CreateOriginEnumerator() {
  return new SandboxObfuscatedOriginEnumerator(obfuscated_file_util());
}

base::FilePath
SandboxFileSystemBackendDelegate::GetBaseDirectoryForOriginAndType(
    const url::Origin& origin,
    FileSystemType type,
    bool create) {
  base::File::Error error = base::File::FILE_OK;
  base::FilePath path = obfuscated_file_util()->GetDirectoryForOriginAndType(
      origin, GetTypeString(type), create, &error);
  if (error != base::File::FILE_OK)
    return base::FilePath();
  return path;
}

void SandboxFileSystemBackendDelegate::OpenFileSystem(
    const url::Origin& origin,
    FileSystemType type,
    OpenFileSystemMode mode,
    OpenFileSystemCallback callback,
    const GURL& root_url) {
  if (!IsAllowedScheme(origin.GetURL())) {
    std::move(callback).Run(GURL(), std::string(),
                            base::File::FILE_ERROR_SECURITY);
    return;
  }

  std::string name = GetFileSystemName(origin.GetURL(), type);

  // |quota_manager_proxy_| may be null in unit tests.
  base::OnceClosure quota_callback =
      (quota_manager_proxy_.get())
          ? base::BindOnce(&QuotaManagerProxy::NotifyStorageAccessed,
                           quota_manager_proxy_, origin,
                           FileSystemTypeToQuotaStorageType(type))
          : base::DoNothing();

  base::File::Error* error_ptr = new base::File::Error;
  file_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&OpenSandboxFileSystemOnFileTaskRunner,
                     obfuscated_file_util(), origin.GetURL(), type, mode,
                     base::Unretained(error_ptr)),
      base::BindOnce(&DidOpenFileSystem, weak_factory_.GetWeakPtr(),
                     std::move(quota_callback),
                     base::BindOnce(std::move(callback), root_url, name),
                     base::Owned(error_ptr)));

  DETACH_FROM_THREAD(io_thread_checker_);
  is_filesystem_opened_ = true;
}

std::unique_ptr<FileSystemOperationContext>
SandboxFileSystemBackendDelegate::CreateFileSystemOperationContext(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::File::Error* error_code) const {
  if (!IsAccessValid(url)) {
    *error_code = base::File::FILE_ERROR_SECURITY;
    return nullptr;
  }

  const UpdateObserverList* update_observers = GetUpdateObservers(url.type());
  const ChangeObserverList* change_observers = GetChangeObservers(url.type());
  DCHECK(update_observers);

  std::unique_ptr<FileSystemOperationContext> operation_context(
      new FileSystemOperationContext(context));
  operation_context->set_update_observers(*update_observers);
  operation_context->set_change_observers(
      change_observers ? *change_observers : ChangeObserverList());

  return operation_context;
}

std::unique_ptr<FileStreamReader>
SandboxFileSystemBackendDelegate::CreateFileStreamReader(
    const FileSystemURL& url,
    int64_t offset,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const {
  if (!IsAccessValid(url))
    return nullptr;
  return FileStreamReader::CreateForFileSystemFile(context, url, offset,
                                                   expected_modification_time);
}

std::unique_ptr<FileStreamWriter>
SandboxFileSystemBackendDelegate::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64_t offset,
    FileSystemContext* context,
    FileSystemType type) const {
  if (!IsAccessValid(url))
    return nullptr;
  const UpdateObserverList* observers = GetUpdateObservers(type);
  DCHECK(observers);
  return std::make_unique<SandboxFileStreamWriter>(context, url, offset,
                                                   *observers);
}

base::File::Error
SandboxFileSystemBackendDelegate::DeleteOriginDataOnFileTaskRunner(
    FileSystemContext* file_system_context,
    QuotaManagerProxy* proxy,
    const url::Origin& origin,
    FileSystemType type) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  int64_t usage =
      GetOriginUsageOnFileTaskRunner(file_system_context, origin, type);
  usage_cache()->CloseCacheFiles();
  bool result = obfuscated_file_util()->DeleteDirectoryForOriginAndType(
      origin, GetTypeString(type));
  if (result && proxy && usage) {
    proxy->NotifyStorageModified(QuotaClientType::kFileSystem, origin,
                                 FileSystemTypeToQuotaStorageType(type),
                                 -usage);
  }

  if (result)
    return base::File::FILE_OK;
  return base::File::FILE_ERROR_FAILED;
}

void SandboxFileSystemBackendDelegate::PerformStorageCleanupOnFileTaskRunner(
    FileSystemContext* context,
    QuotaManagerProxy* proxy,
    FileSystemType type) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  obfuscated_file_util()->RewriteDatabases();
}

void SandboxFileSystemBackendDelegate::GetOriginsForTypeOnFileTaskRunner(
    FileSystemType type,
    std::set<url::Origin>* origins) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(origins);
  std::unique_ptr<OriginEnumerator> enumerator(CreateOriginEnumerator());
  base::Optional<url::Origin> origin;
  while ((origin = enumerator->Next()).has_value()) {
    if (enumerator->HasFileSystemType(type))
      origins->insert(origin.value());
  }
  switch (type) {
    case kFileSystemTypeTemporary:
      UMA_HISTOGRAM_COUNTS_1M(kTemporaryOriginsCountLabel, origins->size());
      break;
    case kFileSystemTypePersistent:
      UMA_HISTOGRAM_COUNTS_1M(kPersistentOriginsCountLabel, origins->size());
      break;
    default:
      break;
  }
}

void SandboxFileSystemBackendDelegate::GetOriginsForHostOnFileTaskRunner(
    FileSystemType type,
    const std::string& host,
    std::set<url::Origin>* origins) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(origins);
  std::unique_ptr<OriginEnumerator> enumerator(CreateOriginEnumerator());
  base::Optional<url::Origin> origin;
  while ((origin = enumerator->Next()).has_value()) {
    if (host == net::GetHostOrSpecFromURL(origin->GetURL()) &&
        enumerator->HasFileSystemType(type))
      origins->insert(origin.value());
  }
}

int64_t SandboxFileSystemBackendDelegate::GetOriginUsageOnFileTaskRunner(
    FileSystemContext* file_system_context,
    const url::Origin& origin,
    FileSystemType type) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

  // Don't use usage cache and return recalculated usage for sticky invalidated
  // origins.
  if (base::Contains(sticky_dirty_origins_, std::make_pair(origin, type)))
    return RecalculateUsage(file_system_context, origin, type);

  base::FilePath base_path =
      GetBaseDirectoryForOriginAndType(origin, type, false);
  if (base_path.empty() ||
      !obfuscated_file_util()->delegate()->DirectoryExists(base_path)) {
    return 0;
  }
  base::FilePath usage_file_path =
      base_path.Append(FileSystemUsageCache::kUsageFileName);

  bool is_valid = usage_cache()->IsValid(usage_file_path);
  uint32_t dirty_status = 0;
  bool dirty_status_available =
      usage_cache()->GetDirty(usage_file_path, &dirty_status);
  bool visited = !visited_origins_.insert(origin).second;
  if (is_valid && (dirty_status == 0 || (dirty_status_available && visited))) {
    // The usage cache is clean (dirty == 0) or the origin is already
    // initialized and running.  Read the cache file to get the usage.
    int64_t usage = 0;
    return usage_cache()->GetUsage(usage_file_path, &usage) ? usage : -1;
  }
  // The usage cache has not been initialized or the cache is dirty.
  // Get the directory size now and update the cache.
  usage_cache()->Delete(usage_file_path);

  int64_t usage = RecalculateUsage(file_system_context, origin, type);

  // This clears the dirty flag too.
  usage_cache()->UpdateUsage(usage_file_path, usage);
  return usage;
}

scoped_refptr<QuotaReservation>
SandboxFileSystemBackendDelegate::CreateQuotaReservationOnFileTaskRunner(
    const url::Origin& origin,
    FileSystemType type) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(quota_reservation_manager_);
  return quota_reservation_manager_->CreateReservation(origin, type);
}

void SandboxFileSystemBackendDelegate::AddFileUpdateObserver(
    FileSystemType type,
    FileUpdateObserver* observer,
    base::SequencedTaskRunner* task_runner) {
#if DCHECK_IS_ON()
  DCHECK(!is_filesystem_opened_ || io_thread_checker_.CalledOnValidThread());
#endif
  update_observers_[type] =
      update_observers_[type].AddObserver(observer, task_runner);
}

void SandboxFileSystemBackendDelegate::AddFileChangeObserver(
    FileSystemType type,
    FileChangeObserver* observer,
    base::SequencedTaskRunner* task_runner) {
#if DCHECK_IS_ON()
  DCHECK(!is_filesystem_opened_ || io_thread_checker_.CalledOnValidThread());
#endif
  change_observers_[type] =
      change_observers_[type].AddObserver(observer, task_runner);
}

void SandboxFileSystemBackendDelegate::AddFileAccessObserver(
    FileSystemType type,
    FileAccessObserver* observer,
    base::SequencedTaskRunner* task_runner) {
#if DCHECK_IS_ON()
  DCHECK(!is_filesystem_opened_ || io_thread_checker_.CalledOnValidThread());
#endif
  access_observers_[type] =
      access_observers_[type].AddObserver(observer, task_runner);
}

const UpdateObserverList* SandboxFileSystemBackendDelegate::GetUpdateObservers(
    FileSystemType type) const {
  auto iter = update_observers_.find(type);
  if (iter == update_observers_.end())
    return nullptr;
  return &iter->second;
}

const ChangeObserverList* SandboxFileSystemBackendDelegate::GetChangeObservers(
    FileSystemType type) const {
  auto iter = change_observers_.find(type);
  if (iter == change_observers_.end())
    return nullptr;
  return &iter->second;
}

const AccessObserverList* SandboxFileSystemBackendDelegate::GetAccessObservers(
    FileSystemType type) const {
  auto iter = access_observers_.find(type);
  if (iter == access_observers_.end())
    return nullptr;
  return &iter->second;
}

void SandboxFileSystemBackendDelegate::RegisterQuotaUpdateObserver(
    FileSystemType type) {
  AddFileUpdateObserver(type, quota_observer_.get(), file_task_runner_.get());
}

void SandboxFileSystemBackendDelegate::InvalidateUsageCache(
    const url::Origin& origin,
    FileSystemType type) {
  base::File::Error error = base::File::FILE_OK;
  base::FilePath usage_file_path = GetUsageCachePathForOriginAndType(
      obfuscated_file_util(), origin, type, &error);
  if (error != base::File::FILE_OK)
    return;
  usage_cache()->IncrementDirty(usage_file_path);
}

void SandboxFileSystemBackendDelegate::StickyInvalidateUsageCache(
    const url::Origin& origin,
    FileSystemType type) {
  sticky_dirty_origins_.insert(std::make_pair(origin, type));
  quota_observer()->SetUsageCacheEnabled(origin, type, false);
  InvalidateUsageCache(origin, type);
}

FileSystemFileUtil* SandboxFileSystemBackendDelegate::sync_file_util() {
  return static_cast<AsyncFileUtilAdapter*>(file_util())->sync_file_util();
}

bool SandboxFileSystemBackendDelegate::IsAccessValid(
    const FileSystemURL& url) const {
  if (!IsAllowedScheme(url.origin().GetURL()))
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
  for (const auto* const restricted_name : kRestrictedNames) {
    if (filename.value() == restricted_name)
      return false;
  }
  for (const auto& restricted_char : kRestrictedChars) {
    if (filename.value().find(restricted_char) !=
        base::FilePath::StringType::npos)
      return false;
  }

  return true;
}

bool SandboxFileSystemBackendDelegate::IsAllowedScheme(const GURL& url) const {
  // Basically we only accept http or https. We allow file:// URLs
  // only if --allow-file-access-from-files flag is given.
  if (url.SchemeIsHTTPOrHTTPS())
    return true;
  if (url.SchemeIsFileSystem())
    return url.inner_url() && IsAllowedScheme(*url.inner_url());

  for (size_t i = 0;
       i < file_system_options_.additional_allowed_schemes().size(); ++i) {
    if (url.SchemeIs(
            file_system_options_.additional_allowed_schemes()[i].c_str()))
      return true;
  }
  return false;
}

base::FilePath
SandboxFileSystemBackendDelegate::GetUsageCachePathForOriginAndType(
    const url::Origin& origin,
    FileSystemType type) {
  base::File::Error error;
  base::FilePath path = GetUsageCachePathForOriginAndType(
      obfuscated_file_util(), origin, type, &error);
  if (error != base::File::FILE_OK)
    return base::FilePath();
  return path;
}

// static
base::FilePath
SandboxFileSystemBackendDelegate::GetUsageCachePathForOriginAndType(
    ObfuscatedFileUtil* sandbox_file_util,
    const url::Origin& origin,
    FileSystemType type,
    base::File::Error* error_out) {
  DCHECK(error_out);
  *error_out = base::File::FILE_OK;
  base::FilePath base_path = sandbox_file_util->GetDirectoryForOriginAndType(
      origin, GetTypeString(type), false /* create */, error_out);
  if (*error_out != base::File::FILE_OK)
    return base::FilePath();
  return base_path.Append(FileSystemUsageCache::kUsageFileName);
}

int64_t SandboxFileSystemBackendDelegate::RecalculateUsage(
    FileSystemContext* context,
    const url::Origin& origin,
    FileSystemType type) {
  FileSystemOperationContext operation_context(context);
  FileSystemURL url =
      context->CreateCrackedFileSystemURL(origin, type, base::FilePath());
  std::unique_ptr<FileSystemFileUtil::AbstractFileEnumerator> enumerator(
      obfuscated_file_util()->CreateFileEnumerator(&operation_context, url,
                                                   true));

  base::FilePath file_path_each;
  int64_t usage = 0;

  while (!(file_path_each = enumerator->Next()).empty()) {
    usage += enumerator->Size();
    usage += ObfuscatedFileUtil::ComputeFilePathCost(file_path_each);
  }

  return usage;
}

void SandboxFileSystemBackendDelegate::CollectOpenFileSystemMetrics(
    base::File::Error error_code) {
  base::Time now = base::Time::Now();
  bool throttled = now < next_release_time_for_open_filesystem_stat_;
  if (!throttled) {
    next_release_time_for_open_filesystem_stat_ =
        now + base::TimeDelta::FromHours(kMinimumStatsCollectionIntervalHours);
  }

#define REPORT(report_value)                                            \
  UMA_HISTOGRAM_ENUMERATION(kOpenFileSystemDetailLabel, (report_value), \
                            kFileSystemErrorMax);                       \
  if (!throttled) {                                                     \
    UMA_HISTOGRAM_ENUMERATION(kOpenFileSystemDetailNonThrottledLabel,   \
                              (report_value), kFileSystemErrorMax);     \
  }

  switch (error_code) {
    case base::File::FILE_OK:
      REPORT(kOK);
      break;
    case base::File::FILE_ERROR_INVALID_URL:
      REPORT(kInvalidSchemeError);
      break;
    case base::File::FILE_ERROR_NOT_FOUND:
      REPORT(kNotFound);
      break;
    case base::File::FILE_ERROR_FAILED:
    default:
      REPORT(kUnknownError);
      break;
  }
#undef REPORT
}

void SandboxFileSystemBackendDelegate::CopyFileSystem(
    const url::Origin& origin,
    FileSystemType type,
    SandboxFileSystemBackendDelegate* destination) {
  DCHECK(file_task_runner()->RunsTasksInCurrentSequence());

  base::FilePath base_path =
      GetBaseDirectoryForOriginAndType(origin, type, /*create=*/false);
  if (base::PathExists(base_path)) {
    // Delete any existing file system directories in the destination. A
    // previously failed migration
    // may have left behind partially copied directories.
    base::FilePath dest_path = destination->GetBaseDirectoryForOriginAndType(
        origin, type, /*create=*/false);

    // Make sure we're not about to delete our own file system.
    CHECK_NE(base_path.value(), dest_path.value());
    base::DeleteFileRecursively(dest_path);

    dest_path = destination->GetBaseDirectoryForOriginAndType(origin, type,
                                                              /*create=*/true);

    obfuscated_file_util()->CloseFileSystemForOriginAndType(
        origin, GetTypeString(type));
    base::CopyDirectory(base_path, dest_path.DirName(), true /* rescursive */);
  }
}

ObfuscatedFileUtil* SandboxFileSystemBackendDelegate::obfuscated_file_util() {
  return static_cast<ObfuscatedFileUtil*>(sync_file_util());
}

base::WeakPtr<ObfuscatedFileUtilMemoryDelegate>
SandboxFileSystemBackendDelegate::memory_file_util_delegate() {
  DCHECK(obfuscated_file_util()->is_incognito());
  return static_cast<ObfuscatedFileUtilMemoryDelegate*>(
             obfuscated_file_util()->delegate())
      ->GetWeakPtr();
}

// Declared in obfuscated_file_util.h.
// static
ObfuscatedFileUtil* ObfuscatedFileUtil::CreateForTesting(
    SpecialStoragePolicy* special_storage_policy,
    const base::FilePath& file_system_directory,
    leveldb::Env* env_override,
    bool is_incognito) {
  return new ObfuscatedFileUtil(special_storage_policy, file_system_directory,
                                env_override,
                                base::BindRepeating(&GetTypeStringForURL),
                                GetKnownTypeStrings(), nullptr, is_incognito);
}

}  // namespace storage
