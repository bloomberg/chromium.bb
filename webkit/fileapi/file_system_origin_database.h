// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_ORIGIN_DATABASE_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_ORIGIN_DATABASE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "webkit/storage/webkit_storage_export.h"

namespace leveldb {
class DB;
class Status;
}

namespace tracked_objects {
class Location;
}

namespace fileapi {

// All methods of this class other than the constructor may be used only from
// the browser's FILE thread.  The constructor may be used on any thread.
class WEBKIT_STORAGE_EXPORT_PRIVATE FileSystemOriginDatabase {
 public:
  struct WEBKIT_STORAGE_EXPORT_PRIVATE OriginRecord {
    std::string origin;
    base::FilePath path;

    OriginRecord();
    OriginRecord(const std::string& origin, const base::FilePath& path);
    ~OriginRecord();
  };

  // Only one instance of FileSystemOriginDatabase should exist for a given path
  // at a given time.
  explicit FileSystemOriginDatabase(const base::FilePath& file_system_directory);
  ~FileSystemOriginDatabase();

  bool HasOriginPath(const std::string& origin);

  // This will produce a unique path and add it to its database, if it's not
  // already present.
  bool GetPathForOrigin(const std::string& origin, base::FilePath* directory);

  // Also returns success if the origin is not found.
  bool RemovePathForOrigin(const std::string& origin);

  bool ListAllOrigins(std::vector<OriginRecord>* origins);

  // This will release all database resources in use; call it to save memory.
  void DropDatabase();

 private:
  enum RecoveryOption {
    REPAIR_ON_CORRUPTION,
    DELETE_ON_CORRUPTION,
    FAIL_ON_CORRUPTION,
  };

  bool Init(RecoveryOption recovery_option);
  bool RepairDatabase(const std::string& db_path);
  void HandleError(const tracked_objects::Location& from_here,
                   const leveldb::Status& status);
  void ReportInitStatus(const leveldb::Status& status);
  bool GetLastPathNumber(int* number);

  base::FilePath file_system_directory_;
  scoped_ptr<leveldb::DB> db_;
  base::Time last_reported_time_;
  DISALLOW_COPY_AND_ASSIGN(FileSystemOriginDatabase);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_ORIGIN_DATABASE_H_
