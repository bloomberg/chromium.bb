// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/chromeos/fileapi/cros_mount_point_provider.h"

#include "base/chromeos/chromeos_version.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFileSystem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "webkit/chromeos/fileapi/file_access_permissions.h"
#include "webkit/chromeos/fileapi/remote_file_system_operation.h"
#include "webkit/fileapi/file_system_file_reader.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/native_file_util.h"
#include "webkit/glue/webkit_glue.h"

namespace {

const char kChromeUIScheme[] = "chrome";

// Returns the home directory path, or an empty string if the home directory
// is not found.
std::string GetHomeDirectory() {
  if (base::chromeos::IsRunningOnChromeOS())
    return "/home/chronos/user";

  const char* home = getenv("HOME");
  if (home)
    return home;
  return "";
}

}  // namespace

namespace chromeos {

CrosMountPointProvider::MountPoint::MountPoint(
    const FilePath& in_web_root_path,
    const FilePath& in_local_root_path,
    FileSystemLocation in_location,
    fileapi::RemoteFileSystemProxyInterface* in_proxy)
        : web_root_path(in_web_root_path), local_root_path(in_local_root_path),
          location(in_location), remote_proxy(in_proxy) {
}

CrosMountPointProvider::MountPoint::~MountPoint() {
}

CrosMountPointProvider::CrosMountPointProvider(
    scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy)
    : special_storage_policy_(special_storage_policy),
      file_access_permissions_(new FileAccessPermissions()),
      local_file_util_(
          new fileapi::LocalFileUtil(new fileapi::NativeFileUtil())) {
  const std::string home = GetHomeDirectory();
  if (!home.empty()) {
    AddLocalMountPoint(
        FilePath::FromUTF8Unsafe(home).AppendASCII("Downloads"));
  }
  AddLocalMountPoint(FilePath(FILE_PATH_LITERAL("/media/archive")));
  AddLocalMountPoint(FilePath(FILE_PATH_LITERAL("/media/removable")));
}

CrosMountPointProvider::~CrosMountPointProvider() {
}

bool CrosMountPointProvider::GetRootForVirtualPath(
    const FilePath& virtual_path, FilePath* root_path) {
  const MountPoint* mount_point = GetMountPoint(virtual_path);
  if (!mount_point)
    return false;

  DCHECK(root_path);
  *root_path = mount_point->local_root_path;
  return true;
}

void CrosMountPointProvider::ValidateFileSystemRoot(
    const GURL& origin_url,
    fileapi::FileSystemType type,
    bool create,
    const ValidateFileSystemCallback& callback) {
  // Nothing to validate for external filesystem.
  DCHECK(type == fileapi::kFileSystemTypeExternal);
  callback.Run(base::PLATFORM_FILE_OK);
}

FilePath CrosMountPointProvider::GetFileSystemRootPathOnFileThread(
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

// TODO(zelidrag): Share this code with SandboxMountPointProvider impl.
bool CrosMountPointProvider::IsRestrictedFileName(const FilePath& path) const {
  return false;
}

fileapi::FileSystemQuotaUtil* CrosMountPointProvider::GetQuotaUtil() {
  // No quota support.
  return NULL;
}

bool CrosMountPointProvider::HasMountPoint(const FilePath& mount_point) {
  base::AutoLock locker(mount_point_map_lock_);
  MountPointMap::const_iterator iter = mount_point_map_.find(
      mount_point.BaseName().value());
  DCHECK(iter == mount_point_map_.end() ||
         iter->second.local_root_path == mount_point.DirName());
  return iter != mount_point_map_.end();
}

void CrosMountPointProvider::AddLocalMountPoint(const FilePath& mount_point) {
  base::AutoLock locker(mount_point_map_lock_);
  mount_point_map_.erase(mount_point.BaseName().value());
  mount_point_map_.insert(std::make_pair(
      mount_point.BaseName().value(),
      MountPoint(mount_point.BaseName(),
                 mount_point.DirName(),
                 LOCAL,
                 NULL)));
}

void CrosMountPointProvider::AddRemoteMountPoint(
    const FilePath& mount_point,
    fileapi::RemoteFileSystemProxyInterface* remote_proxy) {
  DCHECK(remote_proxy);
  base::AutoLock locker(mount_point_map_lock_);
  mount_point_map_.erase(mount_point.BaseName().value());
  mount_point_map_.insert(std::make_pair(
      mount_point.BaseName().value(),
      MountPoint(mount_point.BaseName(),
                 mount_point.DirName(),
                 REMOTE,
                 remote_proxy)));
}

void CrosMountPointProvider::RemoveMountPoint(const FilePath& mount_point) {
  base::AutoLock locker(mount_point_map_lock_);
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
    root_dirs.push_back(iter->second.local_root_path.Append(iter->first));
  }
  return root_dirs;
}

fileapi::FileSystemFileUtil* CrosMountPointProvider::GetFileUtil() {
  return local_file_util_.get();
}

FilePath CrosMountPointProvider::GetPathForPermissionsCheck(
    const FilePath& virtual_path) const {
  const MountPoint* mount_point = GetMountPoint(virtual_path);
  if (!mount_point)
    return FilePath();

  FilePath root_path = mount_point->local_root_path;

  return root_path.Append(virtual_path);
}

const CrosMountPointProvider::MountPoint*
CrosMountPointProvider::GetMountPoint(const FilePath& virtual_path) const {
  std::vector<FilePath::StringType> components;
  virtual_path.GetComponents(&components);
  if (components.empty())
    return NULL;

  base::AutoLock locker(
      const_cast<CrosMountPointProvider*>(this)->mount_point_map_lock_);
  // Check if this root mount point is exposed by this provider.
  MountPointMap::const_iterator iter = mount_point_map_.find(components[0]);
  if (iter == mount_point_map_.end())
    return NULL;

  return &(iter->second);
}

fileapi::FileSystemOperationInterface*
CrosMountPointProvider::CreateFileSystemOperation(
    const GURL& origin_url,
    fileapi::FileSystemType file_system_type,
    const FilePath& virtual_path,
    fileapi::FileSystemContext* context) const {
  const MountPoint* mount_point = GetMountPoint(virtual_path);
  if (mount_point && mount_point->location == REMOTE)
    return new chromeos::RemoteFileSystemOperation(mount_point->remote_proxy);

  return new fileapi::FileSystemOperation(context);
}

webkit_blob::FileReader* CrosMountPointProvider::CreateFileReader(
    const GURL& url,
    int64 offset,
    fileapi::FileSystemContext* context) const {
  // For now we return a generic Reader implementation which utilizes
  // CreateSnapshotFile internally (i.e. will download everything first).
  // TODO(satorux,zel): implement more efficient reader for remote cases.
  return new fileapi::FileSystemFileReader(context, url, offset);
}

fileapi::FileWriter* CrosMountPointProvider::CreateFileWriter(
    const GURL& url,
    int64 offset,
    fileapi::FileSystemContext* context) const {
  // TODO(kinaba,kinuko,benchan,satorux): return a writer for remote or local
  // file system depending on the mount point location.
  NOTIMPLEMENTED();
  return NULL;
}

bool CrosMountPointProvider::GetVirtualPath(const FilePath& filesystem_path,
                                           FilePath* virtual_path) {
  for (MountPointMap::const_iterator iter = mount_point_map_.begin();
       iter != mount_point_map_.end();
       ++iter) {
    FilePath mount_prefix = iter->second.local_root_path.Append(iter->first);
    *virtual_path = FilePath(iter->first);
    if (mount_prefix == filesystem_path) {
      return true;
    } else if (mount_prefix.AppendRelativePath(filesystem_path, virtual_path)) {
      return true;
    }
  }
  return false;
}

}  // namespace chromeos
