// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_URL_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_URL_H_

#include <set>
#include <string>

#include "base/platform_file.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/storage/webkit_storage_export.h"

namespace fileapi {

// A class representing a filesystem URL which consists of origin URL,
// type and an internal path used inside the filesystem.
//
// When a FileSystemURL instance is created for a GURL (for filesystem: scheme),
// each accessor method would return following values:
//
// Example: For a URL 'filesystem:http://foo.com/temporary/foo/bar':
//   origin() returns 'http://foo.com',
//   type() and mount_type() return kFileSystemTypeTemporary,
//   path() returns 'foo/bar',
//
// All other accessors return empty or invalid value.
//
// FileSystemURL can also be created to represent a 'cracked' filesystem URL if
// the original URL's type/path is pointing to a mount point which can be
// further resolved to a lower filesystem type/path.
//
// Example: Assume a path '/media/removable' is mounted at mount name
// 'mount_name' with type kFileSystemTypeFoo as an external file system.
//
// The original URL would look like:
//     'filesystem:http://bar.com/external/mount_name/foo/bar':
//
// FileSystemURL('http://bar.com',
//               kFileSystemTypeExternal,
//               'mount_name/foo/bar'
//               'mount_name',
//               kFileSystemTypeFoo,
//               '/media/removable/foo/bar');
// would create a FileSystemURL whose accessors return:
//
//   origin() returns 'http://bar.com',
//   type() returns the kFileSystemTypeFoo,
//   path() returns '/media/removable/foo/bar',
//
// Additionally, following accessors would return valid values:
//   filesystem_id() returns 'mount_name', and
//   virtual_path() returns 'mount_name/foo/bar',
//   mount_type() returns kFileSystemTypeExternal.
//
// It is imposible to directly create a valid FileSystemURL instance (except by
// using CreatedForTest methods, which should not be used in production code).
// To get a valid FileSystemURL, one of the following methods can be used:
// <Friend>::CrackURL, <Friend>::CreateCrackedFileSystemURL, where <Friend> is
// one of the friended classes.
//
// TODO(ericu): Look into making path() [and all FileSystem API virtual
// paths] just an std::string, to prevent platform-specific FilePath behavior
// from getting invoked by accident. Currently the FilePath returned here needs
// special treatment, as it may contain paths that are illegal on the current
// platform.  To avoid problems, use VirtualPath::BaseName and
// VirtualPath::GetComponents instead of the FilePath methods.
class WEBKIT_STORAGE_EXPORT FileSystemURL {
 public:
  FileSystemURL();
  ~FileSystemURL();

  // Methods for creating FileSystemURL without attempting to crack them.
  // Should be used only in tests.
  static FileSystemURL CreateForTest(const GURL& url);
  static FileSystemURL CreateForTest(const GURL& origin,
                                     FileSystemType type,
                                     const FilePath& path);

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

  // Returns the filesystem ID/mount name for isolated/external filesystem URLs.
  // See the class comment for details.
  const std::string& filesystem_id() const { return filesystem_id_; }

  FileSystemType mount_type() const { return mount_type_; }

  std::string DebugString() const;

  // Returns a new FileSystemURL with the given path.
  // This creates a new FileSystemURL, copies all fields of this instance
  // to that one, resets the path_ to the given |path| and resets the
  // virtual_path to *empty*.
  // Note that the resulting FileSystemURL loses original URL information
  // if it was a cracked filesystem; i.e. virtual_path and mount_type will
  // be set to empty values.
  FileSystemURL WithPath(const FilePath& path) const;

  // Returns true if this URL is a strict parent of the |child|.
  bool IsParent(const FileSystemURL& child) const;

  bool operator==(const FileSystemURL& that) const;

  struct WEBKIT_STORAGE_EXPORT Comparator {
    bool operator() (const FileSystemURL& lhs, const FileSystemURL& rhs) const;
  };

 private:
  friend class FileSystemContext;
  friend class ExternalMountPoints;
  friend class IsolatedContext;

  explicit FileSystemURL(const GURL& filesystem_url);
  FileSystemURL(const GURL& origin,
                FileSystemType type,
                const FilePath& internal_path);
  // Creates a cracked FileSystemURL.
  FileSystemURL(const GURL& origin,
                FileSystemType original_type,
                const FilePath& original_path,
                const std::string& filesystem_id,
                FileSystemType cracked_type,
                const FilePath& cracked_path);

  bool is_valid_;

  GURL origin_;
  FileSystemType type_;
  FileSystemType mount_type_;
  FilePath path_;

  // Values specific to cracked URLs.
  std::string filesystem_id_;
  FilePath virtual_path_;

};

typedef std::set<FileSystemURL, FileSystemURL::Comparator> FileSystemURLSet;

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_URL_H_
