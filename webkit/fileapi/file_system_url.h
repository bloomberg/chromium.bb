// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_URL_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_URL_H_

#include <string>

#include "base/file_path.h"
#include "base/platform_file.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/fileapi_export.h"

namespace fileapi {

// A class representing a filesystem URL which consists of origin URL,
// type and an internal path used inside the filesystem.
class FILEAPI_EXPORT FileSystemURL {
 public:
  FileSystemURL();
  explicit FileSystemURL(const GURL& filesystem_url);
  FileSystemURL(const GURL& origin,
                FileSystemType type,
                const FilePath& internal_path);
  ~FileSystemURL();

  bool is_valid() const { return is_valid_; }
  const GURL& origin() const { return origin_; }
  FileSystemType type() const { return type_; }

  // TODO(kinuko): this must be std::string.
  const FilePath& path() const { return path_; }

  std::string spec() const;

  // Returns a new FileSystemURL with the given path.
  // This doesn't change the calling instance's data.
  FileSystemURL WithPath(const FilePath& path) const;

  bool operator==(const FileSystemURL& that) const;

 private:
  GURL origin_;
  FileSystemType type_;
  FilePath path_;
  bool is_valid_;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_URL_H_
