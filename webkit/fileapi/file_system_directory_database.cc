// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_directory_database.h"

#include <math.h>

#include "base/file_util.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/pickle.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "webkit/fileapi/file_system_util.h"

namespace {

bool PickleFromFileInfo(
    const fileapi::FileSystemDirectoryDatabase::FileInfo& info,
    Pickle* pickle) {
  DCHECK(pickle);
  std::string data_path;
  // Round off here to match the behavior of the filesystem on real files.
  base::Time time =
      base::Time::FromDoubleT(floor(info.modification_time.ToDoubleT()));
  std::string name;

  data_path = fileapi::FilePathToString(info.data_path);
  name = fileapi::FilePathToString(FilePath(info.name));

  if (pickle->WriteInt64(info.parent_id) &&
      pickle->WriteString(data_path) &&
      pickle->WriteString(name) &&
      pickle->WriteInt64(time.ToInternalValue()))
    return true;

  NOTREACHED();
  return false;
}

bool FileInfoFromPickle(
    const Pickle& pickle,
    fileapi::FileSystemDirectoryDatabase::FileInfo* info) {
  PickleIterator iter(pickle);
  std::string data_path;
  std::string name;
  int64 internal_time;

  if (pickle.ReadInt64(&iter, &info->parent_id) &&
      pickle.ReadString(&iter, &data_path) &&
      pickle.ReadString(&iter, &name) &&
      pickle.ReadInt64(&iter, &internal_time)) {

    info->data_path = fileapi::StringToFilePath(data_path);
    info->name = fileapi::StringToFilePath(name).value();
    info->modification_time = base::Time::FromInternalValue(internal_time);
    return true;
  }
  LOG(ERROR) << "Pickle could not be digested!";
  return false;
}

const FilePath::CharType kDirectoryDatabaseName[] = FILE_PATH_LITERAL("Paths");
const char kChildLookupPrefix[] = "CHILD_OF:";
const char kChildLookupSeparator[] = ":";
const char kLastFileIdKey[] = "LAST_FILE_ID";
const char kLastIntegerKey[] = "LAST_INTEGER";
const int64 kMinimumReportIntervalHours = 1;
const char kInitStatusHistogramLabel[] = "FileSystem.DirectoryDatabaseInit";

enum InitStatus {
  INIT_STATUS_OK = 0,
  INIT_STATUS_CORRUPTION,
  INIT_STATUS_MAX
};

std::string GetChildLookupKey(
    fileapi::FileSystemDirectoryDatabase::FileId parent_id,
    const FilePath::StringType& child_name) {
  std::string name;
  name = fileapi::FilePathToString(FilePath(child_name));
  return std::string(kChildLookupPrefix) + base::Int64ToString(parent_id) +
      std::string(kChildLookupSeparator) + name;
}

std::string GetChildListingKeyPrefix(
    fileapi::FileSystemDirectoryDatabase::FileId parent_id) {
  return std::string(kChildLookupPrefix) + base::Int64ToString(parent_id) +
      std::string(kChildLookupSeparator);
}

const char* LastFileIdKey() {
  return kLastFileIdKey;
}

const char* LastIntegerKey() {
  return kLastIntegerKey;
}

std::string GetFileLookupKey(
    fileapi::FileSystemDirectoryDatabase::FileId file_id) {
  return base::Int64ToString(file_id);
}

}  // namespace

namespace fileapi {

FileSystemDirectoryDatabase::FileInfo::FileInfo() : parent_id(0) {
}

FileSystemDirectoryDatabase::FileInfo::~FileInfo() {
}

FileSystemDirectoryDatabase::FileSystemDirectoryDatabase(
    const FilePath& filesystem_data_directory)
    : filesystem_data_directory_(filesystem_data_directory) {
}

FileSystemDirectoryDatabase::~FileSystemDirectoryDatabase() {
}

bool FileSystemDirectoryDatabase::GetChildWithName(
    FileId parent_id, const FilePath::StringType& name, FileId* child_id) {
  if (!Init(FAIL_ON_CORRUPTION))
    return false;
  DCHECK(child_id);
  std::string child_key = GetChildLookupKey(parent_id, name);
  std::string child_id_string;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), child_key, &child_id_string);
  if (status.IsNotFound())
    return false;
  if (status.ok()) {
    if (!base::StringToInt64(child_id_string, child_id)) {
      LOG(ERROR) << "Hit database corruption!";
      return false;
    }
    return true;
  }
  HandleError(FROM_HERE, status);
  return false;
}

bool FileSystemDirectoryDatabase::GetFileWithPath(
    const FilePath& path, FileId* file_id) {
  std::vector<FilePath::StringType> components;
  VirtualPath::GetComponents(path, &components);
  FileId local_id = 0;
  std::vector<FilePath::StringType>::iterator iter;
  for (iter = components.begin(); iter != components.end(); ++iter) {
    FilePath::StringType name;
    name = *iter;
    if (name == FILE_PATH_LITERAL("/"))
      continue;
    if (!GetChildWithName(local_id, name, &local_id))
      return false;
  }
  *file_id = local_id;
  return true;
}

bool FileSystemDirectoryDatabase::ListChildren(
    FileId parent_id, std::vector<FileId>* children) {
  // Check to add later: fail if parent is a file, at least in debug builds.
  if (!Init(FAIL_ON_CORRUPTION))
    return false;
  DCHECK(children);
  std::string child_key_prefix = GetChildListingKeyPrefix(parent_id);

  scoped_ptr<leveldb::Iterator> iter(db_->NewIterator(leveldb::ReadOptions()));
  iter->Seek(child_key_prefix);
  children->clear();
  while (iter->Valid() &&
      StartsWithASCII(iter->key().ToString(), child_key_prefix, true)) {
    std::string child_id_string = iter->value().ToString();
    FileId child_id;
    if (!base::StringToInt64(child_id_string, &child_id)) {
      LOG(ERROR) << "Hit database corruption!";
      return false;
    }
    children->push_back(child_id);
    iter->Next();
  }
  return true;
}

bool FileSystemDirectoryDatabase::GetFileInfo(FileId file_id, FileInfo* info) {
  if (!Init(FAIL_ON_CORRUPTION))
    return false;
  DCHECK(info);
  std::string file_key = GetFileLookupKey(file_id);
  std::string file_data_string;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), file_key, &file_data_string);
  if (status.ok()) {
    return FileInfoFromPickle(
        Pickle(file_data_string.data(), file_data_string.length()), info);
  }
  // Special-case the root, for databases that haven't been initialized yet.
  // Without this, a query for the root's file info, made before creating the
  // first file in the database, will fail and confuse callers.
  if (status.IsNotFound() && !file_id) {
    info->name = FilePath::StringType();
    info->data_path = FilePath();
    info->modification_time = base::Time::Now();
    info->parent_id = 0;
    return true;
  }
  HandleError(FROM_HERE, status);
  return false;
}

bool FileSystemDirectoryDatabase::AddFileInfo(
    const FileInfo& info, FileId* file_id) {
  if (!Init(FAIL_ON_CORRUPTION))
    return false;
  DCHECK(file_id);
  std::string child_key = GetChildLookupKey(info.parent_id, info.name);
  std::string child_id_string;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), child_key, &child_id_string);
  if (status.ok()) {
    LOG(ERROR) << "File exists already!";
    return false;
  }
  if (!status.IsNotFound()) {
    HandleError(FROM_HERE, status);
    return false;
  }

  if (!VerifyIsDirectory(info.parent_id))
    return false;

  // This would be a fine place to limit the number of files in a directory, if
  // we decide to add that restriction.

  FileId temp_id;
  if (!GetLastFileId(&temp_id))
    return false;
  ++temp_id;

  leveldb::WriteBatch batch;
  if (!AddFileInfoHelper(info, temp_id, &batch))
    return false;

  batch.Put(LastFileIdKey(), base::Int64ToString(temp_id));
  status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  *file_id = temp_id;
  return true;
}

bool FileSystemDirectoryDatabase::RemoveFileInfo(FileId file_id) {
  if (!Init(FAIL_ON_CORRUPTION))
    return false;
  leveldb::WriteBatch batch;
  if (!RemoveFileInfoHelper(file_id, &batch))
    return false;
  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  return true;
}

bool FileSystemDirectoryDatabase::UpdateFileInfo(
    FileId file_id, const FileInfo& new_info) {
  // TODO: We should also check to see that this doesn't create a loop, but
  // perhaps only in a debug build.
  if (!Init(FAIL_ON_CORRUPTION))
    return false;
  DCHECK(file_id);  // You can't remove the root, ever.  Just delete the DB.
  FileInfo old_info;
  if (!GetFileInfo(file_id, &old_info))
    return false;
  if (old_info.parent_id != new_info.parent_id &&
      !VerifyIsDirectory(new_info.parent_id))
    return false;
  if (old_info.parent_id != new_info.parent_id ||
      old_info.name != new_info.name) {
    // Check for name clashes.
    FileId temp_id;
    if (GetChildWithName(new_info.parent_id, new_info.name, &temp_id)) {
      LOG(ERROR) << "Name collision on move.";
      return false;
    }
  }
  leveldb::WriteBatch batch;
  if (!RemoveFileInfoHelper(file_id, &batch) ||
      !AddFileInfoHelper(new_info, file_id, &batch))
    return false;
  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  return true;
}

bool FileSystemDirectoryDatabase::UpdateModificationTime(
    FileId file_id, const base::Time& modification_time) {
  FileInfo info;
  if (!GetFileInfo(file_id, &info))
    return false;
  info.modification_time = modification_time;
  Pickle pickle;
  if (!PickleFromFileInfo(info, &pickle))
    return false;
  leveldb::Status status = db_->Put(
      leveldb::WriteOptions(),
      GetFileLookupKey(file_id),
      leveldb::Slice(reinterpret_cast<const char *>(pickle.data()),
                     pickle.size()));
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  return true;
}

bool FileSystemDirectoryDatabase::OverwritingMoveFile(
    FileId src_file_id, FileId dest_file_id) {
  FileInfo src_file_info;
  FileInfo dest_file_info;

  if (!GetFileInfo(src_file_id, &src_file_info))
    return false;
  if (!GetFileInfo(dest_file_id, &dest_file_info))
    return false;
  if (src_file_info.is_directory() || dest_file_info.is_directory())
    return false;
  leveldb::WriteBatch batch;
  // This is the only field that really gets moved over; if you add fields to
  // FileInfo, e.g. ctime, they might need to be copied here.
  dest_file_info.data_path = src_file_info.data_path;
  if (!RemoveFileInfoHelper(src_file_id, &batch))
    return false;
  Pickle pickle;
  if (!PickleFromFileInfo(dest_file_info, &pickle))
    return false;
  batch.Put(
      GetFileLookupKey(dest_file_id),
      leveldb::Slice(reinterpret_cast<const char *>(pickle.data()),
                     pickle.size()));
  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  return true;
}

bool FileSystemDirectoryDatabase::GetNextInteger(int64* next) {
  if (!Init(FAIL_ON_CORRUPTION))
    return false;
  DCHECK(next);
  std::string int_string;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), LastIntegerKey(), &int_string);
  if (status.ok()) {
    int64 temp;
    if (!base::StringToInt64(int_string, &temp)) {
      LOG(ERROR) << "Hit database corruption!";
      return false;
    }
    ++temp;
    status = db_->Put(leveldb::WriteOptions(), LastIntegerKey(),
        base::Int64ToString(temp));
    if (!status.ok()) {
      HandleError(FROM_HERE, status);
      return false;
    }
    *next = temp;
    return true;
  }
  if (!status.IsNotFound()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  // The database must not yet exist; initialize it.
  if (!StoreDefaultValues())
    return false;

  return GetNextInteger(next);
}

// static
bool FileSystemDirectoryDatabase::DestroyDatabase(const FilePath& path) {
  std::string name  = FilePathToString(path.Append(kDirectoryDatabaseName));
  leveldb::Status status = leveldb::DestroyDB(name, leveldb::Options());
  if (status.ok())
    return true;
  LOG(WARNING) << "Failed to destroy a database with status " <<
      status.ToString();
  return false;
}

bool FileSystemDirectoryDatabase::Init(RecoveryOption recovery_option) {
  if (db_.get())
    return true;

  std::string path =
      FilePathToString(filesystem_data_directory_.Append(
          kDirectoryDatabaseName));
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

  if (recovery_option == FAIL_ON_CORRUPTION)
    return false;

  DCHECK_EQ(DELETE_ON_CORRUPTION, recovery_option);
  if (!file_util::Delete(filesystem_data_directory_, true))
    return false;
  if (!file_util::CreateDirectory(filesystem_data_directory_))
    return false;
  return Init(FAIL_ON_CORRUPTION);
}

void FileSystemDirectoryDatabase::ReportInitStatus(
    const leveldb::Status& status) {
  base::Time now = base::Time::Now();
  const base::TimeDelta minimum_interval =
      base::TimeDelta::FromHours(kMinimumReportIntervalHours);
  if (last_reported_time_ + minimum_interval >= now)
    return;
  last_reported_time_ = now;

  if (status.ok()) {
    UMA_HISTOGRAM_ENUMERATION(kInitStatusHistogramLabel,
                              INIT_STATUS_OK, INIT_STATUS_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kInitStatusHistogramLabel,
                              INIT_STATUS_CORRUPTION, INIT_STATUS_MAX);
  }
}

bool FileSystemDirectoryDatabase::StoreDefaultValues() {
  // Verify that this is a totally new database, and initialize it.
  scoped_ptr<leveldb::Iterator> iter(db_->NewIterator(leveldb::ReadOptions()));
  iter->SeekToFirst();
  if (iter->Valid()) {  // DB was not empty--we shouldn't have been called.
    LOG(ERROR) << "File system origin database is corrupt!";
    return false;
  }
  // This is always the first write into the database.  If we ever add a
  // version number, it should go in this transaction too.
  FileInfo root;
  root.parent_id = 0;
  root.modification_time = base::Time::Now();
  leveldb::WriteBatch batch;
  if (!AddFileInfoHelper(root, 0, &batch))
    return false;
  batch.Put(LastFileIdKey(), base::Int64ToString(0));
  batch.Put(LastIntegerKey(), base::Int64ToString(-1));
  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  return true;
}

bool FileSystemDirectoryDatabase::GetLastFileId(FileId* file_id) {
  if (!Init(FAIL_ON_CORRUPTION))
    return false;
  DCHECK(file_id);
  std::string id_string;
  leveldb::Status status =
      db_->Get(leveldb::ReadOptions(), LastFileIdKey(), &id_string);
  if (status.ok()) {
    if (!base::StringToInt64(id_string, file_id)) {
      LOG(ERROR) << "Hit database corruption!";
      return false;
    }
    return true;
  }
  if (!status.IsNotFound()) {
    HandleError(FROM_HERE, status);
    return false;
  }
  // The database must not yet exist; initialize it.
  if (!StoreDefaultValues())
    return false;
  *file_id = 0;
  return true;
}

bool FileSystemDirectoryDatabase::VerifyIsDirectory(FileId file_id) {
  FileInfo info;
  if (!file_id)
    return true;  // The root is a directory.
  if (!GetFileInfo(file_id, &info))
    return false;
  if (!info.is_directory()) {
    LOG(ERROR) << "New parent directory is a file!";
    return false;
  }
  return true;
}

// This does very few safety checks!
bool FileSystemDirectoryDatabase::AddFileInfoHelper(
    const FileInfo& info, FileId file_id, leveldb::WriteBatch* batch) {
  std::string id_string = GetFileLookupKey(file_id);
  if (!file_id) {
    // The root directory doesn't need to be looked up by path from its parent.
    DCHECK(!info.parent_id);
    DCHECK(info.data_path.empty());
  } else {
    std::string child_key = GetChildLookupKey(info.parent_id, info.name);
    batch->Put(child_key, id_string);
  }
  Pickle pickle;
  if (!PickleFromFileInfo(info, &pickle))
    return false;
  batch->Put(
      id_string,
      leveldb::Slice(reinterpret_cast<const char *>(pickle.data()),
                     pickle.size()));
  return true;
}

// This does very few safety checks!
bool FileSystemDirectoryDatabase::RemoveFileInfoHelper(
    FileId file_id, leveldb::WriteBatch* batch) {
  DCHECK(file_id);  // You can't remove the root, ever.  Just delete the DB.
  FileInfo info;
  if (!GetFileInfo(file_id, &info))
    return false;
  if (info.data_path.empty()) {  // It's a directory
    std::vector<FileId> children;
    // TODO(ericu): Make a faster is-the-directory-empty check.
    if (!ListChildren(file_id, &children))
      return false;
    if (children.size()) {
      LOG(ERROR) << "Can't remove a directory with children.";
      return false;
    }
  }
  batch->Delete(GetChildLookupKey(info.parent_id, info.name));
  batch->Delete(GetFileLookupKey(file_id));
  return true;
}

void FileSystemDirectoryDatabase::HandleError(
    const tracked_objects::Location& from_here,
    const leveldb::Status& status) {
  LOG(ERROR) << "FileSystemDirectoryDatabase failed at: "
             << from_here.ToString() << " with error: " << status.ToString();
  db_.reset();
}

}  // namespace fileapi
