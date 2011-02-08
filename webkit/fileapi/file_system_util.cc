// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_util.h"

#include "build/build_config.h"

#include "base/file_path.h"
#include "base/sys_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "webkit/fileapi/file_system_types.h"

namespace fileapi {

static const char kPersistentDir[] = "/persistent/";
static const char kTemporaryDir[] = "/temporary/";

bool CrackFileSystemURL(const GURL& url, GURL* origin_url, FileSystemType* type,
                        FilePath* file_path) {
  *origin_url = GURL();
  *type = kFileSystemTypeUnknown;
  *file_path = FilePath();

  if (url.scheme() != "filesystem")
    return false;

  GURL bare_url(url.path());
  *origin_url = bare_url.GetOrigin();

  // The input URL was malformed, bail out early.
  if (origin_url->is_empty() || bare_url.path().empty())
    return false;

  std::string path = UnescapeURLComponent(bare_url.path(),
      UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS);
  if (path.compare(0, strlen(kPersistentDir), kPersistentDir) == 0) {
    *type = kFileSystemTypePersistent;
    path = path.substr(strlen(kPersistentDir));
  } else if (path.compare(0, strlen(kTemporaryDir), kTemporaryDir) == 0) {
    *type = kFileSystemTypeTemporary;
    path = path.substr(strlen(kTemporaryDir));
  } else {
    return false;
  }

  // Ensure the path is relative.
  while (!path.empty() && path[0] == '/')
    path.erase(0, 1);

#if defined(OS_WIN)
  const FilePath::StringType& sys_path = base::SysUTF8ToWide(path);
#elif defined(OS_POSIX)
  const FilePath::StringType& sys_path = path;
#endif

  *file_path = FilePath(sys_path);
  return true;
}

}  // namespace fileapi
