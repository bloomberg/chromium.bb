// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_origin_database.h"

#include <set>

#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "webkit/fileapi/file_system_util.h"

namespace {

const FilePath::CharType kOriginDatabaseName[] = FILE_PATH_LITERAL("Origins");
const char kOriginKeyPrefix[] = "ORIGIN:";
const char kLastPathKey[] = "LAST_PATH";
const int64 kMinimumReportIntervalHours = 1;
const char kInitStatusHistogramLabel[] = "FileSystem.OriginDatabaseInit";

enum InitStatus {
  INIT_STATUS_OK = 0,
  INIT_STATUS_CORRUPTION,
  INIT_STATUS_IO_ERROR,
  INIT_STATUS_UNKNOWN_ERROR,
  INIT_STATUS_MAX
};

std::string OriginToOriginKey(const std::string& origin) {
  std::string key(kOriginKeyPrefix);
  return key + origin;
}

const char* LastPathKey() {
  return kLastPathKey;
}

}  // namespace

namespace fileapi {

FileSystemOriginDatabase::OriginRecord::OriginRecord() {
}

FileSystemOriginDatabase::OriginRecord::OriginRecord(
    const std::string& origin_in, const FilePath& path_in)
    : origin(origin_in), path(path_in) {
}

FileSystemOriginDatabase::OriginRecord::~OriginRecord() {
}

FileSystemOriginDatabase::FileSystemOriginDatabase(
    const FilePath& file_system_directory)
    : file_system_directory_(file_system_directory) {
}

FileSystemOriginDatabase::~FileSystemOriginDatabase() {
}

bool FileSystemOriginDatabase::Init(RecoveryOption recovery_option) {
  if (db_.get())
    return true;

  std::string path =
      FilePathToString(file_system_directory_.Append(kOriginDatabaseName));
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::DB* db;
  leveldb::Status status = leveldb::DB::Open(options, path, &db);
  ReportInitStatus(status);
  if (status.ok()) {
    db_.reset(db);
    return true;
  }
  HandleError(FROM_HERE, status);

  if (!status.IsCorruption())
    return false;

  switch (recovery_option) {
    case FAIL_ON_CORRUPTION:
      return false;
    case REPAIR_ON_CORRUPTION:
      LOG(WARNING) << "Attempting to repair FileSystemOriginDatabase.";
      if (RepairDatabase(path)) {
        LOG(WARNING) << "Repairing FileSystemOriginDatabase completed.";
        return true;
      }
      // fall through
    case DELETE_ON_CORRUPTION:
      if (!file_util::Delete(file_system_directory_, true))
        return false;
      if (!file_util::CreateDirectory(file_system_directory_))
        return false;
      return Init(FAIL_ON_CORRUPTION);
  }
  NOTREACHED();
  return false;
}

bool FileSystemOriginDatabase::RepairDatabase(const std::string& db_path) {
  DCHECK(!db_.get());
  if (!leveldb::RepairDB(db_path, leveldb::Options()).ok() ||
      !Init(FAIL_ON_CORRUPTION)) {
    LOG(WARNING) << "Failed to repair FileSystemOriginDatabase.";
    return false;
  }

  // See if the repaired entries match with what we have on disk.
  std::set<FilePath> directories;
  file_util::FileEnumerator file_enum(file_system_directory_,
                                      false /* recursive */,
                                      file_util::FileEnumerator::DIRECTORIES);
  FilePath path_each;
  while (!(path_each = file_enum.Next()).empty())
    directories.insert(path_each.BaseName());
  std::set<FilePath>::iterator db_dir_itr =
      directories.find(FilePath(kOriginDatabaseName));
  // Make sure we have the database file in its directory and therefore we are
  // working on the correct path.
  DCHECK(db_dir_itr != directories.end());
  directories.erase(db_dir_itr);

  std::vector<OriginRecord> origins;
  if (!ListAllOrigins(&origins)) {
    DropDatabase();
    return false;
  }

  // Delete any obsolete entries from the origins database.
  for (std::vector<OriginRecord>::iterator db_origin_itr = origins.begin();
       db_origin_itr != origins.end();
       ++db_origin_itr) {
    std::set<FilePath>::iterator dir_itr =
        directories.find(db_origin_itr->path);
    if (dir_itr == directories.end()) {
      if (!RemovePathForOrigin(db_origin_itr->origin)) {
        DropDatabase();
        return false;
      }
    } else {
      directories.erase(dir_itr);
    }
  }

  // Delete any directories not listed in the origins database.
  for (std::set<FilePath>::iterator dir_itr = directories.begin();
       dir_itr != directories.end();
       ++dir_itr) {
    if (!file_util::Delete(file_system_directory_.Append(*dir_itr),
                           true /* recursive */)) {
      DropDatabase();
      return false;
    }
  }

  return true;
}

void FileSystemOriginDatabase::HandleError(
    const tracked_objects::Location& from_here,
    const leveldb::Status& status) {
  db_.reset();
  LOG(ERROR) << "FileSystemOriginDatabase failed at: "
             << from_here.ToString() << " with error: " << status.ToString();
}

void FileSystemOriginDatabase::ReportInitStatus(const leveldb::Status& status) {
  base::Time now = base::Time::Now();
  base::TimeDelta minimum_interval =
      base::TimeDelta::FromHours(kMinimumReportIntervalHours);
  if (last_reported_time_ + minimum_interval >= now)
    return;
  last_reported_time_ = now;

  if (status.ok()) {
    UMA_HISTOGRAM_ENUMERATION(kInitStatusHistogramLabel,
                              INIT_STATUS_OK, INIT_STATUS_MAX);
  } else if (status.IsCorruption()) {
    UMA_HISTOGRAM_ENUMERATION(kInitStatusHistogramLabel,
                              INIT_STATUS_CORRUPTION, INIT_STATUS_MAX);
  } else if (status.IsIOError()) {
    UMA_HISTOGRAM_ENUMERATION(kInitStatusHistogramLabel,
                              INIT_STATUS_IO_ERROR, INIT_STATUS_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kInitStatusHistogramLabel,
                              INIT_STATUS_UNKNOWN_ERROR, INIT_STATUS_MAX);
  }
}

bool FileSystemOriginDatabase::HasOriginPath(const std::string& origin) {
  if (!Init(REPAIR_ON_CORRUPTION))
    return false;
  if (origin.empty())
    return false;
  std::string path;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), OriginToOriginKey(origin), &path);
  if (status.ok())
    return true;
  if (status.IsNotFound())
    return false;
  HandleError(FROM_HERE, status);
  return false;
}

bool FileSystemOriginDatabase::GetPathForOrigin(
    const std::string& origin, FilePath* directory) {
  if (!Init(REPAIR_ON_CORRUPTION))
    return false;
  DCHECK(directory);
  if (origin.empty())
    return false;
  std::string path_string;
  std::string origin_key = OriginToOriginKey(origin);
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), origin_key, &path_string);
  if (status.IsNotFound()) {
    int last_path_number;
    if (!GetLastPathNumber(&last_path_number))
      return false;
    path_string = StringPrintf("%03u", last_path_number + 1);
    // store both back as a single transaction
    leveldb::WriteBatch batch;
    batch.Put(LastPathKey(), path_string);
    batch.Put(origin_key, path_string);
    status = db_->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
      HandleError(FROM_HERE, status);
      return false;
    }
  }
  if (status.ok()) {
    *directory = StringToFilePath(path_string);
    return true;
  }
  HandleError(FROM_HERE, status);
  return false;
}

bool FileSystemOriginDatabase::RemovePathForOrigin(const std::string& origin) {
  if (!Init(REPAIR_ON_CORRUPTION))
    return false;
  leveldb::Status status =
      db_->Delete(leveldb::WriteOptions(), OriginToOriginKey(origin));
  if (status.ok() || status.IsNotFound())
    return true;
  HandleError(FROM_HERE, status);
  return false;
}

bool FileSystemOriginDatabase::ListAllOrigins(
    std::vector<OriginRecord>* origins) {
  if (!Init(REPAIR_ON_CORRUPTION))
    return false;
  DCHECK(origins);
  scoped_ptr<leveldb::Iterator> iter(db_->NewIterator(leveldb::ReadOptions()));
  std::string origin_key_prefix = OriginToOriginKey("");
  iter->Seek(origin_key_prefix);
  origins->clear();
  while (iter->Valid() &&
      StartsWithASCII(iter->key().ToString(), origin_key_prefix, true)) {
    std::string origin =
      iter->key().ToString().substr(origin_key_prefix.length());
    FilePath path = StringToFilePath(iter->value().ToString());
    origins->push_back(OriginRecord(origin, path));
    iter->Next();
  }
  return true;
}

void FileSystemOriginDatabase::DropDatabase() {
  db_.reset();
}

bool FileSystemOriginDatabase::GetLastPathNumber(int* number) {
  if (!Init(REPAIR_ON_CORRUPTION))
    return false;
  DCHECK(number);
  std::string number_string;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), LastPathKey(), &number_string);
  if (status.ok())
    return base::StringToInt(number_string, number);
  if (!status.IsNotFound()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  // Verify that this is a totally new database, and initialize it.
  scoped_ptr<leveldb::Iterator> iter(db_->NewIterator(leveldb::ReadOptions()));
  iter->SeekToFirst();
  if (iter->Valid()) {  // DB was not empty, but had no last path number!
    LOG(ERROR) << "File system origin database is corrupt!";
    return false;
  }
  // This is always the first write into the database.  If we ever add a
  // version number, they should go in in a single transaction.
  status =
      db_->Put(leveldb::WriteOptions(), LastPathKey(), std::string("-1"));
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  *number = -1;
  return true;
}

}  // namespace fileapi
