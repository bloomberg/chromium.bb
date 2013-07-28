// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/sandbox_isolated_origin_database.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "webkit/browser/fileapi/sandbox_origin_database.h"

namespace fileapi {

// Special directory name for isolated origin.
const base::FilePath::CharType
SandboxIsolatedOriginDatabase::kOriginDirectory[] = FILE_PATH_LITERAL("iso");

SandboxIsolatedOriginDatabase::SandboxIsolatedOriginDatabase(
    const std::string& origin,
    const base::FilePath& file_system_directory)
    : migration_checked_(false),
      origin_(origin),
      file_system_directory_(file_system_directory) {
}

SandboxIsolatedOriginDatabase::~SandboxIsolatedOriginDatabase() {
}

bool SandboxIsolatedOriginDatabase::HasOriginPath(
    const std::string& origin) {
  MigrateDatabaseIfNeeded();
  return (origin_ == origin);
}

bool SandboxIsolatedOriginDatabase::GetPathForOrigin(
    const std::string& origin, base::FilePath* directory) {
  MigrateDatabaseIfNeeded();
  if (origin != origin_)
    return false;
  *directory = base::FilePath(kOriginDirectory);
  return true;
}

bool SandboxIsolatedOriginDatabase::RemovePathForOrigin(
    const std::string& origin) {
  return true;
}

bool SandboxIsolatedOriginDatabase::ListAllOrigins(
    std::vector<OriginRecord>* origins) {
  MigrateDatabaseIfNeeded();
  origins->push_back(OriginRecord(origin_, base::FilePath(kOriginDirectory)));
  return true;
}

void SandboxIsolatedOriginDatabase::DropDatabase() {
}

void SandboxIsolatedOriginDatabase::MigrateBackDatabase(
    const std::string& origin,
    const base::FilePath& file_system_directory,
    SandboxOriginDatabase* database) {
  base::FilePath isolated_directory =
      file_system_directory.Append(kOriginDirectory);

  if (database->HasOriginPath(origin)) {
    // Don't bother.
    base::DeleteFile(isolated_directory, true /* recursive */);
    return;
  }

  base::FilePath directory_name;
  if (database->GetPathForOrigin(origin, &directory_name)) {
    base::FilePath origin_directory =
        file_system_directory.Append(directory_name);
    base::DeleteFile(origin_directory, true /* recursive */);
    base::Move(isolated_directory, origin_directory);
  }
}

void SandboxIsolatedOriginDatabase::MigrateDatabaseIfNeeded() {
  if (migration_checked_)
    return;

  migration_checked_ = true;
  // See if we have non-isolated version of sandbox database.
  scoped_ptr<SandboxOriginDatabase> database(
      new SandboxOriginDatabase(file_system_directory_));
  if (!database->HasOriginPath(origin_))
    return;

  base::FilePath directory_name;
  if (database->GetPathForOrigin(origin_, &directory_name) &&
      directory_name != base::FilePath(kOriginDirectory)) {
    base::FilePath from_path = file_system_directory_.Append(directory_name);
    base::FilePath to_path = file_system_directory_.Append(kOriginDirectory);

    if (base::PathExists(to_path))
      base::DeleteFile(to_path, true /* recursive */);
    base::Move(from_path, to_path);
  }

  database->RemoveDatabase();
}

}  // namespace fileapi
