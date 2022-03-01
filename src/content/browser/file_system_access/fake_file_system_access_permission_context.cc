// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/fake_file_system_access_permission_context.h"

#include "base/files/file_path.h"
#include "content/browser/file_system_access/fixed_file_system_access_permission_grant.h"

namespace content {

FakeFileSystemAccessPermissionContext::FakeFileSystemAccessPermissionContext() =
    default;

FakeFileSystemAccessPermissionContext::
    ~FakeFileSystemAccessPermissionContext() = default;

scoped_refptr<FileSystemAccessPermissionGrant>
FakeFileSystemAccessPermissionContext::GetReadPermissionGrant(
    const url::Origin& origin,
    const base::FilePath& path,
    HandleType handle_type,
    UserAction user_action) {
  return base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
      FileSystemAccessPermissionGrant::PermissionStatus::GRANTED, path);
}

scoped_refptr<FileSystemAccessPermissionGrant>
FakeFileSystemAccessPermissionContext::GetWritePermissionGrant(
    const url::Origin& origin,
    const base::FilePath& path,
    HandleType handle_type,
    UserAction user_action) {
  return base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
      FileSystemAccessPermissionGrant::PermissionStatus::GRANTED, path);
}

void FakeFileSystemAccessPermissionContext::ConfirmSensitiveDirectoryAccess(
    const url::Origin& origin,
    PathType path_type,
    const base::FilePath& path,
    HandleType handle_type,
    GlobalRenderFrameHostId frame_id,
    base::OnceCallback<void(SensitiveDirectoryResult)> callback) {
  std::move(callback).Run(SensitiveDirectoryResult::kAllowed);
}

void FakeFileSystemAccessPermissionContext::PerformAfterWriteChecks(
    std::unique_ptr<FileSystemAccessWriteItem> item,
    GlobalRenderFrameHostId frame_id,
    base::OnceCallback<void(AfterWriteCheckResult)> callback) {
  std::move(callback).Run(AfterWriteCheckResult::kAllow);
}

bool FakeFileSystemAccessPermissionContext::CanObtainReadPermission(
    const url::Origin& origin) {
  return true;
}
bool FakeFileSystemAccessPermissionContext::CanObtainWritePermission(
    const url::Origin& origin) {
  return true;
}

void FakeFileSystemAccessPermissionContext::SetLastPickedDirectory(
    const url::Origin& origin,
    const std::string& id,
    const base::FilePath& path,
    const PathType type) {
  PathInfo info;
  info.path = path;
  info.type = type;
  id_pathinfo_map_.insert({id, info});
}

FakeFileSystemAccessPermissionContext::PathInfo
FakeFileSystemAccessPermissionContext::GetLastPickedDirectory(
    const url::Origin& origin,
    const std::string& id) {
  return id_pathinfo_map_.find(id) != id_pathinfo_map_.end()
             ? id_pathinfo_map_[id]
             : PathInfo();
}

void FakeFileSystemAccessPermissionContext::SetWellKnownDirectoryPath(
    blink::mojom::WellKnownDirectory directory,
    base::FilePath path) {
  well_known_directory_map_.insert({directory, path});
}

base::FilePath FakeFileSystemAccessPermissionContext::GetWellKnownDirectoryPath(
    blink::mojom::WellKnownDirectory directory) {
  return well_known_directory_map_.find(directory) !=
                 well_known_directory_map_.end()
             ? well_known_directory_map_[directory]
             : base::FilePath();
}

}  // namespace content