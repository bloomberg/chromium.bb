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
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/local_file_system_file_util.h"
#include "webkit/fileapi/obfuscated_file_system_file_util.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/quota/quota_manager.h"

using quota::QuotaManagerProxy;

namespace {

static const char kObfuscationFlag[] = "use-obfuscated-file-system";

static const FilePath::CharType kFileSystemUniqueNamePrefix[] =
    FILE_PATH_LITERAL("chrome-");
static const int kFileSystemUniqueLength = 16;
static const unsigned kFileSystemUniqueDirectoryNameLength =
    kFileSystemUniqueLength + arraysize(kFileSystemUniqueNamePrefix) - 1;

// Restricted names.
// http://dev.w3.org/2009/dap/file-system/file-dir-sys.html#naming-restrictions
static const char* const kRestrictedNames[] = {
  "con", "prn", "aux", "nul",
  "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
  "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9",
};

// Restricted chars.
static const FilePath::CharType kRestrictedChars[] = {
  '/', '\\', '<', '>', ':', '?', '*', '"', '|',
};

inline std::string FilePathStringToASCII(
    const FilePath::StringType& path_string) {
#if defined(OS_WIN)
  return WideToASCII(path_string);
#elif defined(OS_POSIX)
  return path_string;
#endif
}

FilePath::StringType CreateUniqueDirectoryName(const GURL& origin_url) {
  // This can be anything but need to be unpredictable.
  static const FilePath::CharType letters[] = FILE_PATH_LITERAL(
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
  FilePath::StringType unique(kFileSystemUniqueNamePrefix);
  for (int i = 0; i < kFileSystemUniqueLength; ++i)
    unique += letters[base::RandInt(0, arraysize(letters) - 2)];
  return unique;
}

bool ReadOriginDirectory(const FilePath& base_path,
                         const GURL& origin_url,
                         FilePath* unique) {
  file_util::FileEnumerator file_enum(
      base_path, false /* recursive */,
      file_util::FileEnumerator::DIRECTORIES,
      FilePath::StringType(kFileSystemUniqueNamePrefix) +
          FILE_PATH_LITERAL("*"));
  FilePath current;
  bool found = false;
  while (!(current = file_enum.Next()).empty()) {
    if (current.BaseName().value().length() !=
        kFileSystemUniqueDirectoryNameLength)
      continue;
    if (found) {
      // TODO(kinuko): Should notify the user to ask for some action.
      LOG(WARNING) << "Unexpectedly found more than one FileSystem "
                   << "directories for " << origin_url;
      return false;
    }
    found = true;
    *unique = current;
  }
  return !unique->empty();
}

FilePath GetFileSystemRootPathOnFileThreadHelper(
    const GURL& origin_url, const FilePath& origin_base_path, bool create) {
  FilePath root;
  if (ReadOriginDirectory(origin_base_path, origin_url, &root))
    return root;

  if (!create)
    return FilePath();

  // Creates the root directory.
  root = origin_base_path.Append(CreateUniqueDirectoryName(origin_url));
  if (!file_util::CreateDirectory(root))
    return FilePath();

  return root;
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

class SandboxOriginEnumerator
    : public fileapi::SandboxMountPointProvider::OriginEnumerator {
 public:
  explicit SandboxOriginEnumerator(const FilePath& base_path)
      : enumerator_(base_path, false /* recursive */,
                    file_util::FileEnumerator::DIRECTORIES) {}
  virtual ~SandboxOriginEnumerator() {}

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

}  // anonymous namespace

namespace fileapi {

const FilePath::CharType SandboxMountPointProvider::kFileSystemDirectory[] =
    FILE_PATH_LITERAL("FileSystem");

SandboxMountPointProvider::SandboxMountPointProvider(
    FileSystemPathManager* path_manager,
    scoped_refptr<base::MessageLoopProxy> file_message_loop,
    const FilePath& profile_path)
    : FileSystemQuotaUtil(file_message_loop),
      path_manager_(path_manager),
      file_message_loop_(file_message_loop),
      base_path_(profile_path.Append(kFileSystemDirectory)),
      sandbox_file_util_(new ObfuscatedFileSystemFileUtil(base_path_)) {
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
      const FilePath& origin_base_path,
      FileSystemType type,
      ObfuscatedFileSystemFileUtil* file_util,
      FileSystemPathManager::GetRootPathCallback* callback)
      : file_message_loop_(file_message_loop),
        origin_message_loop_proxy_(
            base::MessageLoopProxy::CreateForCurrentThread()),
        origin_url_(origin_url),
        origin_base_path_(origin_base_path),
        type_(type),
        file_util_(file_util),
        callback_(callback) {
  }

  void Start(bool create) {
    file_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &GetFileSystemRootPathTask::GetFileSystemRootPathOnFileThread, create));
  }

 private:
  void GetFileSystemRootPathOnFileThread(
      bool create) {
    if (file_util_.get())
      DispatchCallbackOnCallerThread(
          file_util_->GetDirectoryForOriginAndType(origin_url_, type_, create));
    else
      DispatchCallbackOnCallerThread(
          GetFileSystemRootPathOnFileThreadHelper(
              origin_url_, origin_base_path_, create));
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
  FilePath origin_base_path_;
  FileSystemType type_;
  scoped_refptr<ObfuscatedFileSystemFileUtil> file_util_;
  scoped_ptr<FileSystemPathManager::GetRootPathCallback> callback_;
};

bool SandboxMountPointProvider::IsRestrictedFileName(const FilePath& filename)
    const {
  if (filename.value().empty())
    return false;

  if (IsWhitespace(filename.value()[filename.value().size() - 1]) ||
      filename.value()[filename.value().size() - 1] == '.')
    return true;

  std::string filename_lower = StringToLowerASCII(
      FilePathStringToASCII(filename.value()));

  for (size_t i = 0; i < arraysize(kRestrictedNames); ++i) {
    // Exact match.
    if (filename_lower == kRestrictedNames[i])
      return true;
    // Starts with "RESTRICTED_NAME.".
    if (filename_lower.find(std::string(kRestrictedNames[i]) + ".") == 0)
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
  // but we should switch over.
  return  std::vector<FilePath>();
}

SandboxMountPointProvider::OriginEnumerator*
SandboxMountPointProvider::CreateOriginEnumerator() const {
  if (CommandLine::ForCurrentProcess()->HasSwitch(kObfuscationFlag))
    return new ObfuscatedOriginEnumerator(sandbox_file_util_.get());
  return new SandboxOriginEnumerator(base_path_);
}

void SandboxMountPointProvider::ValidateFileSystemRootAndGetURL(
    const GURL& origin_url, fileapi::FileSystemType type,
    bool create, FileSystemPathManager::GetRootPathCallback* callback_ptr) {
  scoped_ptr<FileSystemPathManager::GetRootPathCallback> callback(callback_ptr);
  ObfuscatedFileSystemFileUtil* file_util = NULL;
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

  if (CommandLine::ForCurrentProcess()->HasSwitch(kObfuscationFlag)) {
    file_util = sandbox_file_util_.get();
  } else {
    std::string name;
    if (!GetOriginBasePathAndName(origin_url, &origin_base_path, type, &name)) {
      callback->Run(false, FilePath(), std::string());
      return;
    }
  }

  scoped_refptr<GetFileSystemRootPathTask> task(
      new GetFileSystemRootPathTask(file_message_loop_,
                                    origin_url,
                                    origin_base_path,
                                    type,
                                    file_util,
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

  if (CommandLine::ForCurrentProcess()->HasSwitch(kObfuscationFlag))
    return sandbox_file_util_->GetDirectoryForOriginAndType(
        origin_url, type, create);

  std::string name;
  FilePath origin_base_path;
  if (!GetOriginBasePathAndName(origin_url, &origin_base_path, type, &name))
    return FilePath();

  return GetFileSystemRootPathOnFileThreadHelper(
      origin_url, origin_base_path, create);
}

FilePath SandboxMountPointProvider::GetBaseDirectoryForOrigin(
    const GURL& origin_url, bool create) const {
  if (CommandLine::ForCurrentProcess()->HasSwitch(kObfuscationFlag))
    return sandbox_file_util_->GetDirectoryForOrigin(
      origin_url, create);
  return base_path_.AppendASCII(GetOriginIdentifierFromURL(origin_url));
}

// Needed for the old way of doing things.
FilePath SandboxMountPointProvider::GetBaseDirectoryForOriginAndType(
    const GURL& origin_url, fileapi::FileSystemType type, bool create) const {
  if (CommandLine::ForCurrentProcess()->HasSwitch(kObfuscationFlag))
    return sandbox_file_util_->GetDirectoryForOriginAndType(
      origin_url, type, create);
  std::string type_string =
      FileSystemPathManager::GetFileSystemTypeString(type);
  if (type_string.empty()) {
    LOG(WARNING) << "Unknown filesystem type is requested:" << type;
    return FilePath();
  }
  return GetBaseDirectoryForOrigin(origin_url, create).AppendASCII(type_string);
}

bool SandboxMountPointProvider::DeleteOriginDataOnFileThread(
    QuotaManagerProxy* proxy, const GURL& origin_url,
    fileapi::FileSystemType type) {
  FilePath path_for_origin =
      GetBaseDirectoryForOriginAndType(origin_url, type, false);
  if (!file_util::PathExists(path_for_origin))
    return true;

  int64 usage = GetOriginUsageOnFileThread(origin_url, type);
  bool result = file_util::Delete(path_for_origin, true /* recursive */);
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
  if (!file_util::DirectoryExists(base_path))
    return 0;

  FilePath usage_file_path = base_path.AppendASCII(
      FileSystemUsageCache::kUsageFileName);
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
  if (FileSystemUsageCache::Exists(usage_file_path))
    FileSystemUsageCache::Delete(usage_file_path);
  int64 usage = file_util::ComputeDirectorySize(base_path);
  // The result of ComputeDirectorySize does not include .usage file size.
  usage += FileSystemUsageCache::kUsageFileSize;
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
  if (CommandLine::ForCurrentProcess()->HasSwitch(kObfuscationFlag))
    return sandbox_file_util_.get();
  return LocalFileSystemFileUtil::GetInstance();
}

// Needed for the old way of doing things.
bool SandboxMountPointProvider::GetOriginBasePathAndName(
    const GURL& origin_url,
    FilePath* origin_base_path,
    FileSystemType type,
    std::string* name) {

  *origin_base_path = GetBaseDirectoryForOriginAndType(origin_url, type, false);
  if (origin_base_path->empty())
    return false;

  std::string origin_identifier = GetOriginIdentifierFromURL(origin_url);
  std::string type_string =
      FileSystemPathManager::GetFileSystemTypeString(type);
  DCHECK(!type_string.empty());
  if (name)
    *name = origin_identifier + ":" + type_string;
  return true;
}

FilePath SandboxMountPointProvider::GetUsageCachePathForOriginAndType(
    const GURL& origin_url, fileapi::FileSystemType type) const {
  FilePath base_path =
      GetBaseDirectoryForOriginAndType(origin_url, type, false);
  return base_path.AppendASCII(FileSystemUsageCache::kUsageFileName);
}

}  // namespace fileapi
