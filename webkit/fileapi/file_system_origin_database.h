// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_ORIGIN_DATABASE_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_ORIGIN_DATABASE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "third_party/leveldb/include/leveldb/db.h"

namespace fileapi {

// All methods of this class other than the constructor may be used only from
// the browser's FILE thread.  The constructor may be used on any thread.
class FileSystemOriginDatabase {
 public:
  struct OriginRecord {
    std::string origin;
    FilePath path;

    OriginRecord();
    OriginRecord(const std::string& origin, const FilePath& path);
    ~OriginRecord();
  };

  // Only one instance of FileSystemOriginDatabase should exist for a given path
  // at a given time.
  FileSystemOriginDatabase(const FilePath& path);
  ~FileSystemOriginDatabase();

  bool HasOriginPath(const std::string& origin);

  // This will produce a unique path and add it to its database, if it's not
  // already present.
  bool GetPathForOrigin(const std::string& origin, FilePath* directory);

  // Also returns success if the origin is not found.
  bool RemovePathForOrigin(const std::string& origin);

  bool ListAllOrigins(std::vector<OriginRecord>* origins);

  // This will release all database resources in use; call it to save memory.
  void DropDatabase();

 private:
  bool Init();
  void HandleError(leveldb::Status status);
  bool GetLastPathNumber(int* number);

  std::string path_;
  scoped_ptr<leveldb::DB> db_;
  DISALLOW_COPY_AND_ASSIGN(FileSystemOriginDatabase);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_ORIGIN_DATABASE_H_
