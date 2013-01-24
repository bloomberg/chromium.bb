// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_url.h"

#include <sstream>

#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/escape.h"
#include "webkit/fileapi/external_mount_points.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"

namespace fileapi {

namespace {

bool ParseFileSystemURL(const GURL& url,
                        GURL* origin_url,
                        FileSystemType* type,
                        FilePath* file_path) {
  GURL origin;
  FileSystemType file_system_type = kFileSystemTypeUnknown;

  if (!url.is_valid() || !url.SchemeIsFileSystem())
    return false;
  DCHECK(url.inner_url());

  std::string inner_path = url.inner_url()->path();

  const struct {
    FileSystemType type;
    const char* dir;
  } kValidTypes[] = {
    { kFileSystemTypePersistent, kPersistentDir },
    { kFileSystemTypeTemporary, kTemporaryDir },
    { kFileSystemTypeIsolated, kIsolatedDir },
    { kFileSystemTypeExternal, kExternalDir },
    { kFileSystemTypeTest, kTestDir },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kValidTypes); ++i) {
    if (StartsWithASCII(inner_path, kValidTypes[i].dir, true)) {
      file_system_type = kValidTypes[i].type;
      break;
    }
  }

  if (file_system_type == kFileSystemTypeUnknown)
    return false;

  std::string path = net::UnescapeURLComponent(url.path(),
      net::UnescapeRule::SPACES | net::UnescapeRule::URL_SPECIAL_CHARS |
      net::UnescapeRule::CONTROL_CHARS);

  // Ensure the path is relative.
  while (!path.empty() && path[0] == '/')
    path.erase(0, 1);

  FilePath converted_path = FilePath::FromUTF8Unsafe(path);

  // All parent references should have been resolved in the renderer.
  if (converted_path.ReferencesParent())
    return false;

  if (origin_url)
    *origin_url = url.GetOrigin();
  if (type)
    *type = file_system_type;
  if (file_path)
    *file_path = converted_path.NormalizePathSeparators().
        StripTrailingSeparators();

  return true;
}

}  // namespace

FileSystemURL::FileSystemURL()
    : is_valid_(false),
      type_(kFileSystemTypeUnknown),
      mount_type_(kFileSystemTypeUnknown) {
}

// static
FileSystemURL FileSystemURL::CreateForTest(const GURL& url) {
  return FileSystemURL(url);
}

FileSystemURL FileSystemURL::CreateForTest(const GURL& origin,
                                           FileSystemType type,
                                           const FilePath& path) {
  return FileSystemURL(origin, type, path);
}

FileSystemURL::FileSystemURL(const GURL& url)
    : type_(kFileSystemTypeUnknown),
      mount_type_(kFileSystemTypeUnknown) {
  is_valid_ = ParseFileSystemURL(url, &origin_, &type_, &path_);
  mount_type_ = type_;
}

FileSystemURL::FileSystemURL(const GURL& origin,
                             FileSystemType type,
                             const FilePath& path)
    : is_valid_(true),
      origin_(origin),
      type_(type),
      mount_type_(type),
      path_(path.NormalizePathSeparators()) {
}

FileSystemURL::FileSystemURL(const GURL& origin,
                             FileSystemType original_type,
                             const FilePath& original_path,
                             const std::string& filesystem_id,
                             FileSystemType cracked_type,
                             const FilePath& cracked_path)
    : is_valid_(true),
      origin_(origin),
      type_(cracked_type),
      mount_type_(original_type),
      path_(cracked_path.NormalizePathSeparators()),
      filesystem_id_(filesystem_id),
      virtual_path_(original_path.NormalizePathSeparators()) {
}

FileSystemURL::~FileSystemURL() {}

std::string FileSystemURL::DebugString() const {
  if (!is_valid_)
    return "invalid filesystem: URL";
  std::ostringstream ss;
  ss << GetFileSystemRootURI(origin_, mount_type_);

  // filesystem_id_ will be non empty for (and only for) cracked URLs.
  if (!filesystem_id_.empty()) {
    ss << virtual_path_.value();
    ss << " (";
    ss << GetFileSystemTypeString(type_) << "@" << filesystem_id_ << ":";
    ss << path_.value();
    ss << ")";
  } else {
    ss << path_.value();
  }
  return ss.str();
}

FileSystemURL FileSystemURL::WithPath(const FilePath& path) const {
  FileSystemURL url = *this;
  url.path_ = path;
  url.virtual_path_.clear();
  return url;
}

bool FileSystemURL::IsParent(const FileSystemURL& child) const {
  return origin() == child.origin() &&
         type() == child.type() &&
         filesystem_id() == child.filesystem_id() &&
         path().IsParent(child.path());
}

bool FileSystemURL::operator==(const FileSystemURL& that) const {
  return origin_ == that.origin_ &&
      type_ == that.type_ &&
      path_ == that.path_ &&
      filesystem_id_ == that.filesystem_id_ &&
      is_valid_ == that.is_valid_;
}

bool FileSystemURL::Comparator::operator()(const FileSystemURL& lhs,
                                           const FileSystemURL& rhs) const {
  DCHECK(lhs.is_valid_ && rhs.is_valid_);
  if (lhs.origin_ != rhs.origin_)
    return lhs.origin_ < rhs.origin_;
  if (lhs.type_ != rhs.type_)
    return lhs.type_ < rhs.type_;
  if (lhs.filesystem_id_ != rhs.filesystem_id_)
    return lhs.filesystem_id_ < rhs.filesystem_id_;
  return lhs.path_ < rhs.path_;
}

}  // namespace fileapi
