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
#include "third_party/WebKit/Source/Platform/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFileSystem.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "webkit/chromeos/fileapi/file_access_permissions.h"
#include "webkit/chromeos/fileapi/remote_file_stream_writer.h"
#include "webkit/chromeos/fileapi/remote_file_system_operation.h"
#include "webkit/fileapi/external_mount_points.h"
#include "webkit/fileapi/file_system_file_stream_reader.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/isolated_file_util.h"
#include "webkit/fileapi/local_file_stream_writer.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/glue/webkit_glue.h"

namespace {

const char kChromeUIScheme[] = "chrome";

}  // namespace

namespace chromeos {

// static
bool CrosMountPointProvider::CanHandleURL(const fileapi::FileSystemURL& url) {
  if (!url.is_valid())
    return false;
  return url.type() == fileapi::kFileSystemTypeNativeLocal ||
         url.type() == fileapi::kFileSystemTypeRestrictedNativeLocal ||
         url.type() == fileapi::kFileSystemTypeDrive;
}

CrosMountPointProvider::CrosMountPointProvider(
    scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<fileapi::ExternalMountPoints> mount_points,
    fileapi::ExternalMountPoints* system_mount_points)
    : special_storage_policy_(special_storage_policy),
      file_access_permissions_(new FileAccessPermissions()),
      local_file_util_(new fileapi::IsolatedFileUtil()),
      mount_points_(mount_points),
      system_mount_points_(system_mount_points) {
}

CrosMountPointProvider::~CrosMountPointProvider() {
}

void CrosMountPointProvider::ValidateFileSystemRoot(
    const GURL& origin_url,
    fileapi::FileSystemType type,
    bool create,
    const ValidateFileSystemCallback& callback) {
  DCHECK(fileapi::IsolatedContext::IsIsolatedType(type));
  // Nothing to validate for external filesystem.
  callback.Run(base::PLATFORM_FILE_OK);
}

FilePath CrosMountPointProvider::GetFileSystemRootPathOnFileThread(
    const fileapi::FileSystemURL& url,
    bool create) {
  DCHECK(fileapi::IsolatedContext::IsIsolatedType(url.mount_type()));
  if (!url.is_valid())
    return FilePath();

  FilePath root_path;
  std::string mount_name = url.filesystem_id();
  if (!mount_points_->GetRegisteredPath(mount_name, &root_path) &&
      !system_mount_points_->GetRegisteredPath(mount_name, &root_path)) {
    return FilePath();
  }

  return root_path.DirName();
}

bool CrosMountPointProvider::IsAccessAllowed(
    const fileapi::FileSystemURL& url) {
  if (!CanHandleURL(url))
    return false;

  // No extra check is needed for isolated file systems.
  if (url.mount_type() == fileapi::kFileSystemTypeIsolated)
    return true;

  // Permit access to mount points from internal WebUI.
  const GURL& origin_url = url.origin();
  if (origin_url.SchemeIs(kChromeUIScheme))
    return true;

  std::string extension_id = origin_url.host();
  // Check first to make sure this extension has fileBrowserHander permissions.
  if (!special_storage_policy_->IsFileHandler(extension_id))
    return false;

  return file_access_permissions_->HasAccessPermission(extension_id,
                                                       url.virtual_path());
}

// TODO(zelidrag): Share this code with SandboxMountPointProvider impl.
bool CrosMountPointProvider::IsRestrictedFileName(const FilePath& path) const {
  return false;
}

fileapi::FileSystemQuotaUtil* CrosMountPointProvider::GetQuotaUtil() {
  // No quota support.
  return NULL;
}

void CrosMountPointProvider::DeleteFileSystem(
    const GURL& origin_url,
    fileapi::FileSystemType type,
    fileapi::FileSystemContext* context,
    const DeleteFileSystemCallback& callback) {
  NOTREACHED();
  callback.Run(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
}

bool CrosMountPointProvider::HasMountPoint(const FilePath& mount_point) const {
  std::string mount_name = mount_point.BaseName().AsUTF8Unsafe();
  FilePath path;

  const bool valid = mount_points_->GetRegisteredPath(mount_name, &path);
  return valid && path == mount_point;
}

bool CrosMountPointProvider::AddLocalMountPoint(const FilePath& mount_point) {
  std::string mount_name = mount_point.BaseName().AsUTF8Unsafe();
  return mount_points_->RegisterFileSystem(
             mount_name,
             fileapi::kFileSystemTypeNativeLocal,
             mount_point);
}

bool CrosMountPointProvider::AddRestrictedLocalMountPoint(
    const FilePath& mount_point) {
  std::string mount_name = mount_point.BaseName().AsUTF8Unsafe();
  return mount_points_->RegisterFileSystem(
             mount_name,
             fileapi::kFileSystemTypeRestrictedNativeLocal,
             mount_point);
}

bool CrosMountPointProvider::AddRemoteMountPoint(
    const FilePath& mount_point,
    fileapi::RemoteFileSystemProxyInterface* remote_proxy) {
  DCHECK(remote_proxy);
  std::string mount_name = mount_point.BaseName().AsUTF8Unsafe();
  return mount_points_->RegisterRemoteFileSystem(mount_name,
                                                 fileapi::kFileSystemTypeDrive,
                                                 remote_proxy,
                                                 mount_point);
}

void CrosMountPointProvider::RemoveMountPoint(const FilePath& mount_point) {
  if (!HasMountPoint(mount_point))
    return;
  std::string mount_name = mount_point.BaseName().AsUTF8Unsafe();
  mount_points_->RevokeFileSystem(mount_name);
}

void CrosMountPointProvider::GrantFullAccessToExtension(
    const std::string& extension_id) {
  DCHECK(special_storage_policy_->IsFileHandler(extension_id));
  if (!special_storage_policy_->IsFileHandler(extension_id))
    return;

  std::vector<fileapi::MountPoints::MountPointInfo> files;
  mount_points_->AddMountPointInfosTo(&files);
  system_mount_points_->AddMountPointInfosTo(&files);

  for (size_t i = 0; i < files.size(); ++i) {
    file_access_permissions_->GrantAccessPermission(
        extension_id,
        FilePath::FromUTF8Unsafe(files[i].name));
  }
}

void CrosMountPointProvider::GrantFileAccessToExtension(
    const std::string& extension_id, const FilePath& virtual_path) {
  // All we care about here is access from extensions for now.
  DCHECK(special_storage_policy_->IsFileHandler(extension_id));
  if (!special_storage_policy_->IsFileHandler(extension_id))
    return;

  std::string id;
  fileapi::FileSystemType type;
  FilePath path;
  if (!mount_points_->CrackVirtualPath(virtual_path, &id, &type, &path) &&
      !system_mount_points_->CrackVirtualPath(virtual_path,
                                              &id, &type, &path)) {
    return;
  }

  if (type == fileapi::kFileSystemTypeRestrictedNativeLocal) {
    LOG(ERROR) << "Can't grant access for restricted mount point";
    return;
  }

  file_access_permissions_->GrantAccessPermission(extension_id, virtual_path);
}

void CrosMountPointProvider::RevokeAccessForExtension(
      const std::string& extension_id) {
  file_access_permissions_->RevokePermissions(extension_id);
}

std::vector<FilePath> CrosMountPointProvider::GetRootDirectories() const {
  std::vector<fileapi::MountPoints::MountPointInfo> mount_points;
  mount_points_->AddMountPointInfosTo(&mount_points);
  system_mount_points_->AddMountPointInfosTo(&mount_points);

  std::vector<FilePath> root_dirs;
  for (size_t i = 0; i < mount_points.size(); ++i)
    root_dirs.push_back(mount_points[i].path);
  return root_dirs;
}

fileapi::FileSystemFileUtil* CrosMountPointProvider::GetFileUtil(
    fileapi::FileSystemType type) {
  DCHECK(type == fileapi::kFileSystemTypeNativeLocal ||
         type == fileapi::kFileSystemTypeRestrictedNativeLocal);
  return local_file_util_.get();
}

fileapi::FilePermissionPolicy CrosMountPointProvider::GetPermissionPolicy(
    const fileapi::FileSystemURL& url, int permissions) const {
  if (url.mount_type() == fileapi::kFileSystemTypeIsolated) {
    // Permissions in isolated filesystems should be examined with
    // FileSystem permission.
    return fileapi::FILE_PERMISSION_USE_FILESYSTEM_PERMISSION;
  }
  return fileapi::FILE_PERMISSION_USE_FILE_PERMISSION;
}

fileapi::FileSystemOperation* CrosMountPointProvider::CreateFileSystemOperation(
    const fileapi::FileSystemURL& url,
    fileapi::FileSystemContext* context,
    base::PlatformFileError* error_code) const {
  DCHECK(url.is_valid());

  if (url.type() == fileapi::kFileSystemTypeDrive) {
    fileapi::RemoteFileSystemProxyInterface* remote_proxy =
        GetRemoteProxy(url.filesystem_id());
    if (!remote_proxy) {
      *error_code = base::PLATFORM_FILE_ERROR_NOT_FOUND;
      return NULL;
    }
    return new chromeos::RemoteFileSystemOperation(remote_proxy);
  }

  DCHECK(url.type() == fileapi::kFileSystemTypeNativeLocal ||
         url.type() == fileapi::kFileSystemTypeRestrictedNativeLocal);
  scoped_ptr<fileapi::FileSystemOperationContext> operation_context(
      new fileapi::FileSystemOperationContext(context));
  return new fileapi::LocalFileSystemOperation(context,
                                               operation_context.Pass());
}

webkit_blob::FileStreamReader* CrosMountPointProvider::CreateFileStreamReader(
    const fileapi::FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    fileapi::FileSystemContext* context) const {
  // For now we return a generic Reader implementation which utilizes
  // CreateSnapshotFile internally (i.e. will download everything first).
  // TODO(satorux,zel): implement more efficient reader for remote cases.
  return new fileapi::FileSystemFileStreamReader(
      context, url, offset, expected_modification_time);
}

fileapi::FileStreamWriter* CrosMountPointProvider::CreateFileStreamWriter(
    const fileapi::FileSystemURL& url,
    int64 offset,
    fileapi::FileSystemContext* context) const {
  DCHECK(url.is_valid());

  if (url.type() == fileapi::kFileSystemTypeDrive) {
    fileapi::RemoteFileSystemProxyInterface* remote_proxy =
        GetRemoteProxy(url.filesystem_id());
    if (!remote_proxy)
      return NULL;
    return new fileapi::RemoteFileStreamWriter(remote_proxy, url, offset);
  }

  if (url.type() == fileapi::kFileSystemTypeRestrictedNativeLocal)
    return NULL;

  DCHECK(url.type() == fileapi::kFileSystemTypeNativeLocal);
  return new fileapi::LocalFileStreamWriter(url.path(), offset);
}

bool CrosMountPointProvider::GetVirtualPath(const FilePath& filesystem_path,
                                           FilePath* virtual_path) {
  return mount_points_->GetVirtualPath(filesystem_path, virtual_path) ||
         system_mount_points_->GetVirtualPath(filesystem_path, virtual_path);
}

fileapi::RemoteFileSystemProxyInterface* CrosMountPointProvider::GetRemoteProxy(
    const std::string& mount_name) const {
  fileapi::RemoteFileSystemProxyInterface* proxy =
      mount_points_->GetRemoteFileSystemProxy(mount_name);
  if (proxy)
    return proxy;
  return system_mount_points_->GetRemoteFileSystemProxy(mount_name);
}

}  // namespace chromeos
