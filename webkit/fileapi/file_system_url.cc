// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_url.h"

#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"

namespace fileapi {

FileSystemURL::FileSystemURL()
    : type_(kFileSystemTypeUnknown),
      is_valid_(false) {}

FileSystemURL::FileSystemURL(const GURL& url)
    : type_(kFileSystemTypeUnknown) {
  is_valid_ = CrackFileSystemURL(url, &origin_, &type_, &virtual_path_);
  MayCrackIsolatedPath();
}

FileSystemURL::FileSystemURL(
    const GURL& origin,
    FileSystemType type,
    const FilePath& path)
    : origin_(origin),
      type_(type),
      virtual_path_(path),
      is_valid_(true) {
  MayCrackIsolatedPath();
}

FileSystemURL::~FileSystemURL() {}

std::string FileSystemURL::spec() const {
  if (!is_valid_)
    return std::string();
  return GetFileSystemRootURI(origin_, type_).spec() + "/" +
      path_.AsUTF8Unsafe();
}

FileSystemURL FileSystemURL::WithPath(const FilePath& path) const {
  return FileSystemURL(origin(), type(), path);
}

bool FileSystemURL::operator==(const FileSystemURL& that) const {
  return origin_ == that.origin_ &&
      type_ == that.type_ &&
      path_ == that.path_ &&
      virtual_path_ == that.virtual_path_ &&
      filesystem_id_ == that.filesystem_id_ &&
      is_valid_ == that.is_valid_;
}

void FileSystemURL::MayCrackIsolatedPath() {
  path_ = virtual_path_;
  mount_type_ = type_;
  if (is_valid_ && IsolatedContext::IsIsolatedType(type_)) {
    // If the type is isolated, crack the path further to get the 'real'
    // filesystem type and path.
    is_valid_ = IsolatedContext::GetInstance()->CrackIsolatedPath(
        virtual_path_, &filesystem_id_, &type_, &path_);
  }
}

}  // namespace fileapi
