// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DATABASE_DATABASE_UTIL_H_
#define WEBKIT_DATABASE_DATABASE_UTIL_H_

#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "webkit/storage/webkit_storage_export.h"

namespace base {
class FilePath;
}

namespace webkit_database {

class DatabaseTracker;

class WEBKIT_STORAGE_EXPORT DatabaseUtil {
 public:
  static const char kJournalFileSuffix[];

  // Extract various information from a database vfs_file_name.  All return
  // parameters are optional.
  static bool CrackVfsFileName(const base::string16& vfs_file_name,
                               base::string16* origin_identifier,
                               base::string16* database_name,
                               base::string16* sqlite_suffix);
  static base::FilePath GetFullFilePathForVfsFile(
      DatabaseTracker* db_tracker,
      const base::string16& vfs_file_name);
  static base::string16 GetOriginIdentifier(const GURL& url);
  static GURL GetOriginFromIdentifier(const base::string16& origin_identifier);
  static bool IsValidOriginIdentifier(const base::string16& origin_identifier);
};

}  // namespace webkit_database

#endif  // WEBKIT_DATABASE_DATABASE_UTIL_H_
