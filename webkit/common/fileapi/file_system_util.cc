// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/fileapi/file_system_util.h"

#include <algorithm>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "url/gurl.h"
#include "webkit/common/database/database_identifier.h"

namespace fileapi {

const char kPersistentDir[] = "/persistent";
const char kTemporaryDir[] = "/temporary";
const char kIsolatedDir[] = "/isolated";
const char kExternalDir[] = "/external";
const char kTestDir[] = "/test";

const base::FilePath::CharType VirtualPath::kRoot[] = FILE_PATH_LITERAL("/");
const base::FilePath::CharType VirtualPath::kSeparator = FILE_PATH_LITERAL('/');

// TODO(ericu): Consider removing support for '\', even on Windows, if possible.
// There's a lot of test code that will need reworking, and we may have trouble
// with base::FilePath elsewhere [e.g. DirName and other methods may also need
// replacement].
base::FilePath VirtualPath::BaseName(const base::FilePath& virtual_path) {
  base::FilePath::StringType path = virtual_path.value();

  // Keep everything after the final separator, but if the pathname is only
  // one character and it's a separator, leave it alone.
  while (path.size() > 1 && base::FilePath::IsSeparator(path[path.size() - 1]))
    path.resize(path.size() - 1);
  base::FilePath::StringType::size_type last_separator =
      path.find_last_of(base::FilePath::kSeparators);
  if (last_separator != base::FilePath::StringType::npos &&
      last_separator < path.size() - 1)
    path.erase(0, last_separator + 1);

  return base::FilePath(path);
}

base::FilePath VirtualPath::DirName(const base::FilePath& virtual_path) {
  typedef base::FilePath::StringType StringType;
  StringType  path = virtual_path.value();

  // The logic below is taken from that of base::FilePath::DirName, except
  // that this version never cares about '//' or drive-letters even on win32.

  // Strip trailing separators.
  while (path.size() > 1 && base::FilePath::IsSeparator(path[path.size() - 1]))
    path.resize(path.size() - 1);

  StringType::size_type last_separator =
      path.find_last_of(base::FilePath::kSeparators);
  if (last_separator == StringType::npos) {
    // path_ is in the current directory.
    return base::FilePath(base::FilePath::kCurrentDirectory);
  }
  if (last_separator == 0) {
    // path_ is in the root directory.
    return base::FilePath(path.substr(0, 1));
  }
  // path_ is somewhere else, trim the basename.
  path.resize(last_separator);

  // Strip trailing separators.
  while (path.size() > 1 && base::FilePath::IsSeparator(path[path.size() - 1]))
    path.resize(path.size() - 1);

  if (path.empty())
    return base::FilePath(base::FilePath::kCurrentDirectory);

  return base::FilePath(path);
}

void VirtualPath::GetComponents(
    const base::FilePath& path,
    std::vector<base::FilePath::StringType>* components) {
  typedef base::FilePath::StringType StringType;

  DCHECK(components);
  if (!components)
    return;
  components->clear();
  if (path.value().empty())
    return;

  StringType::size_type begin = 0, end = 0;
  while (begin < path.value().length() && end != StringType::npos) {
    end = path.value().find_first_of(base::FilePath::kSeparators, begin);
    StringType component = path.value().substr(
        begin, end == StringType::npos ? StringType::npos : end - begin);
    if (!component.empty() && component != base::FilePath::kCurrentDirectory)
      components->push_back(component);
    begin = end + 1;
  }
}

void VirtualPath::GetComponentsUTF8Unsafe(
    const base::FilePath& path,
    std::vector<std::string>* components) {
  DCHECK(components);
  if (!components)
    return;
  components->clear();

  std::vector<base::FilePath::StringType> stringtype_components;
  VirtualPath::GetComponents(path, &stringtype_components);
  std::vector<base::FilePath::StringType>::const_iterator it;
  for (it = stringtype_components.begin(); it != stringtype_components.end();
       ++it) {
    components->push_back(base::FilePath(*it).AsUTF8Unsafe());
  }
}

base::FilePath::StringType VirtualPath::GetNormalizedFilePath(
    const base::FilePath& path) {
  base::FilePath::StringType normalized_path = path.value();
  const size_t num_separators = base::FilePath::StringType(
      base::FilePath::kSeparators).length();
  for (size_t i = 0; i < num_separators; ++i) {
    std::replace(normalized_path.begin(), normalized_path.end(),
                 base::FilePath::kSeparators[i], kSeparator);
  }

  return (IsAbsolute(normalized_path)) ?
      normalized_path : base::FilePath::StringType(kRoot) + normalized_path;
}

bool VirtualPath::IsAbsolute(const base::FilePath::StringType& path) {
  return path.find(kRoot) == 0;
}

bool VirtualPath::IsRootPath(const base::FilePath& path) {
  std::vector<base::FilePath::StringType> components;
  VirtualPath::GetComponents(path, &components);
  return (path.empty() || components.empty() ||
          (components.size() == 1 &&
           components[0] == VirtualPath::kRoot));
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
  std::string origin_identifier =
      webkit_database::GetIdentifierFromOrigin(origin_url);
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
    case kFileSystemTypeSyncableForInternalSync:
      return quota::kStorageTypeSyncable;
    default:
      return quota::kStorageTypeUnknown;
  }
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
    case kFileSystemTypePicasa:
      return "Picasa";
    case kFileSystemTypeItunes:
      return "Itunes";
    case kFileSystemTypeDrive:
      return "Drive";
    case kFileSystemTypeSyncable:
    case kFileSystemTypeSyncableForInternalSync:
      return "Syncable";
    case kFileSystemTypeNativeForPlatformApp:
      return "NativeForPlatformApp";
    case kFileSystemTypeForTransientFile:
      return "TransientFile";
    case kFileSystemInternalTypeEnumStart:
    case kFileSystemInternalTypeEnumEnd:
      NOTREACHED();
      // Fall through.
    case kFileSystemTypeUnknown:
      return "Unknown";
  }
  NOTREACHED();
  return std::string();
}

std::string FilePathToString(const base::FilePath& file_path) {
#if defined(OS_WIN)
  return UTF16ToUTF8(file_path.value());
#elif defined(OS_POSIX)
  return file_path.value();
#endif
}

base::FilePath StringToFilePath(const std::string& file_path_string) {
#if defined(OS_WIN)
  return base::FilePath(UTF8ToUTF16(file_path_string));
#elif defined(OS_POSIX)
  return base::FilePath(file_path_string);
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

bool GetFileSystemPublicType(
    const std::string type_string,
    WebKit::WebFileSystemType* type
) {
  DCHECK(type);
  if (type_string == "Temporary") {
    *type = WebKit::WebFileSystemTypeTemporary;
    return true;
  }
  if (type_string == "Persistent") {
    *type = WebKit::WebFileSystemTypePersistent;
    return true;
  }
  if (type_string == "Isolated") {
    *type = WebKit::WebFileSystemTypeIsolated;
    return true;
  }
  if (type_string == "External") {
    *type = WebKit::WebFileSystemTypeExternal;
    return true;
  }
  NOTREACHED();
  return false;
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
  if (base::FilePath::FromUTF8Unsafe(filesystem_id).ReferencesParent())
    return std::string();
  root.append(filesystem_id);
  root.append("/");
  if (!optional_root_name.empty()) {
    if (base::FilePath::FromUTF8Unsafe(optional_root_name).ReferencesParent())
      return std::string();
    root.append(optional_root_name);
    root.append("/");
  }
  return root;
}

std::string GetExternalFileSystemRootURIString(
    const GURL& origin_url,
    const std::string& mount_name) {
  std::string root = GetFileSystemRootURI(origin_url,
                                          kFileSystemTypeExternal).spec();
  if (base::FilePath::FromUTF8Unsafe(mount_name).ReferencesParent())
    return std::string();
  root.append(mount_name);
  root.append("/");
  return root;
}

}  // namespace fileapi
