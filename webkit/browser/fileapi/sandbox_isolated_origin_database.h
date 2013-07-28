// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_SANDBOX_ISOLATED_ORIGIN_DATABASE_H_
#define WEBKIT_BROWSER_FILEAPI_SANDBOX_ISOLATED_ORIGIN_DATABASE_H_

#include "webkit/browser/fileapi/sandbox_origin_database_interface.h"

namespace fileapi {

class SandboxOriginDatabase;

class WEBKIT_STORAGE_BROWSER_EXPORT_PRIVATE SandboxIsolatedOriginDatabase
    : public SandboxOriginDatabaseInterface {
 public:
  static const base::FilePath::CharType kOriginDirectory[];

  explicit SandboxIsolatedOriginDatabase(
      const std::string& origin,
      const base::FilePath& file_system_directory);
  virtual ~SandboxIsolatedOriginDatabase();

  // SandboxOriginDatabaseInterface overrides.
  virtual bool HasOriginPath(const std::string& origin) OVERRIDE;
  virtual bool GetPathForOrigin(const std::string& origin,
                                base::FilePath* directory) OVERRIDE;
  virtual bool RemovePathForOrigin(const std::string& origin) OVERRIDE;
  virtual bool ListAllOrigins(std::vector<OriginRecord>* origins) OVERRIDE;
  virtual void DropDatabase() OVERRIDE;

  static void MigrateBackDatabase(
      const std::string& origin,
      const base::FilePath& file_system_directory,
      SandboxOriginDatabase* origin_database);

 private:
  void MigrateDatabaseIfNeeded();

  bool migration_checked_;
  const std::string origin_;
  const base::FilePath file_system_directory_;

  DISALLOW_COPY_AND_ASSIGN(SandboxIsolatedOriginDatabase);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_SANDBOX_ISOLATED_ORIGIN_DATABASE_H_
