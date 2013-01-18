// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/syncable_file_system_util.h"

#include "webkit/fileapi/external_mount_points.h"
#include "webkit/fileapi/file_observers.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"

namespace fileapi {

bool RegisterSyncableFileSystem(const std::string& service_name) {
  return ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      service_name, kFileSystemTypeSyncable, FilePath());
}

bool RevokeSyncableFileSystem(const std::string& service_name) {
  return ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
      service_name);
}

GURL GetSyncableFileSystemRootURI(const GURL& origin,
                                  const std::string& service_name) {
  const GURL url = GetFileSystemRootURI(origin, kFileSystemTypeExternal);
  const std::string path = service_name + "/";
  url_canon::Replacements<char> replacements;
  replacements.SetPath(path.c_str(), url_parse::Component(0, path.length()));
  return url.ReplaceComponents(replacements);
}

FileSystemURL CreateSyncableFileSystemURL(const GURL& origin,
                                          const std::string& service_name,
                                          const FilePath& path) {
  return FileSystemURL(origin,
                       kFileSystemTypeExternal,
                       FilePath::FromUTF8Unsafe(service_name).Append(path));
}

bool SerializeSyncableFileSystemURL(const FileSystemURL& url,
                                    std::string* serialized_url) {
  if (!url.is_valid() || url.type() != kFileSystemTypeSyncable)
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

  const FileSystemURL deserialized_url = FileSystemURL(GURL(serialized_url));
  if (!deserialized_url.is_valid() ||
      deserialized_url.type() != kFileSystemTypeSyncable)
    return false;

  *url = deserialized_url;
  return true;
}

LocalFileSystemOperation*
CreateFileSystemOperationForSync(FileSystemContext* file_system_context) {
  DCHECK(file_system_context);
  return file_system_context->sandbox_provider()->
      CreateFileSystemOperationForSync(file_system_context);
}

}  // namespace fileapi
