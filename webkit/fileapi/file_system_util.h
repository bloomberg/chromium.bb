// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

// The file_path this returns will be using '/' as a path separator, no matter
// what platform you're on.
bool CrackFileSystemURL(const GURL& url, GURL* origin_url, FileSystemType* type,
                        FilePath* file_path);

GURL GetFileSystemRootURI(const GURL& origin_url, fileapi::FileSystemType type);

FileSystemType QuotaStorageTypeToFileSystemType(
    quota::StorageType storage_type);

quota::StorageType FileSystemTypeToQuotaStorageType(FileSystemType type);

std::string GetOriginIdentifierFromURL(const GURL& url);
GURL GetOriginURLFromIdentifier(const std::string& origin_identifier);

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_UTIL_H_
