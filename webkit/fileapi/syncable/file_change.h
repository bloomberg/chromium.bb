// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_FILE_CHANGE_H_
#define WEBKIT_FILEAPI_SYNCABLE_FILE_CHANGE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "webkit/fileapi/fileapi_export.h"

namespace fileapi {

class FILEAPI_EXPORT FileChange {
 public:
  enum ChangeType {
    FILE_CHANGE_ADD,
    FILE_CHANGE_DELETE,
    FILE_CHANGE_UPDATE,
  };

  enum FileType {
    FILE_TYPE_DIRECTORY,
    FILE_TYPE_FILE,
  };

  FileChange(ChangeType change, FileType file_type);

  bool IsAdd() const { return change_ == FILE_CHANGE_ADD; }
  bool IsDelete() const { return change_ == FILE_CHANGE_DELETE; }
  bool IsUpdate() const { return change_ == FILE_CHANGE_UPDATE; }

  bool IsFile() const { return file_type_ == FILE_TYPE_FILE; }

  ChangeType change() const { return change_; }
  FileType file_type() const { return file_type_; }

  std::string DebugString() const;

  bool operator==(const FileChange& that) const {
    return change() == that.change() &&
        file_type() == that.file_type();
  }

 private:
  ChangeType change_;
  FileType file_type_;
};

class FILEAPI_EXPORT FileChangeList {
 public:
  FileChangeList();
  ~FileChangeList();

  // Updates the list with the |new_change|.
  void Update(const FileChange& new_change);

  size_t size() const { return list_.size(); }
  bool empty() const { return list_.empty(); }
  void clear() { list_.clear(); }
  const std::vector<FileChange>& list() const { return list_; }

  std::string DebugString() const;

 private:
  std::vector<FileChange> list_;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SYNCABLE_FILE_CHANGE_H_
