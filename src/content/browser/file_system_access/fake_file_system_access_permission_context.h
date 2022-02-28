// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "content/public/browser/file_system_access_permission_context.h"

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FAKE_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FAKE_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_

namespace content {

// Fake permission context which uses an in-memory map for
// [GS]etLastPickedDirectory and returns permissions which are always granted.
// Support for WellKnown directories is provided via a setter which allows
// setting custom paths in an in-memory map.
class FakeFileSystemAccessPermissionContext
    : public content::FileSystemAccessPermissionContext {
 public:
  FakeFileSystemAccessPermissionContext();
  ~FakeFileSystemAccessPermissionContext() override;

  scoped_refptr<FileSystemAccessPermissionGrant> GetReadPermissionGrant(
      const url::Origin& origin,
      const base::FilePath& path,
      HandleType handle_type,
      UserAction user_action) override;

  scoped_refptr<FileSystemAccessPermissionGrant> GetWritePermissionGrant(
      const url::Origin& origin,
      const base::FilePath& path,
      HandleType handle_type,
      UserAction user_action) override;

  void ConfirmSensitiveDirectoryAccess(
      const url::Origin& origin,
      PathType path_type,
      const base::FilePath& path,
      HandleType handle_type,
      GlobalRenderFrameHostId frame_id,
      base::OnceCallback<void(SensitiveDirectoryResult)> callback) override;

  void PerformAfterWriteChecks(
      std::unique_ptr<FileSystemAccessWriteItem> item,
      GlobalRenderFrameHostId frame_id,
      base::OnceCallback<void(AfterWriteCheckResult)> callback) override;

  bool CanObtainReadPermission(const url::Origin& origin) override;
  bool CanObtainWritePermission(const url::Origin& origin) override;

  void SetLastPickedDirectory(const url::Origin& origin,
                              const std::string& id,
                              const base::FilePath& path,
                              const PathType type) override;
  PathInfo GetLastPickedDirectory(const url::Origin& origin,
                                  const std::string& id) override;

  // Establishes a mapping between a WellKnownDirectory and a custom path.
  void SetWellKnownDirectoryPath(blink::mojom::WellKnownDirectory directory,
                                 base::FilePath path);
  // Retrieves a path which was earlier specified via SetWellKnownDirectoryPath.
  // Otherwise, returns an empty path.
  base::FilePath GetWellKnownDirectoryPath(
      blink::mojom::WellKnownDirectory directory) override;

 private:
  std::map<std::string, PathInfo> id_pathinfo_map_;
  std::map<blink::mojom::WellKnownDirectory, base::FilePath>
      well_known_directory_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FAKE_FILE_SYSTEM_ACCESS_PERMISSION_CONTEXT_H_