// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_path_manager.h"

#include "base/rand_util.h"
#include "base/logging.h"
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
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(type);
  if (!mount_point_provider) {
    callback.Run(false, FilePath(), std::string());
    return;
  }
  mount_point_provider->ValidateFileSystemRootAndGetURL(
      origin_url, type, create, callback);
}

FilePath FileSystemPathManager::ValidateFileSystemRootAndGetPathOnFileThread(
    const GURL& origin_url, FileSystemType type, const FilePath& virtual_path,
    bool create) {
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(type);
  if (!mount_point_provider)
    return FilePath();
  return mount_point_provider->ValidateFileSystemRootAndGetPathOnFileThread(
      origin_url, type, virtual_path, create);
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
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(type);
  if (!mount_point_provider)
    return true;
  return mount_point_provider->IsRestrictedFileName(filename);
}

// Checks if an origin has access to a particular filesystem type.
bool FileSystemPathManager::IsAccessAllowed(
    const GURL& origin, FileSystemType type, const FilePath& virtual_path) {
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(type);
  DCHECK(mount_point_provider);
  return mount_point_provider->IsAccessAllowed(origin, type, virtual_path);
}

FileSystemFileUtil* FileSystemPathManager::GetFileUtil(
    FileSystemType type) const {
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(type);
  DCHECK(mount_point_provider);
  return mount_point_provider->GetFileUtil();
}

FileSystemMountPointProvider* FileSystemPathManager::GetMountPointProvider(
    FileSystemType type) const {
  switch (type) {
    case kFileSystemTypeTemporary:
    case kFileSystemTypePersistent:
      return sandbox_provider();
    case kFileSystemTypeExternal:
      return external_provider();
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
