// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_path_manager.h"

#include "base/rand_util.h"
#include "base/logging.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFileSystem.h"
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

static const char kChromeScheme[] = "chrome";
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
  external_provider_.reset(
      new chromeos::CrosMountPointProvider(special_storage_policy));
#endif
}

FileSystemPathManager::~FileSystemPathManager() {}

void FileSystemPathManager::ValidateFileSystemRootAndGetURL(
    const GURL& origin_url, fileapi::FileSystemType type, bool create,
    const GetRootPathCallback& callback) {
  switch (type) {
    case kFileSystemTypeTemporary:
    case kFileSystemTypePersistent:
      sandbox_provider_->ValidateFileSystemRootAndGetURL(
          origin_url, type, create, callback);
      break;
    case kFileSystemTypeExternal:
      if (external_provider_.get()) {
        external_provider_->ValidateFileSystemRootAndGetURL(
            origin_url, type, create, callback);
      } else {
        callback.Run(false, FilePath(), std::string());
      }
      break;
    case kFileSystemTypeUnknown:
    default:
      NOTREACHED();
      callback.Run(false, FilePath(), std::string());
  }
}

FilePath FileSystemPathManager::ValidateFileSystemRootAndGetPathOnFileThread(
    const GURL& origin_url, FileSystemType type, const FilePath& virtual_path,
    bool create) {
  switch (type) {
  case kFileSystemTypeTemporary:
  case kFileSystemTypePersistent:
    return sandbox_provider_->ValidateFileSystemRootAndGetPathOnFileThread(
        origin_url, type, virtual_path, create);
  case kFileSystemTypeExternal:
    return external_provider_.get() ?
        external_provider_->ValidateFileSystemRootAndGetPathOnFileThread(
           origin_url, type, virtual_path, create) :
        FilePath();
  case kFileSystemTypeUnknown:
  default:
    NOTREACHED();
    return FilePath();
  }
}

bool FileSystemPathManager::IsAllowedScheme(const GURL& url) const {
  // Basically we only accept http or https. We allow file:// URLs
  // only if --allow-file-access-from-files flag is given.
  return url.SchemeIs("http") || url.SchemeIs("https") ||
         url.SchemeIs(kExtensionScheme) || url.SchemeIs(kChromeScheme) ||
         (url.SchemeIsFile() && allow_file_access_from_files_);
}

// static
std::string FileSystemPathManager::GetFileSystemTypeString(
    fileapi::FileSystemType type) {
  if (type == fileapi::kFileSystemTypeTemporary)
    return fileapi::kTemporaryName;
  else if (type == fileapi::kFileSystemTypePersistent)
    return fileapi::kPersistentName;
  else if (type == fileapi::kFileSystemTypeExternal)
    return fileapi::kExternalName;
  return std::string();
}

// Checks if a given |name| contains any restricted names/chars in it.
bool FileSystemPathManager::IsRestrictedFileName(
    FileSystemType type, const FilePath& filename) {
  switch (type) {
  case kFileSystemTypeTemporary:
  case kFileSystemTypePersistent:
    return sandbox_provider_->IsRestrictedFileName(filename);
  case kFileSystemTypeExternal:
    return external_provider_.get() ?
               external_provider_->IsRestrictedFileName(filename) : true;
  case kFileSystemTypeUnknown:
  default:
    NOTREACHED();
    return true;
  }
}

// Checks if an origin has access to a particular filesystem type.
bool FileSystemPathManager::IsAccessAllowed(
    const GURL& origin, FileSystemType type, const FilePath& virtual_path) {
  switch (type) {
    case kFileSystemTypeTemporary:
    case kFileSystemTypePersistent:
      if (!sandbox_provider_->IsAccessAllowed(origin, type, virtual_path))
        return false;
      break;
    case kFileSystemTypeExternal:
      if (!external_provider_.get() ||
          !external_provider_->IsAccessAllowed(origin, type, virtual_path)) {
        return false;
      }
      break;
    case kFileSystemTypeUnknown:
    default:
      NOTREACHED();
      return false;
  }
  return true;
}

FileSystemFileUtil* FileSystemPathManager::GetFileUtil(
    FileSystemType type) const {
  switch (type) {
    case kFileSystemTypeTemporary:
    case kFileSystemTypePersistent:
      return sandbox_provider_->GetFileUtil();
    case kFileSystemTypeExternal:
      if (external_provider_.get()) {
        return external_provider_->GetFileUtil();
      } else {
        NOTREACHED();
        return NULL;
      }
    case kFileSystemTypeUnknown:
    default:
      NOTREACHED();
      return NULL;
  }
}

}  // namespace fileapi

COMPILE_ASSERT(int(WebFileSystem::TypeTemporary) == \
               int(fileapi::kFileSystemTypeTemporary), mismatching_enums);
COMPILE_ASSERT(int(WebFileSystem::TypePersistent) == \
               int(fileapi::kFileSystemTypePersistent), mismatching_enums);
COMPILE_ASSERT(int(WebFileSystem::TypeExternal) == \
               int(fileapi::kFileSystemTypeExternal), mismatching_enums);
