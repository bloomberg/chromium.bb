// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/stringprintf.h"
#include "webkit/fileapi/syncable/file_change.h"

#include "base/logging.h"

namespace fileapi {

FileChange::FileChange(
    ChangeType change,
    FileType file_type)
    : change_(change),
      file_type_(file_type) {
  // There's no update change for directories.
  DCHECK(change != FILE_CHANGE_UPDATE || file_type != FILE_TYPE_DIRECTORY);
}

std::string FileChange::DebugString() const {
  const char* change_string = NULL;
  switch (change()) {
    case FILE_CHANGE_ADD:
      change_string = "ADD";
      break;
    case FILE_CHANGE_DELETE:
      change_string = "DELETE";
      break;
    case FILE_CHANGE_UPDATE:
      change_string = "UPDATE";
      break;
  }
  const char* type_string = NULL;
  switch (file_type()) {
    case FILE_TYPE_FILE:
      type_string = "FILE";
      break;
    case FILE_TYPE_DIRECTORY:
      type_string = "DIRECTORY";
      break;
  }
  return base::StringPrintf("%s:%s", change_string, type_string);
}

FileChangeList::FileChangeList() {}
FileChangeList::~FileChangeList() {}

void FileChangeList::Update(const FileChange& new_change) {
  if (list_.empty()) {
    list_.push_back(new_change);
    return;
  }

  FileChange& last = list_.back();
  if (last.IsFile() != new_change.IsFile()) {
    list_.push_back(new_change);
    return;
  }

  if (last.change() == new_change.change())
    return;

  // Change transitions:
  //  ADD    + DELETE -> revert [A]
  //  ADD    + UPDATE -> ADD [B]
  //  DELETE + ADD    -> ADD
  //  DELETE + UPDATE -> invalid [C]
  //  UPDATE + ADD    -> invalid [C]
  //  UPDATE + DELETE -> DELETE

  // [A]: ADD + DELETE -> revert
  if (last.IsAdd() && new_change.IsDelete()) {
    list_.pop_back();
    return;
  }

  // [B]: ADD + UPDATE -> ADD
  if (last.IsAdd() && new_change.IsUpdate())
    return;

  // [C]: invalid cases.
  DCHECK(!(last.IsDelete() && new_change.IsUpdate()));
  DCHECK(!(last.IsUpdate() && new_change.IsAdd()));

  // All others.
  last = new_change;
}

std::string FileChangeList::DebugString() const {
  std::ostringstream ss;
  ss << "{ ";
  for (size_t i = 0; i < list_.size(); ++i)
    ss << list_[i].DebugString() << ", ";
  ss << "}";
  return ss.str();
}

}  // namespace fileapi
