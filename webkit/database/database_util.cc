// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/database/database_util.h"

#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "webkit/database/database_tracker.h"
#include "webkit/database/vfs_backend.h"

namespace webkit_database {

bool DatabaseUtil::CrackVfsFileName(const string16& vfs_file_name,
                                    string16* origin_identifier,
                                    string16* database_name,
                                    string16* sqlite_suffix) {
  // 'vfs_file_name' is of the form <origin_identifier>/<db_name>#<suffix>.
  // <suffix> is optional.
  DCHECK(!vfs_file_name.empty());
  size_t first_slash_index = vfs_file_name.find('/');
  size_t last_pound_index = vfs_file_name.rfind('#');
  // '/' and '#' must be present in the string. Also, the string cannot start
  // with a '/' (origin_identifier cannot be empty) and '/' must come before '#'
  if ((first_slash_index == string16::npos) ||
      (last_pound_index == string16::npos) ||
      (first_slash_index == 0) ||
      (first_slash_index > last_pound_index)) {
    return false;
  }

  if (origin_identifier)
    *origin_identifier = vfs_file_name.substr(0, first_slash_index);
  if (database_name) {
    *database_name = vfs_file_name.substr(
        first_slash_index + 1, last_pound_index - first_slash_index - 1);
  }
  if (sqlite_suffix) {
    *sqlite_suffix = vfs_file_name.substr(
        last_pound_index + 1, vfs_file_name.length() - last_pound_index - 1);
  }
  return true;
}

FilePath DatabaseUtil::GetFullFilePathForVfsFile(
    DatabaseTracker* db_tracker, const string16& vfs_file_name) {
  string16 origin_identifier;
  string16 database_name;
  string16 sqlite_suffix;
  if (!CrackVfsFileName(vfs_file_name, &origin_identifier,
                        &database_name, &sqlite_suffix)) {
    return FilePath(); // invalid vfs_file_name
  }

  FilePath full_path = db_tracker->GetFullDBFilePath(
      origin_identifier, database_name);
  if (!full_path.empty() && !sqlite_suffix.empty()) {
    DCHECK(full_path.Extension().empty());
    full_path = full_path.InsertBeforeExtensionASCII(
        UTF16ToASCII(sqlite_suffix));
  }
  // Watch out for directory traversal attempts from a compromised renderer.
  if (full_path.value().find(FILE_PATH_LITERAL("..")) !=
          FilePath::StringType::npos)
    return FilePath();
  return full_path;
}

string16 DatabaseUtil::GetOriginIdentifier(const GURL& url) {
  string16 spec = UTF8ToUTF16(url.spec());
  return WebKit::WebSecurityOrigin::createFromString(spec).databaseIdentifier();
}

GURL DatabaseUtil::GetOriginFromIdentifier(const string16& origin_identifier) {
  GURL origin(WebKit::WebSecurityOrigin::createFromDatabaseIdentifier(
      origin_identifier).toString());
  DCHECK(origin == origin.GetOrigin());
  return origin;
}

}  // namespace webkit_database
