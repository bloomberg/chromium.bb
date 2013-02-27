// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/syncable_file_system_util.h"

#include "webkit/fileapi/external_mount_points.h"
#include "webkit/fileapi/file_observers.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"

using fileapi::ExternalMountPoints;
using fileapi::FileSystemContext;
using fileapi::FileSystemURL;
using fileapi::LocalFileSystemOperation;

namespace sync_file_system {

bool RegisterSyncableFileSystem(const std::string& service_name) {
  return ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      service_name, fileapi::kFileSystemTypeSyncable, base::FilePath());
}

bool RevokeSyncableFileSystem(const std::string& service_name) {
  return ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
      service_name);
}

GURL GetSyncableFileSystemRootURI(const GURL& origin,
                                  const std::string& service_name) {
  const GURL url = GetFileSystemRootURI(origin,
                                        fileapi::kFileSystemTypeExternal);
  const std::string path = service_name + "/";
  url_canon::Replacements<char> replacements;
  replacements.SetPath(path.c_str(), url_parse::Component(0, path.length()));
  return url.ReplaceComponents(replacements);
}

FileSystemURL CreateSyncableFileSystemURL(const GURL& origin,
                                          const std::string& service_name,
                                          const base::FilePath& path) {
  return ExternalMountPoints::GetSystemInstance()->CreateCrackedFileSystemURL(
      origin,
      fileapi::kFileSystemTypeExternal,
      base::FilePath::FromUTF8Unsafe(service_name).Append(path));
}

bool SerializeSyncableFileSystemURL(const FileSystemURL& url,
                                    std::string* serialized_url) {
  if (!url.is_valid() || url.type() != fileapi::kFileSystemTypeSyncable)
    return false;
  *serialized_url =
      GetSyncableFileSystemRootURI(url.origin(), url.filesystem_id()).spec() +
      url.path().AsUTF8Unsafe();
  return true;
}

bool DeserializeSyncableFileSystemURL(
    const std::string& serialized_url, FileSystemURL* url) {
#if !defined(FILE_PATH_USES_WIN_SEPARATORS)
  DCHECK(serialized_url.find('\\') == std::string::npos);
#endif  // FILE_PATH_USES_WIN_SEPARATORS

  FileSystemURL deserialized =
      ExternalMountPoints::GetSystemInstance()->CrackURL(GURL(serialized_url));
  if (!deserialized.is_valid() ||
      deserialized.type() != fileapi::kFileSystemTypeSyncable) {
    return false;
  }

  *url = deserialized;
  return true;
}

LocalFileSystemOperation* CreateFileSystemOperationForSync(
    FileSystemContext* file_system_context) {
  DCHECK(file_system_context);
  return file_system_context->sandbox_provider()->
      CreateFileSystemOperationForSync(file_system_context);
}

}  // namespace sync_file_system
