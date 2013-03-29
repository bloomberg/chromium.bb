// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/database/database_util.h"

#include "base/basictypes.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "webkit/database/database_tracker.h"
#include "webkit/database/vfs_backend.h"

namespace webkit_database {

const char DatabaseUtil::kJournalFileSuffix[] = "-journal";

bool DatabaseUtil::CrackVfsFileName(const base::string16& vfs_file_name,
                                    base::string16* origin_identifier,
                                    base::string16* database_name,
                                    base::string16* sqlite_suffix) {
  // 'vfs_file_name' is of the form <origin_identifier>/<db_name>#<suffix>.
  // <suffix> is optional.
  DCHECK(!vfs_file_name.empty());
  size_t first_slash_index = vfs_file_name.find('/');
  size_t last_pound_index = vfs_file_name.rfind('#');
  // '/' and '#' must be present in the string. Also, the string cannot start
  // with a '/' (origin_identifier cannot be empty) and '/' must come before '#'
  if ((first_slash_index == base::string16::npos) ||
      (last_pound_index == base::string16::npos) ||
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

base::FilePath DatabaseUtil::GetFullFilePathForVfsFile(
    DatabaseTracker* db_tracker, const base::string16& vfs_file_name) {
  base::string16 origin_identifier;
  base::string16 database_name;
  base::string16 sqlite_suffix;
  if (!CrackVfsFileName(vfs_file_name, &origin_identifier,
                        &database_name, &sqlite_suffix)) {
    return base::FilePath(); // invalid vfs_file_name
  }

  base::FilePath full_path = db_tracker->GetFullDBFilePath(
      origin_identifier, database_name);
  if (!full_path.empty() && !sqlite_suffix.empty()) {
    DCHECK(full_path.Extension().empty());
    full_path = full_path.InsertBeforeExtensionASCII(
        UTF16ToASCII(sqlite_suffix));
  }
  // Watch out for directory traversal attempts from a compromised renderer.
  if (full_path.value().find(FILE_PATH_LITERAL("..")) !=
          base::FilePath::StringType::npos)
    return base::FilePath();
  return full_path;
}

base::string16 DatabaseUtil::GetOriginIdentifier(const GURL& url) {
  base::string16 spec = UTF8ToUTF16(url.spec());
  return WebKit::WebSecurityOrigin::createFromString(spec).databaseIdentifier();
}

GURL DatabaseUtil::GetOriginFromIdentifier(
    const base::string16& origin_identifier) {
  WebKit::WebSecurityOrigin web_security_origin =
      WebKit::WebSecurityOrigin::createFromDatabaseIdentifier(
          origin_identifier);

  // We need this work-around for file:/// URIs as
  // createFromDatabaseIdentifier returns null origin_url for them.
  if (web_security_origin.isUnique()) {
    if (origin_identifier.find(UTF8ToUTF16("file__")) == 0)
      return GURL("file:///");
    return GURL();
  }

  return GURL(web_security_origin.toString());
}

bool DatabaseUtil::IsValidOriginIdentifier(
    const base::string16& origin_identifier) {
  base::string16 dotdot = ASCIIToUTF16("..");
  char16 forbidden[] = {'\\', '/', '\0'};

  base::string16::size_type pos = origin_identifier.find(dotdot);
  if (pos == base::string16::npos)
    pos = origin_identifier.find_first_of(forbidden, 0, arraysize(forbidden));

  return pos == base::string16::npos;
}

}  // namespace webkit_database
