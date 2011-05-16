// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_DIRECTORY_DATABASE_H
#define WEBKIT_FILEAPI_FILE_SYSTEM_DIRECTORY_DATABASE_H

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "third_party/leveldb/include/leveldb/db.h"

namespace leveldb {
class WriteBatch;
}

namespace fileapi {

// This class WILL NOT protect you against producing directory loops, giving an
// empty directory a backing data file, giving two files the same backing file,
// or pointing to a nonexistent backing file.  It does no file IO other than
// that involved with talking to its underlying database.  It does not create or
// in any way touch real files; it only creates path entries in its database.

// TODO(ericu): Safe mode, which does more checks such as the above on debug
// builds.
// TODO(ericu): FSCK, for a full-database check [data file validation possibly
// done elsewhere].
// TODO(ericu): Add a method that will give a unique filename for a data file.
class FileSystemDirectoryDatabase {
 public:
  typedef int64 FileId;

  struct FileInfo {
    FileInfo();
    ~FileInfo();

    bool is_directory() {
      return data_path.empty();
    }

    FileId parent_id;
    FilePath data_path;
    FilePath::StringType name;
    // This modification time is valid only for directories, not files, as
    // FileWriter will get the files out of sync.
    // For files, look at the modification time of the underlying data_path.
    base::Time modification_time;
  };

  FileSystemDirectoryDatabase(const FilePath& path);
  ~FileSystemDirectoryDatabase();

  bool GetChildWithName(
      FileId parent_id, const FilePath::StringType& name, FileId* child_id);
  bool GetFileWithPath(const FilePath& path, FileId* file_id);
  // ListChildren will succeed, returning 0 children, if parent_id doesn't
  // exist.
  bool ListChildren(FileId parent_id, std::vector<FileId>* children);
  bool GetFileInfo(FileId file_id, FileInfo* info);
  bool AddFileInfo(const FileInfo& info, FileId* file_id);
  bool RemoveFileInfo(FileId file_id);
  // This does a full update of the FileInfo, and is what you'd use for moves
  // and renames.  If you just want to update the modification_time, use
  // UpdateModificationTime.
  bool UpdateFileInfo(FileId file_id, const FileInfo& info);
  bool UpdateModificationTime(
      FileId file_id, const base::Time& modification_time);
  // This is used for an overwriting move of a file [not a directory] on top of
  // another file [also not a directory]; we need to alter two files' info in a
  // single transaction to avoid weird backing file references in the event of a
  // partial failure.
  bool OverwritingMoveFile(FileId src_file_id, FileId dest_file_id);

  // This produces the series 0, 1, 2..., starting at 0 when the underlying
  // filesystem is first created, and maintaining state across
  // creation/destruction of FileSystemDirectoryDatabase objects.
  bool GetNextInteger(int64* next);

 private:
  bool Init();
  bool StoreDefaultValues();
  bool GetLastFileId(FileId* file_id);
  bool VerifyIsDirectory(FileId file_id);
  bool AddFileInfoHelper(
      const FileInfo& info, FileId file_id, leveldb::WriteBatch* batch);
  bool RemoveFileInfoHelper(FileId file_id, leveldb::WriteBatch* batch);
  void HandleError(leveldb::Status status);

  std::string path_;
  scoped_ptr<leveldb::DB> db_;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_DIRECTORY_DATABASE_H
