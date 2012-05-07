// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_directory_database.h"

#include <algorithm>
#include <math.h>

#include "base/file_util.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/pickle.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "webkit/fileapi/file_system_usage_cache.h"
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
  INIT_STATUS_IO_ERROR,
  INIT_STATUS_UNKNOWN_ERROR,
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

// Assumptions:
//  - Any database entry is one of:
//    - ("CHILD_OF:|parent_id|:<name>", "|file_id|"),
//    - ("LAST_FILE_ID", "|last_file_id|"),
//    - ("LAST_INTEGER", "|last_integer|"),
//    - ("|file_id|", "pickled FileInfo")
//        where FileInfo has |parent_id|, |data_path|, |name| and
//        |modification_time|,
// Constraints:
//  - Each file in the database has unique backing file.
//  - Each file in |filesystem_data_directory_| has a database entry.
//  - Directory structure is tree, i.e. connected and acyclic.
class DatabaseCheckHelper {
 public:
  typedef fileapi::FileSystemDirectoryDatabase::FileId FileId;
  typedef fileapi::FileSystemDirectoryDatabase::FileInfo FileInfo;

  DatabaseCheckHelper(fileapi::FileSystemDirectoryDatabase* dir_db,
                      leveldb::DB* db,
                      const FilePath& path);

  bool IsFileSystemConsistent() {
    return IsDatabaseEmpty() ||
        (ScanDatabase() && ScanDirectory() && ScanHierarchy());
  }

 private:
  bool IsDatabaseEmpty();
  // These 3 methods need to be called in the order.  Each method requires its
  // previous method finished successfully. They also require the database is
  // not empty.
  bool ScanDatabase();
  bool ScanDirectory();
  bool ScanHierarchy();

  fileapi::FileSystemDirectoryDatabase* dir_db_;
  leveldb::DB* db_;
  FilePath path_;

  std::set<FilePath> files_in_db_;

  size_t num_directories_in_db_;
  size_t num_files_in_db_;
  size_t num_hierarchy_links_in_db_;

  FileId last_file_id_;
  FileId last_integer_;
};

DatabaseCheckHelper::DatabaseCheckHelper(
    fileapi::FileSystemDirectoryDatabase* dir_db,
    leveldb::DB* db,
    const FilePath& path)
    : dir_db_(dir_db), db_(db), path_(path),
      num_directories_in_db_(0),
      num_files_in_db_(0),
      num_hierarchy_links_in_db_(0),
      last_file_id_(-1), last_integer_(-1) {
  DCHECK(dir_db_);
  DCHECK(db_);
  DCHECK(!path_.empty() && file_util::DirectoryExists(path_));
}

bool DatabaseCheckHelper::IsDatabaseEmpty() {
  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  itr->SeekToFirst();
  return !itr->Valid();
}

bool DatabaseCheckHelper::ScanDatabase() {
  // Scans all database entries sequentially to verify each of them has unique
  // backing file.
  int64 max_file_id = -1;
  std::set<FileId> file_ids;

  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  for (itr->SeekToFirst(); itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    if (StartsWithASCII(key, kChildLookupPrefix, true)) {
      // key: "CHILD_OF:<parent_id>:<name>"
      // value: "<child_id>"
      ++num_hierarchy_links_in_db_;
    } else if (key == kLastFileIdKey) {
      // key: "LAST_FILE_ID"
      // value: "<last_file_id>"
      if (last_file_id_ >= 0 ||
          !base::StringToInt64(itr->value().ToString(), &last_file_id_))
        return false;

      if (last_file_id_ < 0)
        return false;
    } else if (key == kLastIntegerKey) {
      // key: "LAST_INTEGER"
      // value: "<last_integer>"
      if (last_integer_ >= 0 ||
          !base::StringToInt64(itr->value().ToString(), &last_integer_))
        return false;
    } else {
      // key: "<entry_id>"
      // value: "<pickled FileInfo>"
      FileInfo file_info;
      if (!FileInfoFromPickle(
              Pickle(itr->value().data(), itr->value().size()), &file_info))
        return false;

      FileId file_id = -1;
      if (!base::StringToInt64(key, &file_id) || file_id < 0)
        return false;

      if (max_file_id < file_id)
        max_file_id = file_id;
      if (!file_ids.insert(file_id).second)
        return false;

      if (file_info.is_directory()) {
        ++num_directories_in_db_;
        DCHECK(file_info.data_path.empty());
      } else {
        // Ensure any pair of file entry don't share their data_path.
        if (!files_in_db_.insert(file_info.data_path).second)
          return false;

        // Ensure the backing file exists as a normal file.
        base::PlatformFileInfo platform_file_info;
        if (!file_util::GetFileInfo(
                path_.Append(file_info.data_path), &platform_file_info) ||
            platform_file_info.is_directory ||
            platform_file_info.is_symbolic_link) {
          // leveldb::Iterator iterates a snapshot of the database.
          // So even after RemoveFileInfo() call, we'll visit hierarchy link
          // from |parent_id| to |file_id|.
          if (!dir_db_->RemoveFileInfo(file_id))
            return false;
          --num_hierarchy_links_in_db_;
          files_in_db_.erase(file_info.data_path);
        } else {
          ++num_files_in_db_;
        }
      }
    }
  }

  // TODO(tzik): Add constraint for |last_integer_| to avoid possible
  // data path confliction on ObfuscatedFileUtil.
  return max_file_id <= last_file_id_;
}

bool DatabaseCheckHelper::ScanDirectory() {
  // Scans all local file system entries to verify each of them has a database
  // entry.
  const FilePath kExcludes[] = {
    FilePath(kDirectoryDatabaseName),
    FilePath(fileapi::FileSystemUsageCache::kUsageFileName),
  };

  // Any path in |pending_directories| is relative to |path_|.
  std::stack<FilePath> pending_directories;
  pending_directories.push(FilePath());

  while (!pending_directories.empty()) {
    FilePath dir_path = pending_directories.top();
    pending_directories.pop();

    file_util::FileEnumerator file_enum(
        dir_path.empty() ? path_ : path_.Append(dir_path),
        false /* recursive */,
        static_cast<file_util::FileEnumerator::FileType>(
            file_util::FileEnumerator::DIRECTORIES |
            file_util::FileEnumerator::FILES));

    FilePath absolute_file_path;
    while (!(absolute_file_path = file_enum.Next()).empty()) {
      file_util::FileEnumerator::FindInfo find_info;
      file_enum.GetFindInfo(&find_info);

      FilePath relative_file_path;
      if (!path_.AppendRelativePath(absolute_file_path, &relative_file_path))
        return false;

      if (std::find(kExcludes, kExcludes + arraysize(kExcludes),
                    relative_file_path) != kExcludes + arraysize(kExcludes))
        continue;

      if (file_util::FileEnumerator::IsDirectory(find_info)) {
        pending_directories.push(relative_file_path);
        continue;
      }

      // Check if the file has a database entry.
      std::set<FilePath>::iterator itr = files_in_db_.find(relative_file_path);
      if (itr == files_in_db_.end()) {
        if (!file_util::Delete(absolute_file_path, false))
          return false;
      } else {
        files_in_db_.erase(itr);
      }
    }
  }

  return files_in_db_.empty();
}

bool DatabaseCheckHelper::ScanHierarchy() {
  size_t visited_directories = 0;
  size_t visited_files = 0;
  size_t visited_links = 0;

  std::stack<FileId> directories;
  directories.push(0);

  // Check if the root directory exists as a directory.
  FileInfo file_info;
  if (!dir_db_->GetFileInfo(0, &file_info))
    return false;
  if (file_info.parent_id != 0 ||
      !file_info.is_directory())
    return false;

  while (!directories.empty()) {
    ++visited_directories;
    FileId dir_id = directories.top();
    directories.pop();

    std::vector<FileId> children;
    if (!dir_db_->ListChildren(dir_id, &children))
      return false;
    for (std::vector<FileId>::iterator itr = children.begin();
         itr != children.end();
         ++itr) {
      // Any directory must not have root directory as child.
      if (!*itr)
        return false;

      // Check if the child knows the parent as its parent.
      FileInfo file_info;
      if (!dir_db_->GetFileInfo(*itr, &file_info))
        return false;
      if (file_info.parent_id != dir_id)
        return false;

      // Check if the parent knows the name of its child correctly.
      FileId file_id;
      if (!dir_db_->GetChildWithName(dir_id, file_info.name, &file_id) ||
          file_id != *itr)
        return false;

      if (file_info.is_directory())
        directories.push(*itr);
      else
        ++visited_files;
      ++visited_links;
    }
  }

  // Check if we've visited all database entries.
  return num_directories_in_db_ == visited_directories &&
      num_files_in_db_ == visited_files &&
      num_hierarchy_links_in_db_ == visited_links;
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
  if (!Init(REPAIR_ON_CORRUPTION))
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
  if (!Init(REPAIR_ON_CORRUPTION))
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
  if (!Init(REPAIR_ON_CORRUPTION))
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
  if (!Init(REPAIR_ON_CORRUPTION))
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
  if (!Init(REPAIR_ON_CORRUPTION))
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
  if (!Init(REPAIR_ON_CORRUPTION))
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
  if (!Init(REPAIR_ON_CORRUPTION))
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

  if (!status.IsCorruption())
    return false;

  switch (recovery_option) {
    case FAIL_ON_CORRUPTION:
      return false;
    case REPAIR_ON_CORRUPTION:
      LOG(WARNING) << "Corrupted FileSystemDirectoryDatabase detected."
                   << " Attempting to repair.";
      if (RepairDatabase(path))
        return true;
      LOG(WARNING) << "Failed to repair FileSystemDirectoryDatabase.";
      // fall through
    case DELETE_ON_CORRUPTION:
      LOG(WARNING) << "Clearing FileSystemDirectoryDatabase.";
      if (!file_util::Delete(filesystem_data_directory_, true))
        return false;
      if (!file_util::CreateDirectory(filesystem_data_directory_))
        return false;
      return Init(FAIL_ON_CORRUPTION);
  }

  NOTREACHED();
  return false;
}

bool FileSystemDirectoryDatabase::RepairDatabase(const std::string& db_path) {
  DCHECK(!db_.get());
  if (!leveldb::RepairDB(db_path, leveldb::Options()).ok())
    return false;
  if (!Init(FAIL_ON_CORRUPTION))
    return false;
  if (IsFileSystemConsistent())
    return true;
  db_.reset();
  return false;
}

bool FileSystemDirectoryDatabase::IsFileSystemConsistent() {
  if (!Init(FAIL_ON_CORRUPTION))
    return false;
  DatabaseCheckHelper helper(this, db_.get(), filesystem_data_directory_);
  return helper.IsFileSystemConsistent();
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
  if (!Init(REPAIR_ON_CORRUPTION))
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
