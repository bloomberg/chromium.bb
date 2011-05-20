// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_origin_database.h"

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "third_party/leveldb/include/leveldb/iterator.h"
#include "third_party/leveldb/include/leveldb/write_batch.h"

namespace {

const char kOriginKeyPrefix[] = "ORIGIN:";
const char kLastPathKey[] = "LAST_PATH";

std::string OriginToOriginKey(const std::string& origin) {
  std::string key(kOriginKeyPrefix);
  return key + origin;
}

const char* LastPathKey() {
  return kLastPathKey;
}

}

namespace fileapi {

FileSystemOriginDatabase::FileSystemOriginDatabase(const FilePath& path) {
#if defined(OS_POSIX)
  path_ = path.value();
#elif defined(OS_WIN)
  path_ = base::SysWideToUTF8(path.value());
#endif
}

FileSystemOriginDatabase::~FileSystemOriginDatabase() {
}

bool FileSystemOriginDatabase::Init() {
  if (db_.get())
    return true;

  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::DB* db;
  leveldb::Status status = leveldb::DB::Open(options, path_, &db);
  if (status.ok()) {
    db_.reset(db);
    return true;
  }
  HandleError(status);
  return false;
}

void FileSystemOriginDatabase::HandleError(leveldb::Status status) {
  db_.reset();
  LOG(ERROR) << "FileSystemOriginDatabase failed with error: " <<
      status.ToString();
}

bool FileSystemOriginDatabase::HasOriginPath(const std::string& origin) {
  if (!Init())
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
  HandleError(status);
  return false;
}

bool FileSystemOriginDatabase::GetPathForOrigin(
    const std::string& origin, FilePath* directory) {
  if (!Init())
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
      HandleError(status);
      return false;
    }
  }
  if (status.ok()) {
#if defined(OS_POSIX)
    *directory = FilePath(path_string);
#elif defined(OS_WIN)
    *directory = FilePath(base::SysUTF8ToWide(path_string));
#endif
    return true;
  }
  HandleError(status);
  return false;
}

bool FileSystemOriginDatabase::RemovePathForOrigin(const std::string& origin) {
  if (!Init())
    return false;
  leveldb::Status status =
      db_->Delete(leveldb::WriteOptions(), OriginToOriginKey(origin));
  if (status.ok() || status.IsNotFound())
    return true;
  HandleError(status);
  return false;
}

bool FileSystemOriginDatabase::ListAllOrigins(
    std::vector<OriginRecord>* origins) {
  if (!Init())
    return false;
  DCHECK(origins);
  scoped_ptr<leveldb::Iterator> iter(db_->NewIterator(leveldb::ReadOptions()));
  std::string origin_key_prefix = OriginToOriginKey("");
  iter->Seek(origin_key_prefix);
  origins->clear();
  while(iter->Valid() &&
      StartsWithASCII(iter->key().ToString(), origin_key_prefix, true)) {
    std::string origin =
      iter->key().ToString().substr(origin_key_prefix.length());
#if defined(OS_POSIX)
    FilePath path = FilePath(iter->value().ToString());
#elif defined(OS_WIN)
    FilePath path = FilePath(base::SysUTF8ToWide(iter->value().ToString()));
#endif
    origins->push_back(OriginRecord(origin, path));
    iter->Next();
  }
  return true;
}

void FileSystemOriginDatabase::DropDatabase() {
  db_.reset();
}

bool FileSystemOriginDatabase::GetLastPathNumber(int* number) {
  if (!Init())
    return false;
  DCHECK(number);
  std::string number_string;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), LastPathKey(), &number_string);
  if (status.ok())
    return base::StringToInt(number_string, number);
  if (!status.IsNotFound()) {
    HandleError(status);
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
    HandleError(status);
    return false;
  }
  *number = -1;
  return true;
}

}  // namespace fileapi
