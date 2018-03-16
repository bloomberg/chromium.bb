// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/database/database_util.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/database/vfs_backend.h"
#include "storage/common/database/database_identifier.h"

namespace storage {

namespace {

bool IsSafeSuffix(const base::string16& suffix) {
  base::char16 prev_c = 0;
  for (const base::char16 c : suffix) {
    if (!(base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) || c == '-' ||
          c == '.' || c == '_')) {
      return false;
    }
    if (c == '.' && prev_c == '.')
      return false;
    prev_c = c;
  }
  return true;
}

}

const char DatabaseUtil::kJournalFileSuffix[] = "-journal";

bool DatabaseUtil::CrackVfsFileName(const base::string16& vfs_file_name,
                                    std::string* origin_identifier,
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

  std::string origin_id = base::UTF16ToASCII(
        vfs_file_name.substr(0, first_slash_index));
  if (!IsValidOriginIdentifier(origin_id))
    return false;

  base::string16 suffix = vfs_file_name.substr(
      last_pound_index + 1, vfs_file_name.length() - last_pound_index - 1);
  if (!IsSafeSuffix(suffix))
    return false;

  if (origin_identifier)
    *origin_identifier = origin_id;

  if (database_name) {
    *database_name = vfs_file_name.substr(
        first_slash_index + 1, last_pound_index - first_slash_index - 1);
  }

  if (sqlite_suffix)
    *sqlite_suffix = suffix;

  return true;
}

base::FilePath DatabaseUtil::GetFullFilePathForVfsFile(
    DatabaseTracker* db_tracker, const base::string16& vfs_file_name) {
  std::string origin_identifier;
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
        base::UTF16ToASCII(sqlite_suffix));
  }
  // Watch out for directory traversal attempts from a compromised renderer.
  if (full_path.value().find(FILE_PATH_LITERAL("..")) !=
          base::FilePath::StringType::npos)
    return base::FilePath();
  return full_path;
}

}  // namespace storage
