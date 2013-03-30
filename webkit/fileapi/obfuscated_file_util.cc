// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/obfuscated_file_util.h"

#include <queue>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_observers.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/native_file_util.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/quota/quota_manager.h"

// Example of various paths:
//   void ObfuscatedFileUtil::DoSomething(const FileSystemURL& url) {
//     base::FilePath virtual_path = url.path();
//     base::FilePath local_path = GetLocalFilePath(url);
//
//     NativeFileUtil::DoSomething(local_path);
//     file_util::DoAnother(local_path);
//  }

namespace fileapi {

namespace {

typedef FileSystemDirectoryDatabase::FileId FileId;
typedef FileSystemDirectoryDatabase::FileInfo FileInfo;

const int64 kFlushDelaySeconds = 10 * 60;  // 10 minutes

void InitFileInfo(
    FileSystemDirectoryDatabase::FileInfo* file_info,
    FileSystemDirectoryDatabase::FileId parent_id,
    const base::FilePath::StringType& file_name) {
  DCHECK(file_info);
  file_info->parent_id = parent_id;
  file_info->name = file_name;
}

// Costs computed as per crbug.com/86114, based on the LevelDB implementation of
// path storage under Linux.  It's not clear if that will differ on Windows, on
// which base::FilePath uses wide chars [since they're converted to UTF-8 for storage
// anyway], but as long as the cost is high enough that one can't cheat on quota
// by storing data in paths, it doesn't need to be all that accurate.
const int64 kPathCreationQuotaCost = 146;  // Bytes per inode, basically.
const int64 kPathByteQuotaCost = 2;  // Bytes per byte of path length in UTF-8.

int64 UsageForPath(size_t length) {
  return kPathCreationQuotaCost +
      static_cast<int64>(length) * kPathByteQuotaCost;
}

bool AllocateQuota(FileSystemOperationContext* context, int64 growth) {
  if (context->allowed_bytes_growth() == quota::QuotaManager::kNoLimit)
    return true;

  int64 new_quota = context->allowed_bytes_growth() - growth;
  if (growth > 0 && new_quota < 0)
    return false;
  context->set_allowed_bytes_growth(new_quota);
  return true;
}

void UpdateUsage(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    int64 growth) {
  context->update_observers()->Notify(
      &FileUpdateObserver::OnUpdate, MakeTuple(url, growth));
}

void TouchDirectory(FileSystemDirectoryDatabase* db, FileId dir_id) {
  DCHECK(db);
  if (!db->UpdateModificationTime(dir_id, base::Time::Now()))
    NOTREACHED();
}

const base::FilePath::CharType kTemporaryDirectoryName[] = FILE_PATH_LITERAL("t");
const base::FilePath::CharType kPersistentDirectoryName[] = FILE_PATH_LITERAL("p");
const base::FilePath::CharType kSyncableDirectoryName[] = FILE_PATH_LITERAL("s");

}  // namespace

using base::PlatformFile;
using base::PlatformFileError;

class ObfuscatedFileEnumerator
    : public FileSystemFileUtil::AbstractFileEnumerator {
 public:
  ObfuscatedFileEnumerator(
      FileSystemDirectoryDatabase* db,
      FileSystemOperationContext* context,
      ObfuscatedFileUtil* obfuscated_file_util,
      const FileSystemURL& root_url,
      bool recursive)
      : db_(db),
        context_(context),
        obfuscated_file_util_(obfuscated_file_util),
        origin_(root_url.origin()),
        type_(root_url.type()),
        recursive_(recursive),
        current_file_id_(0) {
    base::FilePath root_virtual_path = root_url.path();
    FileId file_id;

    if (!db_->GetFileWithPath(root_virtual_path, &file_id))
      return;

    FileRecord record = { file_id, root_virtual_path };
    recurse_queue_.push(record);
  }

  virtual ~ObfuscatedFileEnumerator() {}

  virtual base::FilePath Next() OVERRIDE {
    ProcessRecurseQueue();
    if (display_stack_.empty())
      return base::FilePath();

    current_file_id_ = display_stack_.back();
    display_stack_.pop_back();

    FileInfo file_info;
    base::FilePath platform_file_path;
    base::PlatformFileError error =
        obfuscated_file_util_->GetFileInfoInternal(
            db_, context_, origin_, type_, current_file_id_,
            &file_info, &current_platform_file_info_, &platform_file_path);
    if (error != base::PLATFORM_FILE_OK)
      return Next();

    base::FilePath virtual_path =
        current_parent_virtual_path_.Append(file_info.name);
    if (recursive_ && file_info.is_directory()) {
      FileRecord record = { current_file_id_, virtual_path };
      recurse_queue_.push(record);
    }
    return virtual_path;
  }

  virtual int64 Size() OVERRIDE {
    return current_platform_file_info_.size;
  }

  virtual base::Time LastModifiedTime() OVERRIDE {
    return current_platform_file_info_.last_modified;
  }

  virtual bool IsDirectory() OVERRIDE {
    return current_platform_file_info_.is_directory;
  }

 private:
  typedef FileSystemDirectoryDatabase::FileId FileId;
  typedef FileSystemDirectoryDatabase::FileInfo FileInfo;

  struct FileRecord {
    FileId file_id;
    base::FilePath virtual_path;
  };

  void ProcessRecurseQueue() {
    while (display_stack_.empty() && !recurse_queue_.empty()) {
      FileRecord entry = recurse_queue_.front();
      recurse_queue_.pop();
      if (!db_->ListChildren(entry.file_id, &display_stack_)) {
        display_stack_.clear();
        return;
      }
      current_parent_virtual_path_ = entry.virtual_path;
    }
  }

  FileSystemDirectoryDatabase* db_;
  FileSystemOperationContext* context_;
  ObfuscatedFileUtil* obfuscated_file_util_;
  GURL origin_;
  FileSystemType type_;
  bool recursive_;

  std::queue<FileRecord> recurse_queue_;
  std::vector<FileId> display_stack_;
  base::FilePath current_parent_virtual_path_;

  FileId current_file_id_;
  base::PlatformFileInfo current_platform_file_info_;
};

class ObfuscatedOriginEnumerator
    : public ObfuscatedFileUtil::AbstractOriginEnumerator {
 public:
  typedef FileSystemOriginDatabase::OriginRecord OriginRecord;
  ObfuscatedOriginEnumerator(
      FileSystemOriginDatabase* origin_database,
      const base::FilePath& base_file_path)
      : base_file_path_(base_file_path) {
    if (origin_database)
      origin_database->ListAllOrigins(&origins_);
  }

  virtual ~ObfuscatedOriginEnumerator() {}

  // Returns the next origin.  Returns empty if there are no more origins.
  virtual GURL Next() OVERRIDE {
    OriginRecord record;
    if (!origins_.empty()) {
      record = origins_.back();
      origins_.pop_back();
    }
    current_ = record;
    return GetOriginURLFromIdentifier(record.origin);
  }

  // Returns the current origin's information.
  virtual bool HasFileSystemType(FileSystemType type) const OVERRIDE {
    if (current_.path.empty())
      return false;
    base::FilePath::StringType type_string =
        ObfuscatedFileUtil::GetDirectoryNameForType(type);
    if (type_string.empty()) {
      NOTREACHED();
      return false;
    }
    base::FilePath path = base_file_path_.Append(current_.path).Append(type_string);
    return file_util::DirectoryExists(path);
  }

 private:
  std::vector<OriginRecord> origins_;
  OriginRecord current_;
  base::FilePath base_file_path_;
};

ObfuscatedFileUtil::ObfuscatedFileUtil(
    const base::FilePath& file_system_directory)
    : file_system_directory_(file_system_directory) {
}

ObfuscatedFileUtil::~ObfuscatedFileUtil() {
  DropDatabases();
}

PlatformFileError ObfuscatedFileUtil::CreateOrOpen(
    FileSystemOperationContext* context,
    const FileSystemURL& url, int file_flags,
    PlatformFile* file_handle, bool* created) {
  DCHECK(!(file_flags & (base::PLATFORM_FILE_DELETE_ON_CLOSE |
        base::PLATFORM_FILE_HIDDEN | base::PLATFORM_FILE_EXCLUSIVE_READ |
        base::PLATFORM_FILE_EXCLUSIVE_WRITE)));
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      url.origin(), url.type(), true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;
  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id)) {
    // The file doesn't exist.
    if (!(file_flags & (base::PLATFORM_FILE_CREATE |
        base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_OPEN_ALWAYS)))
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    FileId parent_id;
    if (!db->GetFileWithPath(VirtualPath::DirName(url.path()),
                             &parent_id))
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    FileInfo file_info;
    InitFileInfo(&file_info, parent_id,
                 VirtualPath::BaseName(url.path()).value());

    int64 growth = UsageForPath(file_info.name.size());
    if (!AllocateQuota(context, growth))
      return base::PLATFORM_FILE_ERROR_NO_SPACE;
    PlatformFileError error = CreateFile(
        context, base::FilePath(),
        url.origin(), url.type(), &file_info,
        file_flags, file_handle);
    if (created && base::PLATFORM_FILE_OK == error) {
      *created = true;
      UpdateUsage(context, url, growth);
      context->change_observers()->Notify(
          &FileChangeObserver::OnCreateFile, MakeTuple(url));
    }
    return error;
  }

  if (file_flags & base::PLATFORM_FILE_CREATE)
    return base::PLATFORM_FILE_ERROR_EXISTS;

  base::PlatformFileInfo platform_file_info;
  base::FilePath local_path;
  FileInfo file_info;
  base::PlatformFileError error = GetFileInfoInternal(
      db, context, url.origin(), url.type(), file_id,
      &file_info, &platform_file_info, &local_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;
  if (file_info.is_directory())
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;

  int64 delta = 0;
  if (file_flags & (base::PLATFORM_FILE_CREATE_ALWAYS |
                    base::PLATFORM_FILE_OPEN_TRUNCATED)) {
    // The file exists and we're truncating.
    delta = -platform_file_info.size;
    AllocateQuota(context, delta);
  }

  error = NativeFileUtil::CreateOrOpen(
      local_path, file_flags, file_handle, created);
  if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND) {
    // TODO(tzik): Also invalidate on-memory usage cache in UsageTracker.
    // TODO(tzik): Delete database entry after ensuring the file lost.
    InvalidateUsageCache(context, url.origin(), url.type());
    LOG(WARNING) << "Lost a backing file.";
    error = base::PLATFORM_FILE_ERROR_FAILED;
  }

  // If truncating we need to update the usage.
  if (error == base::PLATFORM_FILE_OK && delta) {
    UpdateUsage(context, url, delta);
    context->change_observers()->Notify(
        &FileChangeObserver::OnModifyFile, MakeTuple(url));
  }
  return error;
}

PlatformFileError ObfuscatedFileUtil::Close(
    FileSystemOperationContext* context,
    base::PlatformFile file) {
  return NativeFileUtil::Close(file);
}

PlatformFileError ObfuscatedFileUtil::EnsureFileExists(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    bool* created) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      url.origin(), url.type(), true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;

  FileId file_id;
  if (db->GetFileWithPath(url.path(), &file_id)) {
    FileInfo file_info;
    if (!db->GetFileInfo(file_id, &file_info)) {
      NOTREACHED();
      return base::PLATFORM_FILE_ERROR_FAILED;
    }
    if (file_info.is_directory())
      return base::PLATFORM_FILE_ERROR_NOT_A_FILE;
    if (created)
      *created = false;
    return base::PLATFORM_FILE_OK;
  }
  FileId parent_id;
  if (!db->GetFileWithPath(VirtualPath::DirName(url.path()), &parent_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  FileInfo file_info;
  InitFileInfo(&file_info, parent_id,
               VirtualPath::BaseName(url.path()).value());

  int64 growth = UsageForPath(file_info.name.size());
  if (!AllocateQuota(context, growth))
    return base::PLATFORM_FILE_ERROR_NO_SPACE;
  PlatformFileError error = CreateFile(
      context, base::FilePath(), url.origin(), url.type(), &file_info, 0, NULL);
  if (created && base::PLATFORM_FILE_OK == error) {
    *created = true;
    UpdateUsage(context, url, growth);
    context->change_observers()->Notify(
        &FileChangeObserver::OnCreateFile, MakeTuple(url));
  }
  return error;
}

PlatformFileError ObfuscatedFileUtil::CreateDirectory(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    bool exclusive,
    bool recursive) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      url.origin(), url.type(), true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;

  // TODO(kinuko): Remove this dirty hack when we fully support directory
  // operations or clean up the code if we decided not to support directory
  // operations. (http://crbug.com/161442)
  if (url.type() == kFileSystemTypeSyncable &&
      !context->file_system_context()->sandbox_provider()->
          is_sync_directory_operation_enabled()) {
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  }

  FileId file_id;
  if (db->GetFileWithPath(url.path(), &file_id)) {
    FileInfo file_info;
    if (exclusive)
      return base::PLATFORM_FILE_ERROR_EXISTS;
    if (!db->GetFileInfo(file_id, &file_info)) {
      NOTREACHED();
      return base::PLATFORM_FILE_ERROR_FAILED;
    }
    if (!file_info.is_directory())
      return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
    return base::PLATFORM_FILE_OK;
  }

  std::vector<base::FilePath::StringType> components;
  VirtualPath::GetComponents(url.path(), &components);
  FileId parent_id = 0;
  size_t index;
  for (index = 0; index < components.size(); ++index) {
    base::FilePath::StringType name = components[index];
    if (name == FILE_PATH_LITERAL("/"))
      continue;
    if (!db->GetChildWithName(parent_id, name, &parent_id))
      break;
  }
  if (!recursive && components.size() - index > 1)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  bool first = true;
  for (; index < components.size(); ++index) {
    FileInfo file_info;
    file_info.name = components[index];
    if (file_info.name == FILE_PATH_LITERAL("/"))
      continue;
    file_info.modification_time = base::Time::Now();
    file_info.parent_id = parent_id;
    int64 growth = UsageForPath(file_info.name.size());
    if (!AllocateQuota(context, growth))
      return base::PLATFORM_FILE_ERROR_NO_SPACE;
    if (!db->AddFileInfo(file_info, &parent_id)) {
      NOTREACHED();
      return base::PLATFORM_FILE_ERROR_FAILED;
    }
    UpdateUsage(context, url, growth);
    context->change_observers()->Notify(
        &FileChangeObserver::OnCreateDirectory, MakeTuple(url));
    if (first) {
      first = false;
      TouchDirectory(db, file_info.parent_id);
    }
  }
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileUtil::GetFileInfo(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    base::PlatformFileInfo* file_info,
    base::FilePath* platform_file_path) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      url.origin(), url.type(), false);
  if (!db)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileInfo local_info;
  return GetFileInfoInternal(db, context,
                             url.origin(), url.type(),
                             file_id, &local_info,
                             file_info, platform_file_path);
}

scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator>
    ObfuscatedFileUtil::CreateFileEnumerator(
    FileSystemOperationContext* context,
    const FileSystemURL& root_url,
    bool recursive) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      root_url.origin(), root_url.type(), false);
  if (!db) {
    return make_scoped_ptr(new FileSystemFileUtil::EmptyFileEnumerator())
        .PassAs<FileSystemFileUtil::AbstractFileEnumerator>();
  }
  return make_scoped_ptr(new ObfuscatedFileEnumerator(db, context, this,
                                                      root_url, recursive))
      .PassAs<FileSystemFileUtil::AbstractFileEnumerator>();
}

PlatformFileError ObfuscatedFileUtil::GetLocalFilePath(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    base::FilePath* local_path) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      url.origin(), url.type(), false);
  if (!db)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info) || file_info.is_directory()) {
    NOTREACHED();
    // Directories have no local file path.
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  }
  *local_path = DataPathToLocalPath(
      url.origin(), url.type(), file_info.data_path);

  if (local_path->empty())
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileUtil::Touch(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      url.origin(), url.type(), false);
  if (!db)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info)) {
    NOTREACHED();
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  if (file_info.is_directory()) {
    if (!db->UpdateModificationTime(file_id, last_modified_time))
      return base::PLATFORM_FILE_ERROR_FAILED;
    return base::PLATFORM_FILE_OK;
  }
  base::FilePath local_path = DataPathToLocalPath(
      url.origin(), url.type(), file_info.data_path);
  return NativeFileUtil::Touch(
      local_path, last_access_time, last_modified_time);
}

PlatformFileError ObfuscatedFileUtil::Truncate(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    int64 length) {
  base::PlatformFileInfo file_info;
  base::FilePath local_path;
  base::PlatformFileError error =
      GetFileInfo(context, url, &file_info, &local_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  int64 growth = length - file_info.size;
  if (!AllocateQuota(context, growth))
    return base::PLATFORM_FILE_ERROR_NO_SPACE;
  error = NativeFileUtil::Truncate(local_path, length);
  if (error == base::PLATFORM_FILE_OK) {
    UpdateUsage(context, url, growth);
    context->change_observers()->Notify(
        &FileChangeObserver::OnModifyFile, MakeTuple(url));
  }
  return error;
}

PlatformFileError ObfuscatedFileUtil::CopyOrMoveFile(
    FileSystemOperationContext* context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    bool copy) {
  // Cross-filesystem copies and moves should be handled via CopyInForeignFile.
  DCHECK(src_url.origin() == dest_url.origin());
  DCHECK(src_url.type() == dest_url.type());

  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      src_url.origin(), src_url.type(), true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;

  FileId src_file_id;
  if (!db->GetFileWithPath(src_url.path(), &src_file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  FileId dest_file_id;
  bool overwrite = db->GetFileWithPath(dest_url.path(),
                                       &dest_file_id);

  FileInfo src_file_info;
  base::PlatformFileInfo src_platform_file_info;
  base::FilePath src_local_path;
  base::PlatformFileError error = GetFileInfoInternal(
      db, context, src_url.origin(), src_url.type(), src_file_id,
      &src_file_info, &src_platform_file_info, &src_local_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;
  if (src_file_info.is_directory())
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;

  FileInfo dest_file_info;
  base::PlatformFileInfo dest_platform_file_info;  // overwrite case only
  base::FilePath dest_local_path;  // overwrite case only
  if (overwrite) {
    base::PlatformFileError error = GetFileInfoInternal(
        db, context, dest_url.origin(), dest_url.type(), dest_file_id,
        &dest_file_info, &dest_platform_file_info, &dest_local_path);
    if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
      overwrite = false;  // fallback to non-overwrite case
    else if (error != base::PLATFORM_FILE_OK)
      return error;
    else if (dest_file_info.is_directory())
      return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  }
  if (!overwrite) {
    FileId dest_parent_id;
    if (!db->GetFileWithPath(VirtualPath::DirName(dest_url.path()),
                             &dest_parent_id)) {
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    }

    dest_file_info = src_file_info;
    dest_file_info.parent_id = dest_parent_id;
    dest_file_info.name =
        VirtualPath::BaseName(dest_url.path()).value();
  }

  int64 growth = 0;
  if (copy)
    growth += src_platform_file_info.size;
  else
    growth -= UsageForPath(src_file_info.name.size());
  if (overwrite)
    growth -= dest_platform_file_info.size;
  else
    growth += UsageForPath(dest_file_info.name.size());
  if (!AllocateQuota(context, growth))
    return base::PLATFORM_FILE_ERROR_NO_SPACE;

  /*
   * Copy-with-overwrite
   *  Just overwrite data file
   * Copy-without-overwrite
   *  Copy backing file
   *  Create new metadata pointing to new backing file.
   * Move-with-overwrite
   *  transaction:
   *    Remove source entry.
   *    Point target entry to source entry's backing file.
   *  Delete target entry's old backing file
   * Move-without-overwrite
   *  Just update metadata
   */
  error = base::PLATFORM_FILE_ERROR_FAILED;
  if (copy) {
    if (overwrite) {
      error = NativeFileUtil::CopyOrMoveFile(
          src_local_path,
          dest_local_path,
          true /* copy */);
    } else {  // non-overwrite
      error = CreateFile(context, src_local_path,
                         dest_url.origin(), dest_url.type(),
                         &dest_file_info, 0, NULL);
    }
  } else {
    if (overwrite) {
      if (db->OverwritingMoveFile(src_file_id, dest_file_id)) {
        if (base::PLATFORM_FILE_OK !=
            NativeFileUtil::DeleteFile(dest_local_path))
          LOG(WARNING) << "Leaked a backing file.";
        error = base::PLATFORM_FILE_OK;
      } else {
        error = base::PLATFORM_FILE_ERROR_FAILED;
      }
    } else {  // non-overwrite
      if (db->UpdateFileInfo(src_file_id, dest_file_info))
        error = base::PLATFORM_FILE_OK;
      else
        error = base::PLATFORM_FILE_ERROR_FAILED;
    }
  }

  if (error != base::PLATFORM_FILE_OK)
    return error;

  if (overwrite) {
    context->change_observers()->Notify(
        &FileChangeObserver::OnModifyFile,
        MakeTuple(dest_url));
  } else {
    context->change_observers()->Notify(
        &FileChangeObserver::OnCreateFileFrom,
        MakeTuple(dest_url, src_url));
  }

  if (!copy) {
    context->change_observers()->Notify(
        &FileChangeObserver::OnRemoveFile, MakeTuple(src_url));
    TouchDirectory(db, src_file_info.parent_id);
  }

  TouchDirectory(db, dest_file_info.parent_id);

  UpdateUsage(context, dest_url, growth);
  return error;
}

PlatformFileError ObfuscatedFileUtil::CopyInForeignFile(
    FileSystemOperationContext* context,
    const base::FilePath& src_file_path,
    const FileSystemURL& dest_url) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      dest_url.origin(), dest_url.type(), true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;

  base::PlatformFileInfo src_platform_file_info;
  if (!file_util::GetFileInfo(src_file_path, &src_platform_file_info))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  FileId dest_file_id;
  bool overwrite = db->GetFileWithPath(dest_url.path(),
                                       &dest_file_id);

  FileInfo dest_file_info;
  base::PlatformFileInfo dest_platform_file_info;  // overwrite case only
  if (overwrite) {
    base::FilePath dest_local_path;
    base::PlatformFileError error = GetFileInfoInternal(
        db, context, dest_url.origin(), dest_url.type(), dest_file_id,
        &dest_file_info, &dest_platform_file_info, &dest_local_path);
    if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
      overwrite = false;  // fallback to non-overwrite case
    else if (error != base::PLATFORM_FILE_OK)
      return error;
    else if (dest_file_info.is_directory())
      return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  }
  if (!overwrite) {
    FileId dest_parent_id;
    if (!db->GetFileWithPath(VirtualPath::DirName(dest_url.path()),
                             &dest_parent_id)) {
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    }
    if (!dest_file_info.is_directory())
      return base::PLATFORM_FILE_ERROR_FAILED;
    InitFileInfo(&dest_file_info, dest_parent_id,
                 VirtualPath::BaseName(dest_url.path()).value());
  }

  int64 growth = src_platform_file_info.size;
  if (overwrite)
    growth -= dest_platform_file_info.size;
  else
    growth += UsageForPath(dest_file_info.name.size());
  if (!AllocateQuota(context, growth))
    return base::PLATFORM_FILE_ERROR_NO_SPACE;

  base::PlatformFileError error;
  if (overwrite) {
    base::FilePath dest_local_path = DataPathToLocalPath(
        dest_url.origin(), dest_url.type(), dest_file_info.data_path);
    error = NativeFileUtil::CopyOrMoveFile(
        src_file_path, dest_local_path, true);
  } else {
    error = CreateFile(context, src_file_path,
                       dest_url.origin(), dest_url.type(),
                       &dest_file_info, 0, NULL);
  }

  if (error != base::PLATFORM_FILE_OK)
    return error;

  if (overwrite) {
    context->change_observers()->Notify(
        &FileChangeObserver::OnModifyFile, MakeTuple(dest_url));
  } else {
    context->change_observers()->Notify(
        &FileChangeObserver::OnCreateFile, MakeTuple(dest_url));
  }

  UpdateUsage(context, dest_url, growth);
  TouchDirectory(db, dest_file_info.parent_id);
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileUtil::DeleteFile(
    FileSystemOperationContext* context,
    const FileSystemURL& url) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      url.origin(), url.type(), true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;
  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  FileInfo file_info;
  base::PlatformFileInfo platform_file_info;
  base::FilePath local_path;
  base::PlatformFileError error = GetFileInfoInternal(
      db, context, url.origin(), url.type(), file_id,
      &file_info, &platform_file_info, &local_path);
  if (error != base::PLATFORM_FILE_ERROR_NOT_FOUND &&
      error != base::PLATFORM_FILE_OK)
    return error;

  if (file_info.is_directory())
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;

  int64 growth = -UsageForPath(file_info.name.size()) - platform_file_info.size;
  AllocateQuota(context, growth);
  if (!db->RemoveFileInfo(file_id)) {
    NOTREACHED();
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  UpdateUsage(context, url, growth);
  TouchDirectory(db, file_info.parent_id);

  context->change_observers()->Notify(
      &FileChangeObserver::OnRemoveFile, MakeTuple(url));

  if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
    return base::PLATFORM_FILE_OK;

  error = NativeFileUtil::DeleteFile(local_path);
  if (base::PLATFORM_FILE_OK != error)
    LOG(WARNING) << "Leaked a backing file.";
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileUtil::DeleteDirectory(
    FileSystemOperationContext* context,
    const FileSystemURL& url) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      url.origin(), url.type(), true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;

  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info)) {
    NOTREACHED();
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  if (!file_info.is_directory())
    return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
  if (!db->RemoveFileInfo(file_id))
    return base::PLATFORM_FILE_ERROR_NOT_EMPTY;
  int64 growth = -UsageForPath(file_info.name.size());
  AllocateQuota(context, growth);
  UpdateUsage(context, url, growth);
  TouchDirectory(db, file_info.parent_id);
  context->change_observers()->Notify(
      &FileChangeObserver::OnRemoveDirectory, MakeTuple(url));
  return base::PLATFORM_FILE_OK;
}

base::PlatformFileError ObfuscatedFileUtil::CreateSnapshotFile(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    base::PlatformFileInfo* file_info,
    base::FilePath* platform_path,
    SnapshotFilePolicy* policy) {
  DCHECK(policy);
  // We're just returning the local file information.
  *policy = kSnapshotFileLocal;
  base::PlatformFileError error = GetFileInfo(
      context, url, file_info, platform_path);
  if (error == base::PLATFORM_FILE_OK && file_info->is_directory) {
    *file_info = base::PlatformFileInfo();
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;
  }
  return error;
}

bool ObfuscatedFileUtil::IsDirectoryEmpty(
    FileSystemOperationContext* context,
    const FileSystemURL& url) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      url.origin(), url.type(), false);
  if (!db)
    return true;  // Not a great answer, but it's what others do.
  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id))
    return true;  // Ditto.
  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info)) {
    DCHECK(!file_id);
    // It's the root directory and the database hasn't been initialized yet.
    return true;
  }
  if (!file_info.is_directory())
    return true;
  std::vector<FileId> children;
  // TODO(ericu): This could easily be made faster with help from the database.
  if (!db->ListChildren(file_id, &children))
    return true;
  return children.empty();
}

base::FilePath ObfuscatedFileUtil::GetDirectoryForOriginAndType(
    const GURL& origin,
    FileSystemType type,
    bool create,
    base::PlatformFileError* error_code) {
  base::FilePath origin_dir = GetDirectoryForOrigin(origin, create, error_code);
  if (origin_dir.empty())
    return base::FilePath();
  base::FilePath::StringType type_string = GetDirectoryNameForType(type);
  if (type_string.empty()) {
    LOG(WARNING) << "Unknown filesystem type requested:" << type;

    if (error_code)
      *error_code = base::PLATFORM_FILE_ERROR_INVALID_URL;
    return base::FilePath();
  }
  base::FilePath path = origin_dir.Append(type_string);
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  if (!file_util::DirectoryExists(path) &&
      (!create || !file_util::CreateDirectory(path))) {
    error = create ?
          base::PLATFORM_FILE_ERROR_FAILED :
          base::PLATFORM_FILE_ERROR_NOT_FOUND;
  }

  if (error_code)
    *error_code = error;
  return path;
}

bool ObfuscatedFileUtil::DeleteDirectoryForOriginAndType(
    const GURL& origin, FileSystemType type) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath origin_type_path = GetDirectoryForOriginAndType(origin, type, false,
                                                           &error);
  if (origin_type_path.empty())
    return true;

  if (error != base::PLATFORM_FILE_ERROR_NOT_FOUND) {
    // TODO(dmikurube): Consider the return value of DestroyDirectoryDatabase.
    // We ignore its error now since 1) it doesn't matter the final result, and
    // 2) it always returns false in Windows because of LevelDB's
    // implementation.
    // Information about failure would be useful for debugging.
    DestroyDirectoryDatabase(origin, type);
    if (!file_util::Delete(origin_type_path, true /* recursive */))
      return false;
  }

  base::FilePath origin_path = VirtualPath::DirName(origin_type_path);
  DCHECK_EQ(origin_path.value(),
            GetDirectoryForOrigin(origin, false, NULL).value());

  // At this point we are sure we had successfully deleted the origin/type
  // directory (i.e. we're ready to just return true).
  // See if we have other directories in this origin directory.
  std::vector<FileSystemType> other_types;
  if (type != kFileSystemTypeTemporary)
    other_types.push_back(kFileSystemTypeTemporary);
  if (type != kFileSystemTypePersistent)
    other_types.push_back(kFileSystemTypePersistent);
  if (type != kFileSystemTypeSyncable)
    other_types.push_back(kFileSystemTypeSyncable);

  for (size_t i = 0; i < other_types.size(); ++i) {
    if (file_util::DirectoryExists(
            origin_path.Append(GetDirectoryNameForType(other_types[i])))) {
      // Other type's directory exists; just return true here.
      return true;
    }
  }

  // No other directories seem exist. Try deleting the entire origin directory.
  InitOriginDatabase(false);
  if (origin_database_.get())
    origin_database_->RemovePathForOrigin(GetOriginIdentifierFromURL(origin));
  if (!file_util::Delete(origin_path, true /* recursive */))
    return false;

  return true;
}

// static
base::FilePath::StringType ObfuscatedFileUtil::GetDirectoryNameForType(
    FileSystemType type) {
  switch (type) {
    case kFileSystemTypeTemporary:
      return kTemporaryDirectoryName;
    case kFileSystemTypePersistent:
      return kPersistentDirectoryName;
    case kFileSystemTypeSyncable:
      return kSyncableDirectoryName;
    case kFileSystemTypeUnknown:
    default:
      return base::FilePath::StringType();
  }
}

ObfuscatedFileUtil::AbstractOriginEnumerator*
ObfuscatedFileUtil::CreateOriginEnumerator() {
  std::vector<FileSystemOriginDatabase::OriginRecord> origins;

  InitOriginDatabase(false);
  return new ObfuscatedOriginEnumerator(
      origin_database_.get(), file_system_directory_);
}

bool ObfuscatedFileUtil::DestroyDirectoryDatabase(
    const GURL& origin, FileSystemType type) {
  std::string type_string = GetFileSystemTypeString(type);
  if (type_string.empty()) {
    LOG(WARNING) << "Unknown filesystem type requested:" << type;
    return true;
  }
  std::string key = GetOriginIdentifierFromURL(origin) + type_string;
  DirectoryMap::iterator iter = directories_.find(key);
  if (iter != directories_.end()) {
    FileSystemDirectoryDatabase* database = iter->second;
    directories_.erase(iter);
    delete database;
  }

  PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath path = GetDirectoryForOriginAndType(origin, type, false, &error);
  if (path.empty() || error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
    return true;
  return FileSystemDirectoryDatabase::DestroyDatabase(path);
}

// static
int64 ObfuscatedFileUtil::ComputeFilePathCost(const base::FilePath& path) {
  return UsageForPath(VirtualPath::BaseName(path).value().size());
}

PlatformFileError ObfuscatedFileUtil::GetFileInfoInternal(
    FileSystemDirectoryDatabase* db,
    FileSystemOperationContext* context,
    const GURL& origin,
    FileSystemType type,
    FileId file_id,
    FileInfo* local_info,
    base::PlatformFileInfo* file_info,
    base::FilePath* platform_file_path) {
  DCHECK(db);
  DCHECK(context);
  DCHECK(file_info);
  DCHECK(platform_file_path);

  if (!db->GetFileInfo(file_id, local_info)) {
    NOTREACHED();
    return base::PLATFORM_FILE_ERROR_FAILED;
  }

  if (local_info->is_directory()) {
    file_info->size = 0;
    file_info->is_directory = true;
    file_info->is_symbolic_link = false;
    file_info->last_modified = local_info->modification_time;
    *platform_file_path = base::FilePath();
    // We don't fill in ctime or atime.
    return base::PLATFORM_FILE_OK;
  }
  if (local_info->data_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  base::FilePath local_path = DataPathToLocalPath(
      origin, type, local_info->data_path);
  base::PlatformFileError error = NativeFileUtil::GetFileInfo(
      local_path, file_info);
  // We should not follow symbolic links in sandboxed file system.
  if (file_util::IsLink(local_path)) {
    LOG(WARNING) << "Found a symbolic file.";
    error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
  }
  if (error == base::PLATFORM_FILE_OK) {
    *platform_file_path = local_path;
  } else if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND) {
    LOG(WARNING) << "Lost a backing file.";
    InvalidateUsageCache(context, origin, type);
    if (!db->RemoveFileInfo(file_id))
      return base::PLATFORM_FILE_ERROR_FAILED;
  }
  return error;
}

PlatformFileError ObfuscatedFileUtil::CreateFile(
    FileSystemOperationContext* context,
    const base::FilePath& src_file_path,
    const GURL& dest_origin,
    FileSystemType dest_type,
    FileInfo* dest_file_info, int file_flags, PlatformFile* handle) {
  if (handle)
    *handle = base::kInvalidPlatformFileValue;
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      dest_origin, dest_type, true);

  PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath root = GetDirectoryForOriginAndType(dest_origin, dest_type, false,
                                               &error);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  base::FilePath dest_local_path;
  error = GenerateNewLocalPath(db, context, dest_origin, dest_type,
                               &dest_local_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  bool created = false;
  if (!src_file_path.empty()) {
    DCHECK(!file_flags);
    DCHECK(!handle);
    error = NativeFileUtil::CopyOrMoveFile(
        src_file_path, dest_local_path, true /* copy */);
    created = true;
  } else {
    if (file_util::PathExists(dest_local_path)) {
      if (!file_util::Delete(dest_local_path, true /* recursive */)) {
        NOTREACHED();
        return base::PLATFORM_FILE_ERROR_FAILED;
      }
      LOG(WARNING) << "A stray file detected";
      InvalidateUsageCache(context, dest_origin, dest_type);
    }

    if (handle) {
      error = NativeFileUtil::CreateOrOpen(
          dest_local_path, file_flags, handle, &created);
      // If this succeeds, we must close handle on any subsequent error.
    } else {
      DCHECK(!file_flags);  // file_flags is only used by CreateOrOpen.
      error = NativeFileUtil::EnsureFileExists(dest_local_path, &created);
    }
  }
  if (error != base::PLATFORM_FILE_OK)
    return error;

  if (!created) {
    NOTREACHED();
    if (handle) {
      DCHECK_NE(base::kInvalidPlatformFileValue, *handle);
      base::ClosePlatformFile(*handle);
      file_util::Delete(dest_local_path, false /* recursive */);
    }
    return base::PLATFORM_FILE_ERROR_FAILED;
  }

  // This removes the root, including the trailing slash, leaving a relative
  // path.
  dest_file_info->data_path = base::FilePath(
      dest_local_path.value().substr(root.value().length() + 1));

  FileId file_id;
  if (!db->AddFileInfo(*dest_file_info, &file_id)) {
    if (handle) {
      DCHECK_NE(base::kInvalidPlatformFileValue, *handle);
      base::ClosePlatformFile(*handle);
    }
    file_util::Delete(dest_local_path, false /* recursive */);
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  TouchDirectory(db, dest_file_info->parent_id);

  return base::PLATFORM_FILE_OK;
}

base::FilePath ObfuscatedFileUtil::DataPathToLocalPath(
    const GURL& origin, FileSystemType type, const base::FilePath& data_path) {
  PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath root = GetDirectoryForOriginAndType(origin, type, false, &error);
  if (error != base::PLATFORM_FILE_OK)
    return base::FilePath();
  return root.Append(data_path);
}

// TODO(ericu): How to do the whole validation-without-creation thing?
// We may not have quota even to create the database.
// Ah, in that case don't even get here?
// Still doesn't answer the quota issue, though.
FileSystemDirectoryDatabase* ObfuscatedFileUtil::GetDirectoryDatabase(
    const GURL& origin, FileSystemType type, bool create) {
  std::string type_string = GetFileSystemTypeString(type);
  if (type_string.empty()) {
    LOG(WARNING) << "Unknown filesystem type requested:" << type;
    return NULL;
  }
  std::string key = GetOriginIdentifierFromURL(origin) + type_string;
  DirectoryMap::iterator iter = directories_.find(key);
  if (iter != directories_.end()) {
    MarkUsed();
    return iter->second;
  }

  PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath path = GetDirectoryForOriginAndType(origin, type, create, &error);
  if (error != base::PLATFORM_FILE_OK) {
    LOG(WARNING) << "Failed to get origin+type directory: " << path.value();
    return NULL;
  }
  MarkUsed();
  FileSystemDirectoryDatabase* database = new FileSystemDirectoryDatabase(path);
  directories_[key] = database;
  return database;
}

base::FilePath ObfuscatedFileUtil::GetDirectoryForOrigin(
    const GURL& origin, bool create, base::PlatformFileError* error_code) {
  if (!InitOriginDatabase(create)) {
    if (error_code) {
      *error_code = create ?
          base::PLATFORM_FILE_ERROR_FAILED :
          base::PLATFORM_FILE_ERROR_NOT_FOUND;
    }
    return base::FilePath();
  }
  base::FilePath directory_name;
  std::string id = GetOriginIdentifierFromURL(origin);

  bool exists_in_db = origin_database_->HasOriginPath(id);
  if (!exists_in_db && !create) {
    if (error_code)
      *error_code = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    return base::FilePath();
  }
  if (!origin_database_->GetPathForOrigin(id, &directory_name)) {
    if (error_code)
      *error_code = base::PLATFORM_FILE_ERROR_FAILED;
    return base::FilePath();
  }

  base::FilePath path = file_system_directory_.Append(directory_name);
  bool exists_in_fs = file_util::DirectoryExists(path);
  if (!exists_in_db && exists_in_fs) {
    if (!file_util::Delete(path, true)) {
      if (error_code)
        *error_code = base::PLATFORM_FILE_ERROR_FAILED;
      return base::FilePath();
    }
    exists_in_fs = false;
  }

  if (!exists_in_fs) {
    if (!create || !file_util::CreateDirectory(path)) {
      if (error_code)
        *error_code = create ?
            base::PLATFORM_FILE_ERROR_FAILED :
            base::PLATFORM_FILE_ERROR_NOT_FOUND;
      return base::FilePath();
    }
  }

  if (error_code)
    *error_code = base::PLATFORM_FILE_OK;

  return path;
}

void ObfuscatedFileUtil::InvalidateUsageCache(
    FileSystemOperationContext* context,
    const GURL& origin,
    FileSystemType type) {
  context->file_system_context()->GetQuotaUtil(type)->
      InvalidateUsageCache(origin, type);
}

void ObfuscatedFileUtil::MarkUsed() {
  if (timer_.IsRunning())
    timer_.Reset();
  else
    timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(kFlushDelaySeconds),
                 this, &ObfuscatedFileUtil::DropDatabases);
}

void ObfuscatedFileUtil::DropDatabases() {
  origin_database_.reset();
  STLDeleteContainerPairSecondPointers(
      directories_.begin(), directories_.end());
  directories_.clear();
}

bool ObfuscatedFileUtil::InitOriginDatabase(bool create) {
  if (!origin_database_.get()) {
    if (!create && !file_util::DirectoryExists(file_system_directory_))
      return false;
    if (!file_util::CreateDirectory(file_system_directory_)) {
      LOG(WARNING) << "Failed to create FileSystem directory: " <<
          file_system_directory_.value();
      return false;
    }
    origin_database_.reset(
        new FileSystemOriginDatabase(file_system_directory_));
  }
  return true;
}

PlatformFileError ObfuscatedFileUtil::GenerateNewLocalPath(
    FileSystemDirectoryDatabase* db,
    FileSystemOperationContext* context,
    const GURL& origin,
    FileSystemType type,
    base::FilePath* local_path) {
  DCHECK(local_path);
  int64 number;
  if (!db || !db->GetNextInteger(&number))
    return base::PLATFORM_FILE_ERROR_FAILED;

  PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath new_local_path = GetDirectoryForOriginAndType(origin, type,
                                                               false, &error);
  if (error != base::PLATFORM_FILE_OK)
    return base::PLATFORM_FILE_ERROR_FAILED;

  // We use the third- and fourth-to-last digits as the directory.
  int64 directory_number = number % 10000 / 100;
  new_local_path = new_local_path.AppendASCII(
      base::StringPrintf("%02" PRId64, directory_number));

  error = NativeFileUtil::CreateDirectory(
      new_local_path, false /* exclusive */, false /* recursive */);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  *local_path =
      new_local_path.AppendASCII(base::StringPrintf("%08" PRId64, number));
  return base::PLATFORM_FILE_OK;
}

}  // namespace fileapi
