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
//
// When a FileSystemURL instance is created for regular sandbox file systems
// each accessor method would return following values:
//
// Example: For a URL 'filesystem:http://foo.com/temporary/foo/bar':
//   origin() returns 'http://foo.com',
//   type() and mount_type() return kFileSystemTypeTemporary,
//   path() and virtual_path() return 'foo/bar', and
//   filesystem_id() returns an empty string.
//
// path() and virtual_path() usually return the same value, but they
// have different values if an instance is created for Isolated or External
// FileSystem URL, for which we may mount different paths from its exposed
// virtual paths.
//
// Example: Assume a path '/media/removable' is mounted at mount name
// 'mount_name' with type kFileSystemTypeFoo as an external file system.
// For a URL 'filesystem:http://bar.com/external/mount_name/foo/bar':
//   origin() returns 'http://bar.com',
//   type() returns the kFileSystemTypeFoo,
//   path() returns '/media/removable/foo/bar',
//   virtual_path() returns 'mount_name/foo/bar',
//   filesystem_id() returns 'mount_name', and
//   mount_type() returns kFileSystemTypeExternal.
//
// TODO(ericu): Look into making path() [and all FileSystem API virtual
// paths] just an std::string, to prevent platform-specific FilePath behavior
// from getting invoked by accident. Currently the FilePath returned here needs
// special treatment, as it may contain paths that are illegal on the current
// platform.  To avoid problems, use VirtualPath::BaseName and
// VirtualPath::GetComponents instead of the FilePath methods.
class FILEAPI_EXPORT FileSystemURL {
 public:
  FileSystemURL();
  explicit FileSystemURL(const GURL& filesystem_url);
  FileSystemURL(const GURL& origin,
                FileSystemType type,
                const FilePath& internal_path);
  ~FileSystemURL();

  // Returns true if this instance represents a valid FileSystem URL.
  bool is_valid() const { return is_valid_; }

  // Returns the origin part of this URL. See the class comment for details.
  const GURL& origin() const { return origin_; }

  // Returns the type part of this URL. See the class comment for details.
  FileSystemType type() const { return type_; }

  // Returns the path part of this URL. See the class comment for details.
  // TODO(kinuko): this must return std::string.
  const FilePath& path() const { return path_; }

  // Returns the original path part of this URL.
  // See the class comment for details.
  // TODO(kinuko): this must return std::string.
  const FilePath& virtual_path() const { return virtual_path_; }

  // Returns the filesystem ID/name for isolated/external file system URLs.
  // See the class comment for details.
  const std::string& filesystem_id() const { return filesystem_id_; }

  // Returns the public file system type of this URL.
  // See the class comment for details.
  FileSystemType mount_type() const { return mount_type_; }

  std::string spec() const;

  // Returns a new FileSystemURL with the given path.
  // This doesn't change the calling instance's data.
  FileSystemURL WithPath(const FilePath& path) const;

  bool operator==(const FileSystemURL& that) const;

  struct FILEAPI_EXPORT Comparator {
    bool operator() (const FileSystemURL& lhs, const FileSystemURL& rhs) const;
  };

 private:
  void MayCrackIsolatedPath();

  GURL origin_;
  FileSystemType type_;
  FilePath path_;

  // For isolated filesystem.
  std::string filesystem_id_;
  FilePath virtual_path_;
  FileSystemType mount_type_;

  bool is_valid_;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_URL_H_
