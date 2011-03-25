// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystem.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_CHROMEOS)
#include "webkit/chromeos/fileapi/cros_mount_point_provider.h"
#endif

// We use some of WebKit types for conversions between origin identifiers
// and origin URLs.
using WebKit::WebFileSystem;

using base::PlatformFileError;

static const char kExtensionScheme[] = "chrome-extension";

namespace fileapi {

FileSystemPathManager::FileSystemPathManager(
    scoped_refptr<base::MessageLoopProxy> file_message_loop,
    const FilePath& profile_path,
    scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy,
    bool is_incognito,
    bool allow_file_access_from_files)
    : is_incognito_(is_incognito),
      allow_file_access_from_files_(allow_file_access_from_files),
      sandbox_provider_(
          new SandboxMountPointProvider(
              ALLOW_THIS_IN_INITIALIZER_LIST(this),
              file_message_loop,
              profile_path)) {
#if defined(OS_CHROMEOS)
  local_provider_.reset(
      new chromeos::CrosMountPointProvider(special_storage_policy));
#endif
}

FileSystemPathManager::~FileSystemPathManager() {}

void FileSystemPathManager::GetFileSystemRootPath(
    const GURL& origin_url, fileapi::FileSystemType type,
    bool create, GetRootPathCallback* callback_ptr) {

  switch (type) {
  case kFileSystemTypeTemporary:
  case kFileSystemTypePersistent:
    sandbox_provider_->GetFileSystemRootPath(
        origin_url, type, create, callback_ptr);
    break;
  case kFileSystemTypeLocal:
    if (local_provider_.get()) {
      local_provider_->GetFileSystemRootPath(
          origin_url, type, create, callback_ptr);
    } else {
      callback_ptr->Run(false, FilePath(), std::string());
    }
    break;
  case kFileSystemTypeUnknown:
  default:
    NOTREACHED();
    callback_ptr->Run(false, FilePath(), std::string());
  }
}

FilePath FileSystemPathManager::GetFileSystemRootPathOnFileThread(
    const GURL& origin_url, FileSystemType type, const FilePath& virtual_path,
    bool create) {
  switch (type) {
  case kFileSystemTypeTemporary:
  case kFileSystemTypePersistent:
    return sandbox_provider_->GetFileSystemRootPathOnFileThread(
        origin_url, type, virtual_path, create);
    break;
  case kFileSystemTypeLocal:
    return local_provider_.get() ?
        local_provider_->GetFileSystemRootPathOnFileThread(
           origin_url, type, virtual_path, create) :
        FilePath();
  case kFileSystemTypeUnknown:
  default:
    NOTREACHED();
    return FilePath();
  }
}

bool FileSystemPathManager::CrackFileSystemPath(
    const FilePath& path, GURL* origin_url, FileSystemType* type,
    FilePath* virtual_path) const {
  // TODO(ericu):
  // Paths come in here [for now] as a URL, followed by a virtual path in
  // platform format.  For example, on Windows, this will look like
  // filesystem:http://www.example.com/temporary/\path\to\file.txt.
  // A potentially dangerous malicious path on Windows might look like:
  // filesystem:http://www.example.com/temporary/foo/../../\path\to\file.txt.
  // This code is ugly, but will get cleaned up as we fix the calling side.
  // Eventually there won't be a distinction between a filesystem path and a
  // filesystem URL--they'll all be URLs.
  // We should be passing these to WebKit as string, not FilePath, for ease of
  // manipulation, or possibly as GURL/KURL.

  std::string path_as_string;
#ifdef OS_WIN
  path_as_string = WideToUTF8(path.value());
#else
  path_as_string = path.value();
#endif
  GURL path_as_url(path_as_string);

  FilePath local_path;
  GURL local_url;
  FileSystemType local_type;
  if (!CrackFileSystemURL(path_as_url, &local_url, &local_type, &local_path))
    return false;

#if defined(FILE_PATH_USES_WIN_SEPARATORS)
  // TODO(ericu): This puts the separators back to windows-standard; they come
  // out of the above code as '/' no matter the platform.  Long-term, we'll
  // want to let the underlying FileSystemFileUtil implementation do this part,
  // since they won't all need it.
  local_path = local_path.NormalizeWindowsPathSeparators();
#endif

  // Check if file access to this type of file system is allowed
  // for this origin.
  switch (local_type) {
    case kFileSystemTypeTemporary:
    case kFileSystemTypePersistent:
      if (!sandbox_provider_->IsAccessAllowed(local_url))
        return false;
      break;
    case kFileSystemTypeLocal:
      if (!local_provider_.get() ||
          !local_provider_->IsAccessAllowed(local_url)) {
        return false;
      }
      break;
    case kFileSystemTypeUnknown:
    default:
      NOTREACHED();
      return false;
  }
  // Any paths that include parent references are considered invalid.
  // These should have been taken care of in CrackFileSystemURL.
  DCHECK(!local_path.ReferencesParent());

  // The given |local_path| seems valid. Populates the |origin_url|, |type|
  // and |virtual_path| if they are given.

  if (origin_url) {
    *origin_url = local_url;
  }

  if (type)
    *type = local_type;

  if (virtual_path) {
    *virtual_path = local_path;
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
std::string FileSystemPathManager::GetFileSystemTypeString(
    fileapi::FileSystemType type) {
  if (type == fileapi::kFileSystemTypeTemporary)
    return fileapi::SandboxMountPointProvider::kTemporaryName;
  else if (type == fileapi::kFileSystemTypePersistent)
    return fileapi::SandboxMountPointProvider::kPersistentName;
  return std::string();
}

// Checks if a given |name| contains any restricted names/chars in it.
bool FileSystemPathManager::IsRestrictedFileName(
    FileSystemType type, const FilePath& filename) {
  switch (type) {
  case kFileSystemTypeTemporary:
  case kFileSystemTypePersistent:
    return sandbox_provider_->IsRestrictedFileName(filename);
  case kFileSystemTypeLocal:
    return local_provider_.get() ?
               local_provider_->IsRestrictedFileName(filename) : true;
  case kFileSystemTypeUnknown:
  default:
    NOTREACHED();
    return true;
  }
}

}  // namespace fileapi

COMPILE_ASSERT(int(WebFileSystem::TypeTemporary) == \
               int(fileapi::kFileSystemTypeTemporary), mismatching_enums);
COMPILE_ASSERT(int(WebFileSystem::TypePersistent) == \
               int(fileapi::kFileSystemTypePersistent), mismatching_enums);
