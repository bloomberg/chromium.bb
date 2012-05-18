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
#include "net/base/escape.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "webkit/fileapi/file_system_types.h"

namespace fileapi {

const char kPersistentDir[] = "/persistent";
const char kTemporaryDir[] = "/temporary";
const char kIsolatedDir[] = "/isolated";
const char kExternalDir[] = "/external";
const char kTestDir[] = "/test";

const char kPersistentName[] = "Persistent";
const char kTemporaryName[] = "Temporary";
const char kIsolatedName[] = "Isolated";
const char kExternalName[] = "External";
const char kTestName[] = "Test";

bool CrackFileSystemURL(const GURL& url, GURL* origin_url, FileSystemType* type,
                        FilePath* file_path) {
  GURL origin;
  FileSystemType file_system_type = kFileSystemTypeUnknown;

  if (!url.is_valid() || !url.SchemeIsFileSystem())
    return false;
  DCHECK(url.inner_url());

  std::string inner_path = url.inner_url()->path();

  const struct {
    FileSystemType type;
    const char* dir;
  } kValidTypes[] = {
    { kFileSystemTypePersistent, kPersistentDir },
    { kFileSystemTypeTemporary, kTemporaryDir },
    { kFileSystemTypeIsolated, kIsolatedDir },
    { kFileSystemTypeExternal, kExternalDir },
    { kFileSystemTypeTest, kTestDir },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kValidTypes); ++i) {
    if (StartsWithASCII(inner_path, kValidTypes[i].dir, true)) {
      file_system_type = kValidTypes[i].type;
      break;
    }
  }

  if (file_system_type == kFileSystemTypeUnknown)
    return false;

  std::string path = net::UnescapeURLComponent(url.path(),
      net::UnescapeRule::SPACES | net::UnescapeRule::URL_SPECIAL_CHARS |
      net::UnescapeRule::CONTROL_CHARS);

  // Ensure the path is relative.
  while (!path.empty() && path[0] == '/')
    path.erase(0, 1);

  FilePath converted_path = FilePath::FromUTF8Unsafe(path);

  // All parent references should have been resolved in the renderer.
  if (converted_path.ReferencesParent())
    return false;

  if (origin_url)
    *origin_url = url.GetOrigin();
  if (type)
    *type = file_system_type;
  if (file_path)
    *file_path = converted_path.NormalizePathSeparators().
        StripTrailingSeparators();

  return true;
}

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
  case kFileSystemTypeUnknown:
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
    default:
      return kFileSystemTypeUnknown;
  }
}

quota::StorageType FileSystemTypeToQuotaStorageType(FileSystemType type) {
  switch (type) {
    case kFileSystemTypeTemporary:
      return quota::kStorageTypeTemporary;
    case kFileSystemTypePersistent:
      return quota::kStorageTypePersistent;
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
  GURL origin_url(web_security_origin.toString());

  // We need this work-around for file:/// URIs as
  // createFromDatabaseIdentifier returns empty origin_url for them.
  if (origin_url.spec().empty() &&
      origin_identifier.find("file__") == 0)
    return GURL("file:///");
  return origin_url;
}

std::string GetFileSystemTypeString(FileSystemType type) {
  switch (type) {
    case kFileSystemTypeTemporary:
      return fileapi::kTemporaryName;
    case kFileSystemTypePersistent:
      return fileapi::kPersistentName;
    case kFileSystemTypeExternal:
      return fileapi::kExternalName;
    case kFileSystemTypeTest:
      return fileapi::kTestName;
    case kFileSystemTypeUnknown:
    default:
      return std::string();
  }
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

}  // namespace fileapi
