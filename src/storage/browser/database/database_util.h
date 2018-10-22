// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_DATABASE_DATABASE_UTIL_H_
#define STORAGE_BROWSER_DATABASE_DATABASE_UTIL_H_

#include <string>
#include "base/strings/string16.h"
#include "storage/browser/storage_browser_export.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace storage {

class DatabaseTracker;

class STORAGE_EXPORT DatabaseUtil {
 public:
  static const char kJournalFileSuffix[];

  // Extract various information from a database vfs_file_name.  All return
  // parameters are optional.
  static bool CrackVfsFileName(const base::string16& vfs_file_name,
                               std::string* origin_identifier,
                               base::string16* database_name,
                               base::string16* sqlite_suffix);
  static base::FilePath GetFullFilePathForVfsFile(
      DatabaseTracker* db_tracker,
      const base::string16& vfs_file_name);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_DATABASE_DATABASE_UTIL_H_
