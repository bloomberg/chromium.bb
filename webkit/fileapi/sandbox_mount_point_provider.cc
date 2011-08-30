// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/sandbox_mount_point_provider.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/local_file_system_file_util.h"
#include "webkit/fileapi/obfuscated_file_system_file_util.h"
#include "webkit/fileapi/quota_file_util.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/quota/quota_manager.h"

using quota::QuotaManagerProxy;

namespace {

static const FilePath::CharType kOldFileSystemUniqueNamePrefix[] =
    FILE_PATH_LITERAL("chrome-");
static const int kOldFileSystemUniqueLength = 16;
static const unsigned kOldFileSystemUniqueDirectoryNameLength =
    kOldFileSystemUniqueLength + arraysize(kOldFileSystemUniqueNamePrefix) - 1;

// Restricted names.
// http://dev.w3.org/2009/dap/file-system/file-dir-sys.html#naming-restrictions
static const char* const kRestrictedNames[] = {
  ".", "..",
};

// Restricted chars.
static const FilePath::CharType kRestrictedChars[] = {
  '/', '\\',
};

inline std::string FilePathStringToASCII(
    const FilePath::StringType& path_string) {
#if defined(OS_WIN)
  return WideToASCII(path_string);
#elif defined(OS_POSIX)
  return path_string;
#endif
}

FilePath::StringType OldCreateUniqueDirectoryName(const GURL& origin_url) {
  // This can be anything but need to be unpredictable.
  static const FilePath::CharType letters[] = FILE_PATH_LITERAL(
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
  FilePath::StringType unique(kOldFileSystemUniqueNamePrefix);
  for (int i = 0; i < kOldFileSystemUniqueLength; ++i)
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
    : public fileapi::SandboxMountPointProvider::OriginEnumerator {
 public:
  explicit ObfuscatedOriginEnumerator(
      fileapi::ObfuscatedFileSystemFileUtil* file_util) {
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
  scoped_ptr<fileapi::ObfuscatedFileSystemFileUtil::AbstractOriginEnumerator>
      enum_;
};

class OldSandboxOriginEnumerator
    : public fileapi::SandboxMountPointProvider::OriginEnumerator {
 public:
  explicit OldSandboxOriginEnumerator(const FilePath& base_path)
      : enumerator_(base_path, false /* recursive */,
                    file_util::FileEnumerator::DIRECTORIES) {}
  virtual ~OldSandboxOriginEnumerator() {}

  virtual GURL Next() OVERRIDE {
    current_ = enumerator_.Next();
    if (current_.empty())
      return GURL();
    return fileapi::GetOriginURLFromIdentifier(
        FilePathStringToASCII(current_.BaseName().value()));
  }

  virtual bool HasFileSystemType(fileapi::FileSystemType type) const OVERRIDE {
    if (current_.empty())
      return false;
    std::string directory =
        fileapi::FileSystemPathManager::GetFileSystemTypeString(type);
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
  std::string id = fileapi::GetOriginIdentifierFromURL(origin_url);
  if (!id.empty())
    return old_base_path.AppendASCII(id);
  return FilePath();
}

FilePath OldGetBaseDirectoryForOriginAndType(
    const FilePath& old_base_path,
    const GURL& origin_url, fileapi::FileSystemType type) {
  std::string type_string =
      fileapi::FileSystemPathManager::GetFileSystemTypeString(type);
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
    fileapi::ObfuscatedFileSystemFileUtil* file_util,
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
    fileapi::ObfuscatedFileSystemFileUtil* file_util,
    const FilePath& old_base_path) {
  scoped_ptr<OldSandboxOriginEnumerator> old_origins(
      new OldSandboxOriginEnumerator(old_base_path));
  GURL origin;
  int failures = 0;
  while (!(origin = old_origins->Next()).is_empty()) {
    int failures_this_origin = 0;
    if (old_origins->HasFileSystemType(fileapi::kFileSystemTypeTemporary) &&
        !MigrateOneOldFileSystem(
            file_util, old_base_path, origin,
            fileapi::kFileSystemTypeTemporary))
      ++failures_this_origin;
    if (old_origins->HasFileSystemType(fileapi::kFileSystemTypePersistent) &&
        !MigrateOneOldFileSystem(
            file_util, old_base_path, origin,
            fileapi::kFileSystemTypePersistent))
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
            fileapi::SandboxMountPointProvider::kRenamedOldFileSystemDirectory);
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
    fileapi::ObfuscatedFileSystemFileUtil* file_util,
    const FilePath& old_base_path) {
  if (file_util::DirectoryExists(old_base_path))
    MigrateAllOldFileSystems(file_util, old_base_path);
}

}  // anonymous namespace

namespace fileapi {

const FilePath::CharType SandboxMountPointProvider::kOldFileSystemDirectory[] =
    FILE_PATH_LITERAL("FileSystem");

const FilePath::CharType SandboxMountPointProvider::kNewFileSystemDirectory[] =
    FILE_PATH_LITERAL("File System");

const FilePath::CharType
    SandboxMountPointProvider::kRenamedOldFileSystemDirectory[] =
        FILE_PATH_LITERAL("FS.old");

SandboxMountPointProvider::SandboxMountPointProvider(
    FileSystemPathManager* path_manager,
    scoped_refptr<base::MessageLoopProxy> file_message_loop,
    const FilePath& profile_path)
    : FileSystemQuotaUtil(file_message_loop),
      path_manager_(path_manager),
      file_message_loop_(file_message_loop),
      profile_path_(profile_path),
      sandbox_file_util_(
          new ObfuscatedFileSystemFileUtil(
              profile_path.Append(kNewFileSystemDirectory),
              QuotaFileUtil::CreateDefault())) {
}

SandboxMountPointProvider::~SandboxMountPointProvider() {
  if (!file_message_loop_->BelongsToCurrentThread())
    file_message_loop_->ReleaseSoon(FROM_HERE, sandbox_file_util_.release());
}

bool SandboxMountPointProvider::IsAccessAllowed(const GURL& origin_url,
                                                FileSystemType type,
                                                const FilePath& unused) {
  if (type != kFileSystemTypeTemporary && type != kFileSystemTypePersistent)
    return false;
  // We essentially depend on quota to do our access controls.
  return path_manager_->IsAllowedScheme(origin_url);
}

class SandboxMountPointProvider::GetFileSystemRootPathTask
    : public base::RefCountedThreadSafe<
        SandboxMountPointProvider::GetFileSystemRootPathTask> {
 public:
  GetFileSystemRootPathTask(
      scoped_refptr<base::MessageLoopProxy> file_message_loop,
      const GURL& origin_url,
      FileSystemType type,
      ObfuscatedFileSystemFileUtil* file_util,
      const FilePath& old_base_path,
      FileSystemPathManager::GetRootPathCallback* callback)
      : file_message_loop_(file_message_loop),
        origin_message_loop_proxy_(
            base::MessageLoopProxy::current()),
        origin_url_(origin_url),
        type_(type),
        file_util_(file_util),
        old_base_path_(old_base_path),
        callback_(callback) {
  }

  virtual ~GetFileSystemRootPathTask() {
    // Just in case we get deleted without running, make sure to clean up the
    // file_util_ on the right thread.
    if (file_util_.get() && !file_message_loop_->BelongsToCurrentThread())
      file_message_loop_->ReleaseSoon(FROM_HERE, file_util_.release());
  }

  void Start(bool create) {
    file_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &GetFileSystemRootPathTask::GetFileSystemRootPathOnFileThread, create));
  }

 private:
  void GetFileSystemRootPathOnFileThread(bool create) {
    MigrateIfNeeded(file_util_, old_base_path_);
    DispatchCallbackOnCallerThread(
        file_util_->GetDirectoryForOriginAndType(origin_url_, type_, create));
    // We must clear the reference on the file thread.
    file_util_ = NULL;
  }

  void DispatchCallbackOnCallerThread(const FilePath& root_path) {
    origin_message_loop_proxy_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &GetFileSystemRootPathTask::DispatchCallback,
                          root_path));
  }

  void DispatchCallback(const FilePath& root_path) {
    std::string origin_identifier = GetOriginIdentifierFromURL(origin_url_);
    std::string type_string =
        FileSystemPathManager::GetFileSystemTypeString(type_);
    DCHECK(!type_string.empty());
    std::string name = origin_identifier + ":" + type_string;
    callback_->Run(!root_path.empty(), root_path, name);
    callback_.reset();
  }

  scoped_refptr<base::MessageLoopProxy> file_message_loop_;
  scoped_refptr<base::MessageLoopProxy> origin_message_loop_proxy_;
  GURL origin_url_;
  FileSystemType type_;
  scoped_refptr<ObfuscatedFileSystemFileUtil> file_util_;
  FilePath old_base_path_;
  scoped_ptr<FileSystemPathManager::GetRootPathCallback> callback_;
};

FilePath SandboxMountPointProvider::old_base_path() const {
  return profile_path_.Append(kOldFileSystemDirectory);
}

FilePath SandboxMountPointProvider::new_base_path() const {
  return profile_path_.Append(kNewFileSystemDirectory);
}

FilePath SandboxMountPointProvider::renamed_old_base_path() const {
  return profile_path_.Append(kRenamedOldFileSystemDirectory);
}

bool SandboxMountPointProvider::IsRestrictedFileName(const FilePath& filename)
    const {
  if (filename.value().empty())
    return false;

  std::string filename_lower = StringToLowerASCII(
      FilePathStringToASCII(filename.value()));

  for (size_t i = 0; i < arraysize(kRestrictedNames); ++i) {
    // Exact match.
    if (filename_lower == kRestrictedNames[i])
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

SandboxMountPointProvider::OriginEnumerator*
SandboxMountPointProvider::CreateOriginEnumerator() const {
  MigrateIfNeeded(sandbox_file_util_, old_base_path());
  return new ObfuscatedOriginEnumerator(sandbox_file_util_.get());
}

void SandboxMountPointProvider::ValidateFileSystemRootAndGetURL(
    const GURL& origin_url, fileapi::FileSystemType type,
    bool create, FileSystemPathManager::GetRootPathCallback* callback_ptr) {
  scoped_ptr<FileSystemPathManager::GetRootPathCallback> callback(callback_ptr);
  FilePath origin_base_path;

  if (path_manager_->is_incognito()) {
    // TODO(kinuko): return an isolated temporary directory.
    callback->Run(false, FilePath(), std::string());
    return;
  }

  if (!path_manager_->IsAllowedScheme(origin_url)) {
    callback->Run(false, FilePath(), std::string());
    return;
  }

  scoped_refptr<GetFileSystemRootPathTask> task(
      new GetFileSystemRootPathTask(file_message_loop_,
                                    origin_url,
                                    type,
                                    sandbox_file_util_.get(),
                                    old_base_path(),
                                    callback.release()));
  task->Start(create);
};

FilePath
SandboxMountPointProvider::ValidateFileSystemRootAndGetPathOnFileThread(
    const GURL& origin_url, FileSystemType type, const FilePath& unused,
    bool create) {
  if (path_manager_->is_incognito())
    // TODO(kinuko): return an isolated temporary directory.
    return FilePath();

  if (!path_manager_->IsAllowedScheme(origin_url))
    return FilePath();

  MigrateIfNeeded(sandbox_file_util_, old_base_path());

  return sandbox_file_util_->GetDirectoryForOriginAndType(
      origin_url, type, create);
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
  DCHECK(type == fileapi::kFileSystemTypeTemporary ||
         type == fileapi::kFileSystemTypePersistent);
  DCHECK(origins);
  scoped_ptr<OriginEnumerator> enumerator(CreateOriginEnumerator());
  GURL origin;
  while (!(origin = enumerator->Next()).is_empty()) {
    if (enumerator->HasFileSystemType(type))
      origins->insert(origin);
  }
}

void SandboxMountPointProvider::GetOriginsForHostOnFileThread(
    fileapi::FileSystemType type, const std::string& host,
    std::set<GURL>* origins) {
  DCHECK(type == fileapi::kFileSystemTypeTemporary ||
         type == fileapi::kFileSystemTypePersistent);
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
  DCHECK(type == fileapi::kFileSystemTypeTemporary ||
         type == fileapi::kFileSystemTypePersistent);
  FilePath base_path =
      GetBaseDirectoryForOriginAndType(origin_url, type, false);
  if (base_path.empty() || !file_util::DirectoryExists(base_path)) return 0;
  FilePath usage_file_path =
      base_path.AppendASCII(FileSystemUsageCache::kUsageFileName);

  int32 dirty_status = FileSystemUsageCache::GetDirty(usage_file_path);
  bool visited = (visited_origins_.find(origin_url) != visited_origins_.end());
  visited_origins_.insert(origin_url);
  if (dirty_status == 0 || (dirty_status > 0 && visited)) {
    // The usage cache is clean (dirty == 0) or the origin is already
    // initialized and running.  Read the cache file to get the usage.
    return FileSystemUsageCache::GetUsage(usage_file_path);
  }
  // The usage cache has not been initialized or the cache is dirty.
  // Get the directory size now and update the cache.
  FileSystemUsageCache::Delete(usage_file_path);

  FileSystemOperationContext context(NULL, sandbox_file_util_);
  context.set_src_origin_url(origin_url);
  context.set_src_type(type);
  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> enumerator(
      sandbox_file_util_->CreateFileEnumerator(&context, FilePath()));

  FilePath file_path_each;
  int64 usage = 0;

  while (!(file_path_each = enumerator->Next()).empty()) {
    base::PlatformFileInfo file_info;
    FilePath platform_file_path;
    usage += enumerator->Size();
    usage += ObfuscatedFileSystemFileUtil::ComputeFilePathCost(file_path_each);
  }
  // This clears the dirty flag too.
  FileSystemUsageCache::UpdateUsage(usage_file_path, usage);
  return usage;
}

void SandboxMountPointProvider::NotifyOriginWasAccessedOnIOThread(
    QuotaManagerProxy* proxy, const GURL& origin_url,
    fileapi::FileSystemType type) {
  DCHECK(type == fileapi::kFileSystemTypeTemporary ||
         type == fileapi::kFileSystemTypePersistent);
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
  DCHECK(type == fileapi::kFileSystemTypeTemporary ||
         type == fileapi::kFileSystemTypePersistent);
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
  DCHECK(type == fileapi::kFileSystemTypeTemporary ||
         type == fileapi::kFileSystemTypePersistent);
  FilePath usage_file_path = GetUsageCachePathForOriginAndType(
      origin_url, type);
  FileSystemUsageCache::IncrementDirty(usage_file_path);
}

void SandboxMountPointProvider::EndUpdateOriginOnFileThread(
    const GURL& origin_url, fileapi::FileSystemType type) {
  DCHECK(type == fileapi::kFileSystemTypeTemporary ||
         type == fileapi::kFileSystemTypePersistent);
  FilePath usage_file_path = GetUsageCachePathForOriginAndType(
      origin_url, type);
  FileSystemUsageCache::DecrementDirty(usage_file_path);
}

FileSystemFileUtil* SandboxMountPointProvider::GetFileSystemFileUtil() {
  return sandbox_file_util_.get();
}

FilePath SandboxMountPointProvider::GetUsageCachePathForOriginAndType(
    const GURL& origin_url, fileapi::FileSystemType type) const {
  FilePath base_path =
      GetBaseDirectoryForOriginAndType(origin_url, type, false);
  if (base_path.empty())
    return FilePath();
  return base_path.AppendASCII(FileSystemUsageCache::kUsageFileName);
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

}  // namespace fileapi
