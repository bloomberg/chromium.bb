// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/chromeos/fileapi/cros_mount_point_provider.h"

#include "base/chromeos/chromeos_version.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFileSystem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "webkit/chromeos/fileapi/file_access_permissions.h"
#include "webkit/chromeos/fileapi/remote_file_stream_writer.h"
#include "webkit/chromeos/fileapi/remote_file_system_operation.h"
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
         url.type() == fileapi::kFileSystemTypeDrive;
}

CrosMountPointProvider::CrosMountPointProvider(
    scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy)
    : special_storage_policy_(special_storage_policy),
      file_access_permissions_(new FileAccessPermissions()),
      local_file_util_(new fileapi::IsolatedFileUtil()) {
  FilePath home_path;
  if (PathService::Get(base::DIR_HOME, &home_path))
    AddLocalMountPoint(home_path.AppendASCII("Downloads"));
  AddLocalMountPoint(FilePath(FILE_PATH_LITERAL("/media/archive")));
  AddLocalMountPoint(FilePath(FILE_PATH_LITERAL("/media/removable")));
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
    const GURL& origin_url,
    fileapi::FileSystemType type,
    const FilePath& virtual_path,
    bool create) {
  DCHECK(fileapi::IsolatedContext::IsIsolatedType(type));
  fileapi::FileSystemURL url(origin_url, type, virtual_path);
  if (!url.is_valid())
    return FilePath();

  FilePath root_path;
  if (!isolated_context()->GetRegisteredPath(url.filesystem_id(), &root_path))
    return FilePath();

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

bool CrosMountPointProvider::HasMountPoint(const FilePath& mount_point) {
  std::string mount_name = mount_point.BaseName().AsUTF8Unsafe();
  FilePath path;
  const bool valid = isolated_context()->GetRegisteredPath(mount_name, &path);
  return valid && path == mount_point;
}

void CrosMountPointProvider::AddLocalMountPoint(const FilePath& mount_point) {
  std::string mount_name = mount_point.BaseName().AsUTF8Unsafe();
  isolated_context()->RevokeFileSystem(mount_name);
  isolated_context()->RegisterExternalFileSystem(
      mount_name,
      fileapi::kFileSystemTypeNativeLocal,
      mount_point);
  base::AutoLock locker(mount_point_map_lock_);
  local_to_virtual_map_[mount_point] = mount_point.BaseName();
}

void CrosMountPointProvider::AddRemoteMountPoint(
    const FilePath& mount_point,
    fileapi::RemoteFileSystemProxyInterface* remote_proxy) {
  DCHECK(remote_proxy);
  std::string mount_name = mount_point.BaseName().AsUTF8Unsafe();
  isolated_context()->RevokeFileSystem(mount_name);
  isolated_context()->RegisterExternalFileSystem(mount_name,
                                                 fileapi::kFileSystemTypeDrive,
                                                 mount_point);
  base::AutoLock locker(mount_point_map_lock_);
  remote_proxy_map_[mount_name] = remote_proxy;
  local_to_virtual_map_[mount_point] = mount_point.BaseName();
}

void CrosMountPointProvider::RemoveMountPoint(const FilePath& mount_point) {
  std::string mount_name = mount_point.BaseName().AsUTF8Unsafe();
  isolated_context()->RevokeFileSystem(mount_name);
  base::AutoLock locker(mount_point_map_lock_);
  remote_proxy_map_.erase(mount_name);
  local_to_virtual_map_.erase(mount_point);
}

void CrosMountPointProvider::GrantFullAccessToExtension(
    const std::string& extension_id) {
  DCHECK(special_storage_policy_->IsFileHandler(extension_id));
  if (!special_storage_policy_->IsFileHandler(extension_id))
    return;
  std::vector<fileapi::IsolatedContext::FileInfo> files =
      isolated_context()->GetExternalMountPoints();
  for (size_t i = 0; i < files.size(); ++i) {
    GrantFileAccessToExtension(extension_id,
                               FilePath::FromUTF8Unsafe(files[i].name));
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
  std::vector<fileapi::IsolatedContext::FileInfo> files =
      isolated_context()->GetExternalMountPoints();
  std::vector<FilePath> root_dirs;
  for (size_t i = 0; i < files.size(); ++i)
    root_dirs.push_back(files[i].path);
  return root_dirs;
}

fileapi::FileSystemFileUtil* CrosMountPointProvider::GetFileUtil(
    fileapi::FileSystemType type) {
  DCHECK(type == fileapi::kFileSystemTypeNativeLocal);
  return local_file_util_.get();
}

FilePath CrosMountPointProvider::GetPathForPermissionsCheck(
    const FilePath& virtual_path) const {
  return virtual_path;
}

fileapi::FileSystemOperation* CrosMountPointProvider::CreateFileSystemOperation(
    const fileapi::FileSystemURL& url,
    fileapi::FileSystemContext* context,
    base::PlatformFileError* error_code) const {
  if (url.type() == fileapi::kFileSystemTypeDrive) {
    base::AutoLock locker(mount_point_map_lock_);
    RemoteProxyMap::const_iterator found = remote_proxy_map_.find(
        url.filesystem_id());
    if (found != remote_proxy_map_.end()) {
      return new chromeos::RemoteFileSystemOperation(found->second);
    } else {
      *error_code = base::PLATFORM_FILE_ERROR_NOT_FOUND;
      return NULL;
    }
  }

  DCHECK(url.type() == fileapi::kFileSystemTypeNativeLocal);
  scoped_ptr<fileapi::FileSystemOperationContext> operation_context(
      new fileapi::FileSystemOperationContext(context));
  return new fileapi::LocalFileSystemOperation(context,
                                               operation_context.Pass());
}

webkit_blob::FileStreamReader* CrosMountPointProvider::CreateFileStreamReader(
    const fileapi::FileSystemURL& url,
    int64 offset,
    fileapi::FileSystemContext* context) const {
  // For now we return a generic Reader implementation which utilizes
  // CreateSnapshotFile internally (i.e. will download everything first).
  // TODO(satorux,zel): implement more efficient reader for remote cases.
  return new fileapi::FileSystemFileStreamReader(context, url, offset);
}

fileapi::FileStreamWriter* CrosMountPointProvider::CreateFileStreamWriter(
    const fileapi::FileSystemURL& url,
    int64 offset,
    fileapi::FileSystemContext* context) const {
  if (!url.is_valid())
    return NULL;
  if (url.type() == fileapi::kFileSystemTypeDrive) {
    base::AutoLock locker(mount_point_map_lock_);
    RemoteProxyMap::const_iterator found = remote_proxy_map_.find(
        url.filesystem_id());
    if (found == remote_proxy_map_.end())
      return NULL;
    return new fileapi::RemoteFileStreamWriter(found->second, url, offset);
  }

  DCHECK(url.type() == fileapi::kFileSystemTypeNativeLocal);
  return new fileapi::LocalFileStreamWriter(url.path(), offset);
}

bool CrosMountPointProvider::GetVirtualPath(const FilePath& filesystem_path,
                                           FilePath* virtual_path) {
  base::AutoLock locker(mount_point_map_lock_);
  std::map<FilePath, FilePath>::reverse_iterator iter(
      local_to_virtual_map_.upper_bound(filesystem_path));
  if (iter == local_to_virtual_map_.rend())
    return false;
  if (iter->first == filesystem_path) {
    *virtual_path = iter->second;
    return true;
  }
  return iter->first.DirName().AppendRelativePath(
      filesystem_path, virtual_path);
}

fileapi::IsolatedContext* CrosMountPointProvider::isolated_context() const {
  return fileapi::IsolatedContext::GetInstance();
}

}  // namespace chromeos
