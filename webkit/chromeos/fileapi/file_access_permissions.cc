// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/chromeos/fileapi/file_access_permissions.h"

#include "base/command_line.h"
#include "base/logging.h"

namespace chromeos {

FileAccessPermissions::FileAccessPermissions() {}

FileAccessPermissions::~FileAccessPermissions() {}


void FileAccessPermissions::GrantAccessPermission(
    const std::string& extension_id, const base::FilePath& path) {
  base::AutoLock locker(lock_);
  PathAccessMap::iterator path_map_iter = path_map_.find(extension_id);
  if (path_map_iter == path_map_.end()) {
    PathSet path_set;
    path_set.insert(path);
    path_map_.insert(PathAccessMap::value_type(extension_id, path_set));
  } else {
    if (path_map_iter->second.find(path) != path_map_iter->second.end())
      return;
    path_map_iter->second.insert(path);
  }
}

bool FileAccessPermissions::HasAccessPermission(
    const std::string& extension_id, const base::FilePath& path) const {
  base::AutoLock locker(lock_);
  PathAccessMap::const_iterator path_map_iter = path_map_.find(extension_id);
  if (path_map_iter == path_map_.end())
    return false;

  // Check this file and walk up its directory tree to find if this extension
  // has access to it.
  base::FilePath current_path = path.StripTrailingSeparators();
  base::FilePath last_path;
  while (current_path != last_path) {
    if (path_map_iter->second.find(current_path) != path_map_iter->second.end())
      return true;
    last_path = current_path;
    current_path = current_path.DirName();
  }
  return false;
}

void FileAccessPermissions::RevokePermissions(
    const std::string& extension_id) {
  base::AutoLock locker(lock_);
  path_map_.erase(extension_id);
}

}
