// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_util.h"

#include "build/build_config.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "webkit/fileapi/file_system_types.h"

namespace fileapi {

const char kPersistentDir[] = "/persistent/";
const char kTemporaryDir[] = "/temporary/";
const char kExternalDir[] = "/external/";

const char kPersistentName[] = "Persistent";
const char kTemporaryName[] = "Temporary";
const char kExternalName[] = "External";

bool CrackFileSystemURL(const GURL& url, GURL* origin_url, FileSystemType* type,
                        FilePath* file_path) {
  GURL origin;
  FileSystemType file_system_type;

  if (url.scheme() != "filesystem")
    return false;

  std::string temp = url.path();
  // TODO(ericu): This should probably be done elsewhere after the stackable
  // layers are properly in.  We're supposed to reject any paths that contain
  // '..' segments, but the GURL constructor is helpfully resolving them for us.
  // Make sure there aren't any before we call it.
  size_t pos = temp.find("..");
  for (; pos != std::string::npos; pos = temp.find("..", pos + 1)) {
    if ((pos == 0 || temp[pos - 1] == '/') &&
        (pos == temp.length() - 2 || temp[pos + 2] == '/'))
      return false;
  }

  // bare_url will look something like:
  //    http://example.com/temporary/dir/file.txt.
  GURL bare_url(temp);

  // The input URL was malformed, bail out early.
  if (bare_url.path().empty())
    return false;

  origin = bare_url.GetOrigin();

  // The input URL was malformed, bail out early.
  if (origin.is_empty())
    return false;

  std::string path = net::UnescapeURLComponent(bare_url.path(),
      net::UnescapeRule::SPACES | net::UnescapeRule::URL_SPECIAL_CHARS |
      net::UnescapeRule::CONTROL_CHARS);
  if (path.compare(0, strlen(kPersistentDir), kPersistentDir) == 0) {
    file_system_type = kFileSystemTypePersistent;
    path = path.substr(strlen(kPersistentDir));
  } else if (path.compare(0, strlen(kTemporaryDir), kTemporaryDir) == 0) {
    file_system_type = kFileSystemTypeTemporary;
    path = path.substr(strlen(kTemporaryDir));
  } else if (path.compare(0, strlen(kExternalDir), kExternalDir) == 0) {
    file_system_type = kFileSystemTypeExternal;
    path = path.substr(strlen(kExternalDir));
  } else {
    return false;
  }

  // Ensure the path is relative.
  while (!path.empty() && path[0] == '/')
    path.erase(0, 1);

  if (origin_url)
    *origin_url = origin;
  if (type)
    *type = file_system_type;
  if (file_path)
#if defined(OS_WIN)
    *file_path = FilePath(base::SysUTF8ToWide(path)).
        NormalizeWindowsPathSeparators().StripTrailingSeparators();
#elif defined(OS_POSIX)
    *file_path = FilePath(path).StripTrailingSeparators();
#endif

  return true;
}

GURL GetFileSystemRootURI(
    const GURL& origin_url, fileapi::FileSystemType type) {
  std::string path("filesystem:");
  path += origin_url.spec();
  switch (type) {
  case kFileSystemTypeTemporary:
    path += (kTemporaryDir + 1);  // We don't want the leading slash.
    break;
  case kFileSystemTypePersistent:
    path += (kPersistentDir + 1);  // We don't want the leading slash.
    break;
  case kFileSystemTypeExternal:
    path += (kExternalDir + 1);  // We don't want the leading slash.
    break;
  default:
    NOTREACHED();
    return GURL();
  }
  return GURL(path);
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

}  // namespace fileapi
