// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/chromeos/fileapi/cros_mount_point_provider.h"

#include "base/logging.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFileSystem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "webkit/chromeos/fileapi/file_access_permissions.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/native_file_util.h"
#include "webkit/glue/webkit_glue.h"

namespace chromeos {

typedef struct {
  const char* local_root_path;
  const char* web_root_path;
} FixedExposedPaths;

const char kChromeUIScheme[] = "chrome";

// Top level file system elements exposed in FileAPI in ChromeOS:
FixedExposedPaths fixed_exposed_paths[] = {
    {"/home/chronos/user/", "Downloads"},
    {"/media",              "archive"},
    {"/media",              "removable"},
};

CrosMountPointProvider::CrosMountPointProvider(
    scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy)
    : special_storage_policy_(special_storage_policy),
      file_access_permissions_(new FileAccessPermissions()),
      local_file_util_(
          new fileapi::LocalFileUtil(new fileapi::NativeFileUtil())) {
  for (size_t i = 0; i < arraysize(fixed_exposed_paths); i++) {
    mount_point_map_.insert(std::pair<std::string, FilePath>(
        std::string(fixed_exposed_paths[i].web_root_path),
        FilePath(std::string(fixed_exposed_paths[i].local_root_path))));
  }
}

CrosMountPointProvider::~CrosMountPointProvider() {
}

// TODO(zelidrag) share with SandboxMountPointProvider impl.
std::string GetOriginIdentifierFromURL(
    const GURL& url) {
  WebKit::WebSecurityOrigin web_security_origin =
      WebKit::WebSecurityOrigin::createFromString(UTF8ToUTF16(url.spec()));
  return web_security_origin.databaseIdentifier().utf8();
}

bool CrosMountPointProvider::GetRootForVirtualPath(
    const FilePath& virtual_path, FilePath* root_path) {
  std::vector<FilePath::StringType> components;
  virtual_path.GetComponents(&components);
  if (components.size() < 1) {
    return false;
  }

  base::AutoLock locker(lock_);
  // Check if this root mount point is exposed by this provider.
  MountPointMap::iterator iter = mount_point_map_.find(components[0]);
  if (iter == mount_point_map_.end()) {
    return false;
  }
  *root_path = iter->second;
  return true;
}

void CrosMountPointProvider::ValidateFileSystemRootAndGetURL(
    const GURL& origin_url,
    fileapi::FileSystemType type,
    bool create,
    const fileapi::FileSystemPathManager::GetRootPathCallback& callback) {
  DCHECK(type == fileapi::kFileSystemTypeExternal);
  std::string name(GetOriginIdentifierFromURL(origin_url));
  name += ':';
  name += fileapi::kExternalName;
  FilePath root_path;
  root_path = FilePath(fileapi::kExternalDir);
  callback.Run(true, root_path, name);
}

FilePath CrosMountPointProvider::ValidateFileSystemRootAndGetPathOnFileThread(
    const GURL& origin_url,
    fileapi::FileSystemType type,
    const FilePath& virtual_path,
    bool create) {
  DCHECK(type == fileapi::kFileSystemTypeExternal);
  FilePath root_path;
  if (!GetRootForVirtualPath(virtual_path, &root_path))
    return FilePath();

  return root_path;
}

// TODO(zelidrag): Share this code with SandboxMountPointProvider impl.
bool CrosMountPointProvider::IsRestrictedFileName(const FilePath& path) const {
  return false;
}

bool CrosMountPointProvider::IsAccessAllowed(const GURL& origin_url,
                                             fileapi::FileSystemType type,
                                             const FilePath& virtual_path) {
  if (type != fileapi::kFileSystemTypeExternal)
    return false;

  // Permit access to mount points from internal WebUI.
  if (origin_url.SchemeIs(kChromeUIScheme))
    return true;

  std::string extension_id = origin_url.host();
  // Check first to make sure this extension has fileBrowserHander permissions.
  if (!special_storage_policy_->IsFileHandler(extension_id))
    return false;

  return file_access_permissions_->HasAccessPermission(extension_id,
                                                       virtual_path);
}

void CrosMountPointProvider::AddMountPoint(FilePath mount_point) {
  base::AutoLock locker(lock_);
  mount_point_map_.insert(std::pair<std::string, FilePath>(
      mount_point.BaseName().value(), mount_point.DirName()));
}

void CrosMountPointProvider::RemoveMountPoint(FilePath mount_point) {
  base::AutoLock locker(lock_);
  mount_point_map_.erase(mount_point.BaseName().value());
}

void CrosMountPointProvider::GrantFullAccessToExtension(
    const std::string& extension_id) {
  DCHECK(special_storage_policy_->IsFileHandler(extension_id));
  if (!special_storage_policy_->IsFileHandler(extension_id))
    return;
  for (MountPointMap::const_iterator iter = mount_point_map_.begin();
       iter != mount_point_map_.end();
       ++iter) {
    GrantFileAccessToExtension(extension_id, FilePath(iter->first));
  }
}

void CrosMountPointProvider::GrantFileAccessToExtension(
    const std::string& extension_id, const FilePath& virtual_path) {
  // All we care about here is access from extensions for now.
  DCHECK(special_storage_policy_->IsFileHandler(extension_id));
  if (!special_storage_policy_->IsFileHandler(extension_id))
    return;
  file_access_permissions_->GrantAccessPermission(extension_id, virtual_path);
}

void CrosMountPointProvider::RevokeAccessForExtension(
      const std::string& extension_id) {
  file_access_permissions_->RevokePermissions(extension_id);
}

std::vector<FilePath> CrosMountPointProvider::GetRootDirectories() const {
  std::vector<FilePath> root_dirs;
  for (MountPointMap::const_iterator iter = mount_point_map_.begin();
       iter != mount_point_map_.end();
       ++iter) {
    root_dirs.push_back(iter->second.Append(iter->first));
  }
  return root_dirs;
}

fileapi::FileSystemFileUtil* CrosMountPointProvider::GetFileUtil() {
  return local_file_util_.get();
}

bool CrosMountPointProvider::GetVirtualPath(const FilePath& filesystem_path,
                                           FilePath* virtual_path) {
  for (MountPointMap::const_iterator iter = mount_point_map_.begin();
       iter != mount_point_map_.end();
       ++iter) {
    FilePath mount_prefix = iter->second.Append(iter->first);
    *virtual_path = FilePath(iter->first);
    if (mount_prefix.AppendRelativePath(filesystem_path, virtual_path)) {
      return true;
    } else if (mount_prefix == filesystem_path) {
      FilePath root = FilePath(FILE_PATH_LITERAL("/"));
      *virtual_path = root.Append(iter->first);
      return true;
    }
  }
  return false;
}

}  // namespace chromeos
