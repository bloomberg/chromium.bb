// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_util.h"

#include "build/build_config.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "webkit/fileapi/file_system_types.h"

namespace fileapi {

const char kPersistentDir[] = "/persistent";
const char kTemporaryDir[] = "/temporary";
const char kIsolatedDir[] = "/isolated";
const char kExternalDir[] = "/external";
const char kTestDir[] = "/test";

// TODO(ericu): Consider removing support for '\', even on Windows, if possible.
// There's a lot of test code that will need reworking, and we may have trouble
// with FilePath elsewhere [e.g. DirName and other methods may also need
// replacement].
FilePath VirtualPath::BaseName(const FilePath& virtual_path) {
  FilePath::StringType path = virtual_path.value();

  // Keep everything after the final separator, but if the pathname is only
  // one character and it's a separator, leave it alone.
  while (path.size() > 1 && FilePath::IsSeparator(path[path.size() - 1]))
    path.resize(path.size() - 1);
  FilePath::StringType::size_type last_separator =
      path.find_last_of(FilePath::kSeparators);
  if (last_separator != FilePath::StringType::npos &&
      last_separator < path.size() - 1)
    path.erase(0, last_separator + 1);

  return FilePath(path);
}

void VirtualPath::GetComponents(
    const FilePath& path, std::vector<FilePath::StringType>* components) {
  DCHECK(components);
  if (!components)
    return;
  components->clear();
  if (path.value().empty())
    return;

  std::vector<FilePath::StringType> ret_val;
  FilePath current = path;
  FilePath base;

  // Due to the way things are implemented, FilePath::DirName works here,
  // whereas FilePath::BaseName doesn't.
  while (current != current.DirName()) {
    base = BaseName(current);
    ret_val.push_back(base.value());
    current = current.DirName();
  }

  *components =
      std::vector<FilePath::StringType>(ret_val.rbegin(), ret_val.rend());
}

GURL GetFileSystemRootURI(const GURL& origin_url, FileSystemType type) {
  // origin_url is based on a security origin, so http://foo.com or file:///
  // instead of the corresponding filesystem URL.
  DCHECK(!origin_url.SchemeIsFileSystem());

  std::string url = "filesystem:" + origin_url.GetWithEmptyPath().spec();
  switch (type) {
  case kFileSystemTypeTemporary:
    url += (kTemporaryDir + 1);  // We don't want the leading slash.
    return GURL(url + "/");
  case kFileSystemTypePersistent:
    url += (kPersistentDir + 1);  // We don't want the leading slash.
    return GURL(url + "/");
  case kFileSystemTypeExternal:
    url += (kExternalDir + 1);  // We don't want the leading slash.
    return GURL(url + "/");
  case kFileSystemTypeIsolated:
    url += (kIsolatedDir + 1);  // We don't want the leading slash.
    return GURL(url + "/");
  case kFileSystemTypeTest:
    url += (kTestDir + 1);  // We don't want the leading slash.
    return GURL(url + "/");
  // Internal types are always pointed via isolated or external URLs.
  default:
    NOTREACHED();
  }
  NOTREACHED();
  return GURL();
}

std::string GetFileSystemName(const GURL& origin_url, FileSystemType type) {
  std::string origin_identifier = GetOriginIdentifierFromURL(origin_url);
  std::string type_string = GetFileSystemTypeString(type);
  DCHECK(!type_string.empty());
  return origin_identifier + ":" + type_string;
}

FileSystemType QuotaStorageTypeToFileSystemType(
    quota::StorageType storage_type) {
  switch (storage_type) {
    case quota::kStorageTypeTemporary:
      return kFileSystemTypeTemporary;
    case quota::kStorageTypePersistent:
      return kFileSystemTypePersistent;
    case quota::kStorageTypeSyncable:
      return kFileSystemTypeSyncable;
    case quota::kStorageTypeUnknown:
      return kFileSystemTypeUnknown;
  }
  return kFileSystemTypeUnknown;
}

quota::StorageType FileSystemTypeToQuotaStorageType(FileSystemType type) {
  switch (type) {
    case kFileSystemTypeTemporary:
      return quota::kStorageTypeTemporary;
    case kFileSystemTypePersistent:
      return quota::kStorageTypePersistent;
    case kFileSystemTypeSyncable:
      return quota::kStorageTypeSyncable;
    default:
      return quota::kStorageTypeUnknown;
  }
}

// TODO(kinuko): Merge these two methods (conversion methods between
// origin url <==> identifier) with the ones in the database module.
// http://crbug.com/116476
std::string GetOriginIdentifierFromURL(const GURL& url) {
  WebKit::WebSecurityOrigin web_security_origin =
      WebKit::WebSecurityOrigin::createFromString(UTF8ToUTF16(url.spec()));
  return web_security_origin.databaseIdentifier().utf8();
}

GURL GetOriginURLFromIdentifier(const std::string& origin_identifier) {
  WebKit::WebSecurityOrigin web_security_origin =
      WebKit::WebSecurityOrigin::createFromDatabaseIdentifier(
          UTF8ToUTF16(origin_identifier));

  // We need this work-around for file:/// URIs as
  // createFromDatabaseIdentifier returns null origin_url for them.
  if (web_security_origin.isUnique()) {
    if (origin_identifier.find("file__") == 0)
      return GURL("file:///");
    return GURL();
  }

  return GURL(web_security_origin.toString());
}

std::string GetFileSystemTypeString(FileSystemType type) {
  switch (type) {
    case kFileSystemTypeTemporary:
      return "Temporary";
    case kFileSystemTypePersistent:
      return "Persistent";
    case kFileSystemTypeIsolated:
      return "Isolated";
    case kFileSystemTypeExternal:
      return "External";
    case kFileSystemTypeTest:
      return "Test";
    case kFileSystemTypeNativeLocal:
      return "NativeLocal";
    case kFileSystemTypeRestrictedNativeLocal:
      return "RestrictedNativeLocal";
    case kFileSystemTypeDragged:
      return "Dragged";
    case kFileSystemTypeNativeMedia:
      return "NativeMedia";
    case kFileSystemTypeDeviceMedia:
      return "DeviceMedia";
    case kFileSystemTypeDrive:
      return "Drive";
    case kFileSystemTypeSyncable:
      return "Syncable";
    case kFileSystemTypeUnknown:
      return "Unknown";
  }
  NOTREACHED();
  return std::string();
}

std::string FilePathToString(const FilePath& file_path) {
#if defined(OS_WIN)
  return UTF16ToUTF8(file_path.value());
#elif defined(OS_POSIX)
  return file_path.value();
#endif
}

FilePath StringToFilePath(const std::string& file_path_string) {
#if defined(OS_WIN)
  return FilePath(UTF8ToUTF16(file_path_string));
#elif defined(OS_POSIX)
  return FilePath(file_path_string);
#endif
}

WebKit::WebFileError PlatformFileErrorToWebFileError(
    base::PlatformFileError error_code) {
  switch (error_code) {
    case base::PLATFORM_FILE_ERROR_NOT_FOUND:
      return WebKit::WebFileErrorNotFound;
    case base::PLATFORM_FILE_ERROR_INVALID_OPERATION:
    case base::PLATFORM_FILE_ERROR_EXISTS:
    case base::PLATFORM_FILE_ERROR_NOT_EMPTY:
      return WebKit::WebFileErrorInvalidModification;
    case base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY:
    case base::PLATFORM_FILE_ERROR_NOT_A_FILE:
      return WebKit::WebFileErrorTypeMismatch;
    case base::PLATFORM_FILE_ERROR_ACCESS_DENIED:
      return WebKit::WebFileErrorNoModificationAllowed;
    case base::PLATFORM_FILE_ERROR_FAILED:
      return WebKit::WebFileErrorInvalidState;
    case base::PLATFORM_FILE_ERROR_ABORT:
      return WebKit::WebFileErrorAbort;
    case base::PLATFORM_FILE_ERROR_SECURITY:
      return WebKit::WebFileErrorSecurity;
    case base::PLATFORM_FILE_ERROR_NO_SPACE:
      return WebKit::WebFileErrorQuotaExceeded;
    default:
      return WebKit::WebFileErrorInvalidModification;
  }
}

std::string GetIsolatedFileSystemName(const GURL& origin_url,
                                      const std::string& filesystem_id) {
  std::string name(fileapi::GetFileSystemName(origin_url,
      fileapi::kFileSystemTypeIsolated));
  name.append("_");
  name.append(filesystem_id);
  return name;
}

bool CrackIsolatedFileSystemName(const std::string& filesystem_name,
                                 std::string* filesystem_id) {
  DCHECK(filesystem_id);

  // |filesystem_name| is of the form {origin}:isolated_{filesystem_id}.
  std::string start_token(":");
  start_token = start_token.append(
      GetFileSystemTypeString(kFileSystemTypeIsolated)).append("_");
  // WebKit uses different case in its constant for isolated file system
  // names, so we do a case insensitive compare by converting both strings
  // to uppercase.
  // TODO(benwells): Remove this when WebKit uses the same constant.
  start_token = StringToUpperASCII(start_token);
  std::string filesystem_name_upper = StringToUpperASCII(filesystem_name);
  size_t pos = filesystem_name_upper.find(start_token);
  if (pos == std::string::npos)
    return false;
  if (pos == 0)
    return false;

  *filesystem_id = filesystem_name.substr(pos + start_token.length(),
                                          std::string::npos);
  if (filesystem_id->empty())
    return false;

  return true;
}

std::string GetIsolatedFileSystemRootURIString(
    const GURL& origin_url,
    const std::string& filesystem_id,
    const std::string& optional_root_name) {
  std::string root = GetFileSystemRootURI(origin_url,
                                          kFileSystemTypeIsolated).spec();
  root.append(filesystem_id);
  root.append("/");
  if (!optional_root_name.empty()) {
    DCHECK(!FilePath::FromUTF8Unsafe(optional_root_name).ReferencesParent());
    root.append(optional_root_name);
    root.append("/");
  }
  return root;
}

}  // namespace fileapi
