// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_url.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/escape.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"

namespace fileapi {
namespace {
bool CrackFileSystemURL(
    const GURL& url,
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

bool FileSystemURL::Comparator::operator()(const FileSystemURL& lhs,
                                           const FileSystemURL& rhs) const {
  DCHECK(lhs.is_valid_ && rhs.is_valid_);
  if (lhs.origin_ != rhs.origin_)
    return lhs.origin_ < rhs.origin_;
  if (lhs.type_ != rhs.type_)
    return lhs.type_ < rhs.type_;
  // Compares the virtual path, i.e. the path() part of the original URL
  // so rhs this reflects the virtual path relationship (rather than
  // rhs of cracked paths).
  return lhs.virtual_path_ < rhs.virtual_path_;
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
