// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_FILE_ACCESS_PERMISSIONS_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_FILE_ACCESS_PERMISSIONS_H_

#include <map>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"

namespace chromeos {

// In a thread safe manner maintains the set of paths allowed to access for
// each extension.
class FileAccessPermissions {
 public:
  FileAccessPermissions();
  virtual ~FileAccessPermissions();

  // Grants |extension_id| access to |path|.
  void GrantAccessPermission(const std::string& extension_id,
                             const base::FilePath& path);
  // Checks id |extension_id| has permission to access to |path|.
  bool HasAccessPermission(const std::string& extension_id,
                           const base::FilePath& path) const;
  // Revokes all file permissions for |extension_id|.
  void RevokePermissions(const std::string& extension_id);

 private:
  typedef std::set<base::FilePath> PathSet;
  typedef std::map<std::string, PathSet> PathAccessMap;

  mutable base::Lock lock_;  // Synchronize all access to path_map_.
  PathAccessMap path_map_;

  DISALLOW_COPY_AND_ASSIGN(FileAccessPermissions);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_FILE_ACCESS_PERMISSIONS_H_
