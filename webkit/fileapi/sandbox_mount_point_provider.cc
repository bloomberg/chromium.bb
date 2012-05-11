// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/sandbox_mount_point_provider.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "webkit/fileapi/file_system_file_reader.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_options.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/native_file_util.h"
#include "webkit/fileapi/obfuscated_file_util.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/quota/quota_manager.h"

using quota::QuotaManagerProxy;

namespace fileapi {

namespace {

const char kChromeScheme[] = "chrome";
const char kExtensionScheme[] = "chrome-extension";

const FilePath::CharType kOldFileSystemUniqueNamePrefix[] =
    FILE_PATH_LITERAL("chrome-");
const size_t kOldFileSystemUniqueLength = 16;
const size_t kOldFileSystemUniqueDirectoryNameLength =
    kOldFileSystemUniqueLength + arraysize(kOldFileSystemUniqueNamePrefix) - 1;

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
const FilePath::CharType* const kRestrictedNames[] = {
  FILE_PATH_LITERAL("."), FILE_PATH_LITERAL(".."),
};

// Restricted chars.
const FilePath::CharType kRestrictedChars[] = {
  FILE_PATH_LITERAL('/'), FILE_PATH_LITERAL('\\'),
};

FilePath::StringType OldCreateUniqueDirectoryName(const GURL& origin_url) {
  // This can be anything but need to be unpredictable.
  static const FilePath::CharType letters[] = FILE_PATH_LITERAL(
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
  FilePath::StringType unique(kOldFileSystemUniqueNamePrefix);
  for (size_t i = 0; i < kOldFileSystemUniqueLength; ++i)
    unique += letters[base::RandInt(0, arraysize(letters) - 2)];
  return unique;
}

base::PlatformFileError OldReadOriginDirectory(const FilePath& base_path,
                                               FilePath* unique) {
  file_util::FileEnumerator file_enum(
      base_path, false /* recursive */,
      file_util::FileEnumerator::DIRECTORIES,
      FilePath::StringType(kOldFileSystemUniqueNamePrefix) +
          FILE_PATH_LITERAL("*"));
  FilePath current;
  bool found = false;
  while (!(current = file_enum.Next()).empty()) {
    if (current.BaseName().value().length() !=
        kOldFileSystemUniqueDirectoryNameLength)
      continue;
    if (found) {
      LOG(WARNING) << "Unexpectedly found more than one FileSystem "
                   << "directory";
      return base::PLATFORM_FILE_ERROR_FAILED;
    }
    found = true;
    *unique = current;
  }
  if (unique->empty())
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  return base::PLATFORM_FILE_OK;
}

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

class OldSandboxOriginEnumerator
    : public SandboxMountPointProvider::OriginEnumerator {
 public:
  explicit OldSandboxOriginEnumerator(const FilePath& base_path)
      : enumerator_(base_path, false /* recursive */,
                    file_util::FileEnumerator::DIRECTORIES) {}
  virtual ~OldSandboxOriginEnumerator() {}

  virtual GURL Next() OVERRIDE {
    current_ = enumerator_.Next();
    if (current_.empty())
      return GURL();
    return GetOriginURLFromIdentifier(current_.BaseName().MaybeAsASCII());
  }

  virtual bool HasFileSystemType(fileapi::FileSystemType type) const OVERRIDE {
    if (current_.empty())
      return false;
    std::string directory = GetFileSystemTypeString(type);
    DCHECK(!directory.empty());
    return file_util::DirectoryExists(current_.AppendASCII(directory));
  }

 private:
  file_util::FileEnumerator enumerator_;
  FilePath current_;
};

FilePath OldGetBaseDirectoryForOrigin(
    const FilePath& old_base_path,
    const GURL& origin_url) {
  std::string id = GetOriginIdentifierFromURL(origin_url);
  if (!id.empty())
    return old_base_path.AppendASCII(id);
  return FilePath();
}

FilePath OldGetBaseDirectoryForOriginAndType(
    const FilePath& old_base_path,
    const GURL& origin_url, fileapi::FileSystemType type) {
  std::string type_string = GetFileSystemTypeString(type);
  if (type_string.empty()) {
    NOTREACHED();
    return FilePath();
  }
  FilePath base_path = OldGetBaseDirectoryForOrigin(
      old_base_path, origin_url);
  if (base_path.empty()) {
    NOTREACHED();
    return FilePath();
  }
  return base_path.AppendASCII(type_string);
}

bool MigrateOneOldFileSystem(
    ObfuscatedFileUtil* file_util,
    const FilePath& old_base_path, const GURL& origin,
    fileapi::FileSystemType type) {
  FilePath base_path = OldGetBaseDirectoryForOriginAndType(
      old_base_path, origin, type);
  if (base_path.empty())
    return false;

  FilePath root;
  base::PlatformFileError result = OldReadOriginDirectory(base_path, &root);
  if (base::PLATFORM_FILE_ERROR_NOT_FOUND == result)
    return true;  // There was nothing to migrate; call that a success.

  // If we found more than one filesystem [a problem we don't know how to
  // solve], the data is already not accessible through Chrome, so it won't do
  // any harm not to migrate it.  Just flag it as an error, so that we don't
  // delete it.
  if (base::PLATFORM_FILE_OK != result)
    return false;

  if (!file_util->MigrateFromOldSandbox(origin, type, root)) {
    LOG(WARNING) << "Failed to migrate filesystem for origin " << origin <<
        " and type " << type;
    return false;
  }
  return true;
}

void MigrateAllOldFileSystems(
    ObfuscatedFileUtil* file_util,
    const FilePath& old_base_path) {
  scoped_ptr<OldSandboxOriginEnumerator> old_origins(
      new OldSandboxOriginEnumerator(old_base_path));
  GURL origin;
  int failures = 0;
  while (!(origin = old_origins->Next()).is_empty()) {
    int failures_this_origin = 0;
    if (old_origins->HasFileSystemType(kFileSystemTypeTemporary) &&
        !MigrateOneOldFileSystem(
            file_util, old_base_path, origin,
            kFileSystemTypeTemporary))
      ++failures_this_origin;
    if (old_origins->HasFileSystemType(kFileSystemTypePersistent) &&
        !MigrateOneOldFileSystem(
            file_util, old_base_path, origin,
            kFileSystemTypePersistent))
      ++failures_this_origin;
    if (!failures_this_origin) {
      FilePath origin_base_path =
          OldGetBaseDirectoryForOrigin(old_base_path, origin);
      // Yes, that's an rm -rf.  Make sure that path looks valid, just in case.
      if (!origin_base_path.empty())
        file_util::Delete(origin_base_path, true);
    }
    failures += failures_this_origin;
  }
  if (!failures)
    file_util::Delete(old_base_path, true);
  if (file_util::DirectoryExists(old_base_path)) {
    // Move it out of the way so that we won't keep trying to migrate it.  You
    // get only one chance at this; the bits we couldn't do this time, we're
    // unlikely to be able to do in the future.  This way you can now use the
    // new filesystem, but have a way to recover your old files if absolutely
    // necessary.
    FilePath new_path =
        old_base_path.DirName().Append(
            SandboxMountPointProvider::kRenamedOldFileSystemDirectory);
    file_util::ReplaceFile(old_base_path, new_path);
  }
}

// A migration, whether successful or not, will try to move this directory out
// of the way so that we never try to migrate it again.  We need to do this
// check on all public entry points in this file, so that it's guaranteed to be
// done before anyone looks up a filesystem.  Most entry points start by trying
// to look up the filesystem's root, so we can take care of most of them by
// putting a check there.
void MigrateIfNeeded(
    ObfuscatedFileUtil* file_util,
    const FilePath& old_base_path) {
  if (file_util::DirectoryExists(old_base_path))
    MigrateAllOldFileSystems(file_util, old_base_path);
}

void PassPointerErrorByValue(
    const base::Callback<void(PlatformFileError)>& callback,
    PlatformFileError* error_ptr) {
  DCHECK(error_ptr);
  callback.Run(*error_ptr);
}

void DidValidateFileSystemRoot(
    base::WeakPtr<SandboxMountPointProvider> mount_point_provider,
    const base::Callback<void(PlatformFileError)>& callback,
    base::PlatformFileError* error) {
  if (mount_point_provider.get())
    mount_point_provider.get()->CollectOpenFileSystemMetrics(*error);
  callback.Run(*error);
}

void ValidateRootOnFileThread(
    ObfuscatedFileUtil* file_util,
    const GURL& origin_url,
    FileSystemType type,
    const FilePath& old_base_path,
    bool create,
    base::PlatformFileError* error_ptr) {
  DCHECK(error_ptr);
  MigrateIfNeeded(file_util, old_base_path);

  FilePath root_path =
      file_util->GetDirectoryForOriginAndType(
          origin_url, type, create, error_ptr);
  if (root_path.empty()) {
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

const FilePath::CharType SandboxMountPointProvider::kOldFileSystemDirectory[] =
    FILE_PATH_LITERAL("FileSystem");

const FilePath::CharType SandboxMountPointProvider::kNewFileSystemDirectory[] =
    FILE_PATH_LITERAL("File System");

const FilePath::CharType
    SandboxMountPointProvider::kRenamedOldFileSystemDirectory[] =
        FILE_PATH_LITERAL("FS.old");

SandboxMountPointProvider::SandboxMountPointProvider(
    base::SequencedTaskRunner* file_task_runner,
    const FilePath& profile_path,
    const FileSystemOptions& file_system_options)
    : FileSystemQuotaUtil(file_task_runner),
      file_task_runner_(file_task_runner),
      profile_path_(profile_path),
      file_system_options_(file_system_options),
      sandbox_file_util_(
          new ObfuscatedFileUtil(
              profile_path.Append(kNewFileSystemDirectory),
              new NativeFileUtil())),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

SandboxMountPointProvider::~SandboxMountPointProvider() {
  if (!file_task_runner_->RunsTasksOnCurrentThread()) {
    ObfuscatedFileUtil* sandbox_file_util = sandbox_file_util_.release();
    if (!file_task_runner_->ReleaseSoon(FROM_HERE, sandbox_file_util))
      sandbox_file_util->Release();
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
                 sandbox_file_util_,
                 origin_url, type, old_base_path(), create,
                 base::Unretained(error_ptr)),
      base::Bind(&DidValidateFileSystemRoot,
                 weak_factory_.GetWeakPtr(),
                 callback, base::Owned(error_ptr)));
};

FilePath
SandboxMountPointProvider::GetFileSystemRootPathOnFileThread(
    const GURL& origin_url, FileSystemType type, const FilePath& unused,
    bool create) {
  if (file_system_options_.is_incognito())
    // TODO(kinuko): return an isolated temporary directory.
    return FilePath();

  if (!IsAllowedScheme(origin_url))
    return FilePath();

  MigrateIfNeeded(sandbox_file_util_, old_base_path());

  return sandbox_file_util_->GetDirectoryForOriginAndType(
      origin_url, type, create);
}

bool SandboxMountPointProvider::IsAccessAllowed(const GURL& origin_url,
                                                FileSystemType type,
                                                const FilePath& unused) {
  if (type != kFileSystemTypeTemporary && type != kFileSystemTypePersistent)
    return false;
  // We essentially depend on quota to do our access controls, so here
  // we only check if the requested scheme is allowed or not.
  return IsAllowedScheme(origin_url);
}

bool SandboxMountPointProvider::IsRestrictedFileName(const FilePath& filename)
    const {
  if (filename.value().empty())
    return false;

  for (size_t i = 0; i < arraysize(kRestrictedNames); ++i) {
    // Exact match.
    if (filename.value() == kRestrictedNames[i])
      return true;
  }

  for (size_t i = 0; i < arraysize(kRestrictedChars); ++i) {
    if (filename.value().find(kRestrictedChars[i]) !=
        FilePath::StringType::npos)
      return true;
  }

  return false;
}

std::vector<FilePath> SandboxMountPointProvider::GetRootDirectories() const {
  NOTREACHED();
  // TODO(ericu): Implement this method and check for access permissions as
  // fileBrowserPrivate extension API does. We currently have another mechanism,
  // but we should switch over.  This may also need to call MigrateIfNeeded().
  return  std::vector<FilePath>();
}

FileSystemFileUtil* SandboxMountPointProvider::GetFileUtil() {
  return sandbox_file_util_.get();
}

FilePath SandboxMountPointProvider::GetPathForPermissionsCheck(
    const FilePath& virtual_path) const {
  // We simply return the very top directory of the sandbox
  // filesystem regardless of the input path.
  return new_base_path();
}

FileSystemOperationInterface*
SandboxMountPointProvider::CreateFileSystemOperation(
    const GURL& origin_url,
    FileSystemType file_system_type,
    const FilePath& virtual_path,
    FileSystemContext* context) const {
  return new FileSystemOperation(context);
}

webkit_blob::FileReader* SandboxMountPointProvider::CreateFileReader(
    const GURL& url,
    int64 offset,
    FileSystemContext* context) const {
  return new FileSystemFileReader(context, url, offset);
}

FileSystemQuotaUtil* SandboxMountPointProvider::GetQuotaUtil() {
  return this;
}

FilePath SandboxMountPointProvider::old_base_path() const {
  return profile_path_.Append(kOldFileSystemDirectory);
}

FilePath SandboxMountPointProvider::new_base_path() const {
  return profile_path_.Append(kNewFileSystemDirectory);
}

FilePath SandboxMountPointProvider::renamed_old_base_path() const {
  return profile_path_.Append(kRenamedOldFileSystemDirectory);
}

SandboxMountPointProvider::OriginEnumerator*
SandboxMountPointProvider::CreateOriginEnumerator() const {
  MigrateIfNeeded(sandbox_file_util_, old_base_path());
  return new ObfuscatedOriginEnumerator(sandbox_file_util_.get());
}

FilePath SandboxMountPointProvider::GetBaseDirectoryForOriginAndType(
    const GURL& origin_url, fileapi::FileSystemType type, bool create) const {

  MigrateIfNeeded(sandbox_file_util_, old_base_path());

  return sandbox_file_util_->GetDirectoryForOriginAndType(
      origin_url, type, create);
}

bool SandboxMountPointProvider::DeleteOriginDataOnFileThread(
    QuotaManagerProxy* proxy, const GURL& origin_url,
    fileapi::FileSystemType type) {
  MigrateIfNeeded(sandbox_file_util_, old_base_path());

  int64 usage = GetOriginUsageOnFileThread(origin_url, type);

  bool result =
      sandbox_file_util_->DeleteDirectoryForOriginAndType(origin_url, type);
  if (result && proxy) {
    proxy->NotifyStorageModified(
        quota::QuotaClient::kFileSystem,
        origin_url,
        FileSystemTypeToQuotaStorageType(type),
        -usage);
  }
  return result;
}

void SandboxMountPointProvider::GetOriginsForTypeOnFileThread(
    fileapi::FileSystemType type, std::set<GURL>* origins) {
  DCHECK(type == kFileSystemTypeTemporary ||
         type == kFileSystemTypePersistent);
  DCHECK(origins);
  scoped_ptr<OriginEnumerator> enumerator(CreateOriginEnumerator());
  GURL origin;
  while (!(origin = enumerator->Next()).is_empty()) {
    if (enumerator->HasFileSystemType(type))
      origins->insert(origin);
  }
  if (type == kFileSystemTypeTemporary) {
    UMA_HISTOGRAM_COUNTS(kTemporaryOriginsCountLabel,
                         origins->size());
  } else {
    UMA_HISTOGRAM_COUNTS(kPersistentOriginsCountLabel,
                         origins->size());
  }
}

void SandboxMountPointProvider::GetOriginsForHostOnFileThread(
    fileapi::FileSystemType type, const std::string& host,
    std::set<GURL>* origins) {
  DCHECK(type == kFileSystemTypeTemporary ||
         type == kFileSystemTypePersistent);
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
    const GURL& origin_url, fileapi::FileSystemType type) {
  DCHECK(type == kFileSystemTypeTemporary ||
         type == kFileSystemTypePersistent);
  FilePath base_path =
      GetBaseDirectoryForOriginAndType(origin_url, type, false);
  if (base_path.empty() || !file_util::DirectoryExists(base_path)) return 0;
  FilePath usage_file_path =
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

  FileSystemOperationContext context(NULL);
  FileSystemPath path(origin_url, type, FilePath());
  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> enumerator(
      sandbox_file_util_->CreateFileEnumerator(&context, path, true));

  FilePath file_path_each;
  int64 usage = 0;

  while (!(file_path_each = enumerator->Next()).empty()) {
    base::PlatformFileInfo file_info;
    FilePath platform_file_path;
    usage += enumerator->Size();
    usage += ObfuscatedFileUtil::ComputeFilePathCost(file_path_each);
  }
  // This clears the dirty flag too.
  FileSystemUsageCache::UpdateUsage(usage_file_path, usage);
  return usage;
}

void SandboxMountPointProvider::NotifyOriginWasAccessedOnIOThread(
    QuotaManagerProxy* proxy, const GURL& origin_url,
    fileapi::FileSystemType type) {
  DCHECK(type == kFileSystemTypeTemporary ||
         type == kFileSystemTypePersistent);
  if (proxy) {
    proxy->NotifyStorageAccessed(
        quota::QuotaClient::kFileSystem,
        origin_url,
        FileSystemTypeToQuotaStorageType(type));
  }
}

void SandboxMountPointProvider::UpdateOriginUsageOnFileThread(
    QuotaManagerProxy* proxy, const GURL& origin_url,
    fileapi::FileSystemType type, int64 delta) {
  DCHECK(type == kFileSystemTypeTemporary ||
         type == kFileSystemTypePersistent);
  FilePath usage_file_path = GetUsageCachePathForOriginAndType(
      origin_url, type);
  DCHECK(!usage_file_path.empty());
  // TODO(dmikurbe): Make sure that usage_file_path is available.
  FileSystemUsageCache::AtomicUpdateUsageByDelta(usage_file_path, delta);
  if (proxy) {
    proxy->NotifyStorageModified(
        quota::QuotaClient::kFileSystem,
        origin_url,
        FileSystemTypeToQuotaStorageType(type),
        delta);
  }
}

void SandboxMountPointProvider::StartUpdateOriginOnFileThread(
    const GURL& origin_url, fileapi::FileSystemType type) {
  DCHECK(type == kFileSystemTypeTemporary ||
         type == kFileSystemTypePersistent);
  FilePath usage_file_path = GetUsageCachePathForOriginAndType(
      origin_url, type);
  FileSystemUsageCache::IncrementDirty(usage_file_path);
}

void SandboxMountPointProvider::EndUpdateOriginOnFileThread(
    const GURL& origin_url, fileapi::FileSystemType type) {
  DCHECK(type == kFileSystemTypeTemporary ||
         type == kFileSystemTypePersistent);
  FilePath usage_file_path = GetUsageCachePathForOriginAndType(
      origin_url, type);
  FileSystemUsageCache::DecrementDirty(usage_file_path);
}

void SandboxMountPointProvider::InvalidateUsageCache(
    const GURL& origin_url, fileapi::FileSystemType type) {
  DCHECK(type == kFileSystemTypeTemporary ||
         type == kFileSystemTypePersistent);
  FilePath usage_file_path = GetUsageCachePathForOriginAndType(
      origin_url, type);
  FileSystemUsageCache::IncrementDirty(usage_file_path);
}

FilePath SandboxMountPointProvider::GetUsageCachePathForOriginAndType(
    const GURL& origin_url, fileapi::FileSystemType type) const {
  FilePath base_path =
      GetBaseDirectoryForOriginAndType(origin_url, type, false);
  if (base_path.empty())
    return FilePath();
  return base_path.Append(FileSystemUsageCache::kUsageFileName);
}

FilePath SandboxMountPointProvider::OldCreateFileSystemRootPath(
    const GURL& origin_url, fileapi::FileSystemType type) {
  FilePath origin_base_path =
      OldGetBaseDirectoryForOriginAndType(old_base_path(), origin_url, type);
  DCHECK(!origin_base_path.empty());

  FilePath root;
  base::PlatformFileError result =
      OldReadOriginDirectory(origin_base_path, &root);
  if (base::PLATFORM_FILE_OK == result)
    return root;

  // We found more than on filesystem there already--we don't know how to
  // recover from this.
  if (base::PLATFORM_FILE_ERROR_NOT_FOUND != result)
    return FilePath();

  // Creates the root directory.
  root = origin_base_path.Append(OldCreateUniqueDirectoryName(origin_url));
  if (!file_util::CreateDirectory(root))
    return FilePath();

  return root;
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

}  // namespace fileapi
