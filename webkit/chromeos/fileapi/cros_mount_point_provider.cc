  // Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/chromeos/fileapi/cros_mount_point_provider.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_callback_factory.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/glue/webkit_glue.h"

namespace chromeos {

const char CrosMountPointProvider::kLocalName[] = "Local";
const char CrosMountPointProvider::kLocalDirName[] = "/local/";

typedef struct {
  const char* local_root_path;
  const char* web_root_path;
} FixedExposedPaths;

// Top level file system elements exposed in FileAPI in ChromeOS:
FixedExposedPaths fixed_exposed_paths[] = {
    {"/home/chronos/user/", "Downloads"},
    {"/",                   "media"},
    {"/",                   "tmp"},
};

CrosMountPointProvider::CrosMountPointProvider(
    scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy)
    : local_dir_name_(kLocalName),
      special_storage_policy_(special_storage_policy) {

  // TODO(zelidrag): There's got to be a better way to generate UUID.
  srand(time(NULL));
  std::string virtual_root;
  virtual_root.append(base::StringPrintf("%hx%hx-%hx-%hx-%hx-%hx%hx%hx",
      static_cast<unsigned short>(rand()), static_cast<unsigned short>(rand()),
      static_cast<unsigned short>(rand()),
      static_cast<unsigned short>(rand()),
      static_cast<unsigned short>(rand()),
      static_cast<unsigned short>(rand()), static_cast<unsigned short>(rand()),
          static_cast<unsigned short>(rand())));

  // Create virtual root node.
  virtual_root_path_ = FilePath(virtual_root);
  base_path_ = virtual_root_path_.Append(local_dir_name_);

  for (size_t i = 0; i < arraysize(fixed_exposed_paths); i++) {
    mount_point_map_.insert(std::pair<std::string, std::string>(
        std::string(fixed_exposed_paths[i].web_root_path),
        std::string(fixed_exposed_paths[i].local_root_path)));
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

void CrosMountPointProvider::GetFileSystemRootPath(
    const GURL& origin_url,
    fileapi::FileSystemType type,
    bool create,
    fileapi::FileSystemPathManager::GetRootPathCallback* callback_ptr) {
  DCHECK(type == fileapi::kFileSystemTypeLocal);

  std::string name(GetOriginIdentifierFromURL(origin_url));
  name += ':';
  name += CrosMountPointProvider::kLocalName;

  FilePath root_path = FilePath(CrosMountPointProvider::kLocalDirName);
  callback_ptr->Run(!root_path.empty(), root_path, name);
}

// Like GetFileSystemRootPath, but synchronous, and can be called only while
// running on the file thread.
FilePath CrosMountPointProvider::GetFileSystemRootPathOnFileThread(
    const GURL& origin_url,
    fileapi::FileSystemType type,
    const FilePath& virtual_path,
    bool create) {
  DCHECK(type == fileapi::kFileSystemTypeLocal);

  std::vector<FilePath::StringType> components;
  virtual_path.GetComponents(&components);
  if (components.size() < 1) {
    return FilePath();
  }

  // Check if this root directory is exposed by this provider.
  MountPointMap::iterator iter = mount_point_map_.find(components[0]);
  if (iter == mount_point_map_.end()) {
    return FilePath();
  }

  return FilePath(iter->second);
}

// TODO(zelidrag): Share this code with SandboxMountPointProvider impl.
bool CrosMountPointProvider::IsRestrictedFileName(const FilePath& path) const {
  return false;
}

bool CrosMountPointProvider::IsAccessAllowed(const GURL& origin_url) {
  return special_storage_policy_->IsLocalFileSystemAccessAllowed(origin_url);
}

}  // namespace chromeos

