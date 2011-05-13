// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/sandbox_mount_point_provider.h"

#include "base/logging.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/glue/webkit_glue.h"

namespace {

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

// TODO(kinuko): Merge these two methods (conversion methods between
// origin url <==> identifier) with the ones in the database module.
std::string GetOriginIdentifierFromURL(const GURL& url) {
  WebKit::WebSecurityOrigin web_security_origin =
      WebKit::WebSecurityOrigin::createFromString(UTF8ToUTF16(url.spec()));
  return web_security_origin.databaseIdentifier().utf8();
}

GURL GetOriginURLFromIdentifier(const std::string& origin_identifier) {
  WebKit::WebSecurityOrigin web_security_origin =
      WebKit::WebSecurityOrigin::createFromDatabaseIdentifier(
          UTF8ToUTF16(origin_identifier));
  GURL origin_url(web_security_origin.toString());

  // We need this work-around for file:/// URIs as
  // createFromDatabaseIdentifier returns empty origin_url for them.
  if (origin_url.spec().empty() &&
      origin_identifier.find("file__") == 0)
    return GURL("file:///");
  return origin_url;
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
    const GURL& origin_url, const FilePath &origin_base_path, bool create) {
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
    return GetOriginURLFromIdentifier(
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
    : path_manager_(path_manager),
      file_message_loop_(file_message_loop),
      base_path_(profile_path.Append(kFileSystemDirectory)),
      sandbox_file_util_(new ObfuscatedFileSystemFileUtil(base_path_)) {
}

SandboxMountPointProvider::~SandboxMountPointProvider() {
  if (!file_message_loop_->BelongsToCurrentThread())
    file_message_loop_->DeleteSoon(FROM_HERE, sandbox_file_util_.release());
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
      const std::string& name,
      FileSystemPathManager::GetRootPathCallback* callback)
      : file_message_loop_(file_message_loop),
        origin_message_loop_proxy_(
            base::MessageLoopProxy::CreateForCurrentThread()),
        name_(name),
        callback_(callback) {
  }

  void Start(const GURL& origin_url,
             const FilePath& origin_base_path,
             bool create) {
    file_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &GetFileSystemRootPathTask::GetFileSystemRootPathOnFileThread,
        origin_url, origin_base_path, create));
  }

 private:
  void GetFileSystemRootPathOnFileThread(
      const GURL& origin_url,
      const FilePath& origin_base_path,
      bool create) {
    DispatchCallbackOnCallerThread(
        GetFileSystemRootPathOnFileThreadHelper(
            origin_url, origin_base_path, create));
  }

  void DispatchCallbackOnCallerThread(const FilePath& root_path) {
    origin_message_loop_proxy_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &GetFileSystemRootPathTask::DispatchCallback,
                          root_path));
  }

  void DispatchCallback(const FilePath& root_path) {
    callback_->Run(!root_path.empty(), root_path, name_);
    callback_.reset();
  }

  scoped_refptr<base::MessageLoopProxy> file_message_loop_;
  scoped_refptr<base::MessageLoopProxy> origin_message_loop_proxy_;
  std::string name_;
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
  return new SandboxOriginEnumerator(base_path_);
}

void SandboxMountPointProvider::ValidateFileSystemRootAndGetURL(
    const GURL& origin_url, fileapi::FileSystemType type,
    bool create, FileSystemPathManager::GetRootPathCallback* callback_ptr) {
  scoped_ptr<FileSystemPathManager::GetRootPathCallback> callback(callback_ptr);
  std::string name;
  FilePath origin_base_path;

  if (!GetOriginBasePathAndName(origin_url, &origin_base_path, type, &name)) {
    callback->Run(false, FilePath(), std::string());
    return;
  }

  scoped_refptr<GetFileSystemRootPathTask> task(
      new GetFileSystemRootPathTask(file_message_loop_,
                                    name,
                                    callback.release()));
  task->Start(origin_url, origin_base_path, create);
};

FilePath
SandboxMountPointProvider::ValidateFileSystemRootAndGetPathOnFileThread(
    const GURL& origin_url, FileSystemType type, const FilePath& unused,
    bool create) {
  FilePath origin_base_path;
  if (!GetOriginBasePathAndName(origin_url, &origin_base_path, type, NULL)) {
    return FilePath();
  }
  return GetFileSystemRootPathOnFileThreadHelper(
      origin_url, origin_base_path, create);
}

FilePath SandboxMountPointProvider::GetBaseDirectoryForOrigin(
    const GURL& origin_url) const {
  return base_path_.AppendASCII(GetOriginIdentifierFromURL(origin_url));
}

FilePath SandboxMountPointProvider::GetBaseDirectoryForOriginAndType(
    const GURL& origin_url, fileapi::FileSystemType type) const {
  std::string type_string =
      FileSystemPathManager::GetFileSystemTypeString(type);
  if (type_string.empty()) {
    LOG(WARNING) << "Unknown filesystem type is requested:" << type;
    return FilePath();
  }
  return GetBaseDirectoryForOrigin(origin_url).AppendASCII(type_string);
}

bool SandboxMountPointProvider::GetOriginBasePathAndName(
    const GURL& origin_url,
    FilePath* origin_base_path,
    FileSystemType type,
    std::string* name) {

  if (path_manager_->is_incognito())
    // TODO(kinuko): return an isolated temporary directory.
    return false;

  if (!path_manager_->IsAllowedScheme(origin_url))
    return false;

  *origin_base_path = GetBaseDirectoryForOriginAndType(origin_url, type);
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

}  // namespace fileapi
