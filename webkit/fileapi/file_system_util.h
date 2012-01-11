// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_UTIL_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_UTIL_H_
#pragma once

#include "webkit/fileapi/file_system_types.h"
#include "webkit/quota/quota_types.h"

class FilePath;
class GURL;

namespace fileapi {

extern const char kPersistentDir[];
extern const char kTemporaryDir[];
extern const char kExternalDir[];
extern const char kPersistentName[];
extern const char kTemporaryName[];
extern const char kExternalName[];

// Cracks the given filesystem |url| and populates |origin_url|, |type|
// and |file_path|.  Returns true if the given |url| is a valid filesystem
// url and the routine could successfully crack it, returns false otherwise.
// The file_path this returns will be using '/' as a path separator, no matter
// what platform you're on.
bool CrackFileSystemURL(const GURL& url,
                        GURL* origin_url,
                        FileSystemType* type,
                        FilePath* file_path);

// Returns the root URI of the filesystem that can be specified by a pair of
// |origin_url| and |type|.  The returned URI can be used as a root path
// of the filesystem (e.g. <returned_URI> + "/relative/path" will compose
// a path pointing to the entry "/relative/path" in the filesystem).
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

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_UTIL_H_
