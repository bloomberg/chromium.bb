// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_UTIL_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_UTIL_H_
#pragma once

#include <string>
#include <vector>

#include "base/file_path.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/quota/quota_types.h"

class GURL;

namespace fileapi {

extern const char kPersistentDir[];
extern const char kTemporaryDir[];
extern const char kExternalDir[];
extern const char kIsolatedDir[];
extern const char kPersistentName[];
extern const char kTemporaryName[];
extern const char kExternalName[];
extern const char kIsolatedName[];

// Cracks the given filesystem |url| and populates |origin_url|, |type|
// and |file_path|.  Returns true if the given |url| is a valid filesystem
// url and the routine could successfully crack it, returns false otherwise.
// The file_path this returns will be using '/' as a path separator, no matter
// what platform you're on.
// TODO(ericu): Look into making file_path [and all FileSystem API virtual
// paths] just an std::string, to prevent platform-specific FilePath behavior
// from getting invoked by accident.  Currently the FilePath returned here needs
// special treatment, as it may contain paths that are illegal on the current
// platform.  To avoid problems, use VirtualPath::BaseName and
// VirtualPath::GetComponents instead of the FilePath methods.
bool CrackFileSystemURL(const GURL& url,
                        GURL* origin_url,
                        FileSystemType* type,
                        FilePath* file_path);

class VirtualPath {
 public:
  // Use this instead of FilePath::BaseName when operating on virtual paths.
  // FilePath::BaseName will get confused by ':' on Windows when it looks like a
  // drive letter separator; this will treat it as just another character.
  static FilePath BaseName(const FilePath& virtual_path);

  // Likewise, use this instead of FilePath::GetComponents when operating on
  // virtual paths.
  // Note that this assumes very clean input, with no leading slash, and it will
  // not evaluate '.' or '..' components.
  static void GetComponents(const FilePath& path,
      std::vector<FilePath::StringType>* components);
};

// Returns the root URI of the filesystem that can be specified by a pair of
// |origin_url| and |type|.  The returned URI can be used as a root path
// of the filesystem (e.g. <returned_URI> + "/relative/path" will compose
// a path pointing to the entry "/relative/path" in the filesystem).
//
// For Isolated filesystem this returns the 'common' root part, e.g.
// returns URL without the filesystem ID.
GURL GetFileSystemRootURI(const GURL& origin_url, FileSystemType type);

// Returns the name for the filesystem that is specified by a pair of
// |origin_url| and |type|.
// (The name itself is neither really significant nor a formal identifier
// but can be read as the .name field of the returned FileSystem object
// as a user-friendly name in the javascript layer).
//
// Example:
//   The name for a TEMPORARY filesystem of "http://www.example.com:80/"
//   should look like: "http_www.example.host_80:temporary"
std::string GetFileSystemName(const GURL& origin_url, FileSystemType type);

// Converts FileSystemType |type| to/from the StorageType |storage_type| that
// is used for the unified quota system.
// (Basically this naively maps TEMPORARY storage type to TEMPORARY filesystem
// type, PERSISTENT storage type to PERSISTENT filesystem type and vice versa.)
FileSystemType QuotaStorageTypeToFileSystemType(
    quota::StorageType storage_type);
quota::StorageType FileSystemTypeToQuotaStorageType(FileSystemType type);

// Returns the origin identifier string for the given |url| and vice versa.
// The origin identifier string is a serialized form of a security origin
// and can be used as a path name as it contains no "/" or other possibly
// unsafe characters. (See WebKit's SecurityOrigin code for more details.)
//
// Example:
//   "http://www.example.com:80/"'s identifier should look like:
//   "http_www.example.host_80"
std::string GetOriginIdentifierFromURL(const GURL& url);
GURL GetOriginURLFromIdentifier(const std::string& origin_identifier);

// Returns the string representation of the given filesystem |type|.
// Returns an empty string if the |type| is invalid.
std::string GetFileSystemTypeString(FileSystemType type);

// Encodes |file_path| to a string.
// Following conditions should be held:
//  - StringToFilePath(FilePathToString(path)) == path
//  - StringToFilePath(FilePathToString(path) + "/" + "SubDirectory") ==
//    path.AppendASCII("SubDirectory");
//
// TODO(tzik): Replace CreateFilePath and FilePathToString in
// third_party/leveldatabase/env_chromium.cc with them.
std::string FilePathToString(const FilePath& file_path);

// Decode a file path from |file_path_string|.
FilePath StringToFilePath(const std::string& file_path_string);

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_UTIL_H_
