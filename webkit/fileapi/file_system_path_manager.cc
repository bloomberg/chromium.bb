// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_path_manager.h"

#include "base/rand_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_callback_factory.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "webkit/glue/webkit_glue.h"

// We use some of WebKit types for conversions between origin identifiers
// and origin URLs.
using WebKit::WebFileSystem;
using WebKit::WebSecurityOrigin;
using WebKit::WebString;

using base::PlatformFileError;

namespace fileapi {

const FilePath::CharType FileSystemPathManager::kFileSystemDirectory[] =
    FILE_PATH_LITERAL("FileSystem");

const char FileSystemPathManager::kPersistentName[] = "Persistent";
const char FileSystemPathManager::kTemporaryName[] = "Temporary";

static const FilePath::CharType kFileSystemUniqueNamePrefix[] =
    FILE_PATH_LITERAL("chrome-");
static const int kFileSystemUniqueLength = 16;
static const unsigned kFileSystemUniqueDirectoryNameLength =
    kFileSystemUniqueLength + arraysize(kFileSystemUniqueNamePrefix) - 1;

namespace {

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

static const char kExtensionScheme[] = "chrome-extension";

}  // anonymous namespace

class FileSystemPathManager::GetFileSystemRootPathTask
    : public base::RefCountedThreadSafe<
        FileSystemPathManager::GetFileSystemRootPathTask> {
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
      const FilePath& base_path,
      bool create) {
    FilePath root;
    if (ReadOriginDirectory(base_path, origin_url, &root)) {
      DispatchCallbackOnCallerThread(root);
      return;
    }

    if (!create) {
      DispatchCallbackOnCallerThread(FilePath());
      return;
    }

    // Creates the root directory.
    root = base_path.Append(CreateUniqueDirectoryName(origin_url));
    if (!file_util::CreateDirectory(root)) {
      DispatchCallbackOnCallerThread(FilePath());
      return;
    }
    DispatchCallbackOnCallerThread(root);
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

FileSystemPathManager::FileSystemPathManager(
    scoped_refptr<base::MessageLoopProxy> file_message_loop,
    const FilePath& profile_path,
    bool is_incognito,
    bool allow_file_access_from_files)
    : file_message_loop_(file_message_loop),
      base_path_(profile_path.Append(kFileSystemDirectory)),
      is_incognito_(is_incognito),
      allow_file_access_from_files_(allow_file_access_from_files) {
}

FileSystemPathManager::~FileSystemPathManager() {}

void FileSystemPathManager::GetFileSystemRootPath(
    const GURL& origin_url, fileapi::FileSystemType type,
    bool create, GetRootPathCallback* callback_ptr) {
  scoped_ptr<GetRootPathCallback> callback(callback_ptr);
  if (is_incognito_) {
    // TODO(kinuko): return an isolated temporary directory.
    callback->Run(false, FilePath(), std::string());
    return;
  }

  if (!IsAllowedScheme(origin_url)) {
    callback->Run(false, FilePath(), std::string());
    return;
  }

  std::string origin_identifier = GetOriginIdentifierFromURL(origin_url);
  FilePath origin_base_path = GetFileSystemBaseDirectoryForOriginAndType(
      base_path(), origin_identifier, type);
  if (origin_base_path.empty()) {
    callback->Run(false, FilePath(), std::string());
    return;
  }

  std::string type_string = GetFileSystemTypeString(type);
  DCHECK(!type_string.empty());

  scoped_refptr<GetFileSystemRootPathTask> task(
      new GetFileSystemRootPathTask(file_message_loop_,
                                    origin_identifier + ":" + type_string,
                                    callback.release()));
  task->Start(origin_url, origin_base_path, create);
}

bool FileSystemPathManager::CrackFileSystemPath(
    const FilePath& path, GURL* origin_url, FileSystemType* type,
    FilePath* virtual_path) const {
  // Any paths that includes parent references are considered invalid.
  if (path.ReferencesParent())
    return false;

  // The path should be a child of the profile FileSystem path.
  FilePath relative;
  if (!base_path_.AppendRelativePath(path, &relative))
    return false;

  // The relative path from the profile FileSystem path should contain
  // at least three components, one for storage identifier, one for type
  // and one for the 'unique' part.
  std::vector<FilePath::StringType> components;
  relative.GetComponents(&components);
  if (components.size() < 3)
    return false;

  // The second component of the relative path to the root directory
  // must be kPersistent or kTemporary.
  if (!IsStringASCII(components[1]))
    return false;

  std::string ascii_type_component = FilePathStringToASCII(components[1]);
  FileSystemType cracked_type = kFileSystemTypeUnknown;
  if (ascii_type_component == kPersistentName)
    cracked_type = kFileSystemTypePersistent;
  else if (ascii_type_component == kTemporaryName)
    cracked_type = kFileSystemTypeTemporary;
  else
    return false;

  DCHECK(cracked_type != kFileSystemTypeUnknown);

  // The given |path| seems valid. Populates the |origin_url|, |type|
  // and |virtual_path| if they are given.

  if (origin_url) {
    WebSecurityOrigin web_security_origin =
        WebSecurityOrigin::createFromDatabaseIdentifier(
            webkit_glue::FilePathStringToWebString(components[0]));
    *origin_url = GURL(web_security_origin.toString());

    // We need this work-around for file:/// URIs as
    // createFromDatabaseIdentifier returns empty origin_url for them.
    if (allow_file_access_from_files_ && origin_url->spec().empty() &&
        components[0].find(FILE_PATH_LITERAL("file")) == 0)
      *origin_url = GURL("file:///");
  }

  if (type)
    *type = cracked_type;

  if (virtual_path) {
    virtual_path->clear();
    for (size_t i = 3; i < components.size(); ++i)
      *virtual_path = virtual_path->Append(components[i]);
  }

  return true;
}

bool FileSystemPathManager::IsAllowedScheme(const GURL& url) const {
  // Basically we only accept http or https. We allow file:// URLs
  // only if --allow-file-access-from-files flag is given.
  return url.SchemeIs("http") || url.SchemeIs("https") ||
         url.SchemeIs(kExtensionScheme) ||
         (url.SchemeIsFile() && allow_file_access_from_files_);
}

// static
bool FileSystemPathManager::IsRestrictedFileName(const FilePath& filename) {
  if (filename.value().size() == 0)
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

// static
std::string FileSystemPathManager::GetFileSystemTypeString(
    fileapi::FileSystemType type) {
  if (type == fileapi::kFileSystemTypeTemporary)
    return fileapi::FileSystemPathManager::kTemporaryName;
  else if (type == fileapi::kFileSystemTypePersistent)
    return fileapi::FileSystemPathManager::kPersistentName;
  return std::string();
}

// static
std::string FileSystemPathManager::GetOriginIdentifierFromURL(
    const GURL& url) {
  WebKit::WebSecurityOrigin web_security_origin =
      WebKit::WebSecurityOrigin::createFromString(UTF8ToUTF16(url.spec()));
  return web_security_origin.databaseIdentifier().utf8();
}

// static
FilePath FileSystemPathManager::GetFileSystemBaseDirectoryForOriginAndType(
    const FilePath& base_path, const std::string& origin_identifier,
    fileapi::FileSystemType type) {
  if (origin_identifier.empty())
    return FilePath();
  std::string type_string = GetFileSystemTypeString(type);
  if (type_string.empty()) {
    LOG(WARNING) << "Unknown filesystem type is requested:" << type;
    return FilePath();
  }
  return base_path.AppendASCII(origin_identifier)
                  .AppendASCII(type_string);
}

FileSystemPathManager::OriginEnumerator::OriginEnumerator(
    const FilePath& base_path)
    : enumerator_(base_path, false /* recursive */,
                  file_util::FileEnumerator::DIRECTORIES) {
}

std::string FileSystemPathManager::OriginEnumerator::Next() {
  current_ = enumerator_.Next();
  return FilePathStringToASCII(current_.BaseName().value());
}

bool FileSystemPathManager::OriginEnumerator::HasTemporary() {
  return !current_.empty() && file_util::DirectoryExists(current_.AppendASCII(
      FileSystemPathManager::kTemporaryName));
}

bool FileSystemPathManager::OriginEnumerator::HasPersistent() {
  return !current_.empty() && file_util::DirectoryExists(current_.AppendASCII(
      FileSystemPathManager::kPersistentName));
}

}  // namespace fileapi

COMPILE_ASSERT(int(WebFileSystem::TypeTemporary) == \
               int(fileapi::kFileSystemTypeTemporary), mismatching_enums);
COMPILE_ASSERT(int(WebFileSystem::TypePersistent) == \
               int(fileapi::kFileSystemTypePersistent), mismatching_enums);
