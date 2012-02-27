// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_path.h"

#include "webkit/fileapi/file_system_types.h"

namespace fileapi {

FileSystemPath::FileSystemPath()
    : type_(kFileSystemTypeUnknown) {}

FileSystemPath::FileSystemPath(
    const GURL& origin,
    FileSystemType type,
    const FilePath& internal_path)
    : origin_(origin),
      type_(type),
      internal_path_(internal_path) {}

FileSystemPath::~FileSystemPath() {}

FileSystemPath FileSystemPath::WithInternalPath(const FilePath& internal_path)
    const {
  FileSystemPath new_path(*this);
  new_path.set_internal_path(internal_path);
  return new_path;
}

bool FileSystemPath::operator==(const FileSystemPath& that) const {
  return origin_ == that.origin_ &&
      type_ == that.type_ &&
      internal_path_ == that.internal_path_;
}

}  // namespace fileapi
