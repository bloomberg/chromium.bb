// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_PATH_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_PATH_H_

#include "base/file_path.h"
#include "base/platform_file.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/fileapi_export.h"
#include "webkit/fileapi/file_system_types.h"

namespace fileapi {

class FileSystemFileUtil;

// A class representing a filesystem path which consists of origin URL,
// type and an internal path used inside the filesystem. The class also has
// a corresponding FileSystemFileUtil by which the filesystem file is
// handled.
// NOTE: If we completely get rid of cross-filesystem operations under
// FileSystemOperation we may end up converting all the FileSystemPath
// occurences back to a FilePath's.
class FILEAPI_EXPORT_PRIVATE FileSystemPath {
 public:
  FileSystemPath();
  FileSystemPath(const GURL& origin,
                 FileSystemType type,
                 const FilePath& internal_path);
  ~FileSystemPath();

  // Gets and sets the origin URL of this filesystem.
  const GURL& origin() const { return origin_; }
  void set_origin(const GURL& origin) { origin_ = origin; }

  // Gets and sets the type of this filesystem (e.g. FileSystemTypeTemporary).
  FileSystemType type() const { return type_; }
  void set_type(FileSystemType type) { type_ = type; }

  // Gets and sets the internal path of this filesystem, which is the path
  // used to specify the file in the filesystem.  The value could be either
  // virtual or platform path depending on filesystem types and corresponding
  // file_util's.
  const FilePath& internal_path() const { return internal_path_; }
  void set_internal_path(const FilePath& internal_path) {
    internal_path_ = internal_path;
  }

  // Returns a new FileSystemPath with the given internal_path.
  // This doesn't change the calling instance's data.
  FileSystemPath WithInternalPath(const FilePath& internal_path) const;

  // Path related convenient methods.
  FileSystemPath BaseName() const {
    return WithInternalPath(internal_path().BaseName());
  }
  FileSystemPath DirName() const {
    return WithInternalPath(internal_path().DirName());
  }
  FileSystemPath Append(const FilePath& component) const {
    return WithInternalPath(internal_path().Append(component));
  }
  FileSystemPath Append(const FilePath::StringType& component) const {
    return WithInternalPath(internal_path().Append(component));
  }
  FileSystemPath AppendASCII(const base::StringPiece& component) const {
    return WithInternalPath(internal_path().AppendASCII(component));
  }
  bool IsParent(const FileSystemPath& other) const {
    return internal_path_.IsParent(other.internal_path());
  }

  bool operator==(const FileSystemPath& that) const;

 private:
  GURL origin_;
  FileSystemType type_;
  FilePath internal_path_;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_PATH_H_
