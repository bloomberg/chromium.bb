// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DATABASE_DATABASE_UTIL_H_
#define WEBKIT_DATABASE_DATABASE_UTIL_H_

#include "base/string16.h"
#include "googleurl/src/gurl.h"

class FilePath;

namespace webkit_database {

class DatabaseTracker;

class DatabaseUtil {
 public:
  // Extract various information from a database vfs_file_name.  All return
  // parameters are optional.
  static bool CrackVfsFileName(const string16& vfs_file_name,
                               string16* origin_identifier,
                               string16* database_name,
                               string16* sqlite_suffix);
  static FilePath GetFullFilePathForVfsFile(DatabaseTracker* db_tracker,
                                            const string16& vfs_file_name);
  static string16 GetOriginIdentifier(const GURL& url);
  static GURL GetOriginFromIdentifier(const string16& origin_identifier);
};

}  // namespace webkit_database

#endif  // WEBKIT_DATABASE_DATABASE_UTIL_H_
