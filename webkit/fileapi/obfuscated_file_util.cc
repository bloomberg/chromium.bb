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
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path.h"
#include "webkit/fileapi/file_system_quota_util.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/quota/quota_manager.h"

// Example of various paths:
//   void ObfuscatedFileUtil::DoSomething(const FileSystemPath& path) {
//     FilePath virtual_path = path.internal_path();
//     FileSystemPath underlying_path = GetUnderlyingPath(path);
//     FilePath local_file_path = GetLocalFilePath(path);
//
//     // |data_path| is native file path that is relative to the directory for
//     // the origin and type.  We store |data_path| in DirectoryDatabase.
//     FilePath::StringType data_path =
//         UnderlyingPathToDataPath(underlying_path);
//
//     underlying_file_util()->DoSomething(underlying_path);
//     file_util::DoAnother(local_file_path);
//  }

namespace fileapi {

namespace {

typedef FileSystemDirectoryDatabase::FileId FileId;
typedef FileSystemDirectoryDatabase::FileInfo FileInfo;

const int64 kFlushDelaySeconds = 10 * 60;  // 10 minutes

void InitFileInfo(
    FileSystemDirectoryDatabase::FileInfo* file_info,
    FileSystemDirectoryDatabase::FileId parent_id,
    const FilePath::StringType& file_name) {
  DCHECK(file_info);
  file_info->parent_id = parent_id;
  file_info->name = file_name;
}

bool IsRootDirectory(const FileSystemPath& path) {
  return (path.internal_path().empty() ||
          path.internal_path().value() == FILE_PATH_LITERAL("/"));
}

// Costs computed as per crbug.com/86114, based on the LevelDB implementation of
// path storage under Linux.  It's not clear if that will differ on Windows, on
// which FilePath uses wide chars [since they're converted to UTF-8 for storage
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
    const GURL& origin,
    FileSystemType type,
    int64 growth) {
  FileSystemQuotaUtil* quota_util =
      context->file_system_context()->GetQuotaUtil(type);
  quota::QuotaManagerProxy* quota_manager_proxy =
      context->file_system_context()->quota_manager_proxy();
  quota_util->UpdateOriginUsageOnFileThread(
      quota_manager_proxy, origin, type, growth);
}

void TouchDirectory(FileSystemDirectoryDatabase* db, FileId dir_id) {
  DCHECK(db);
  if (!db->UpdateModificationTime(dir_id, base::Time::Now()))
    NOTREACHED();
}

const FilePath::CharType kLegacyDataDirectory[] = FILE_PATH_LITERAL("Legacy");

const FilePath::CharType kTemporaryDirectoryName[] = FILE_PATH_LITERAL("t");
const FilePath::CharType kPersistentDirectoryName[] = FILE_PATH_LITERAL("p");

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
      const FileSystemPath& root_path,
      bool recursive)
      : db_(db),
        context_(context),
        obfuscated_file_util_(obfuscated_file_util),
        origin_(root_path.origin()),
        type_(root_path.type()),
        recursive_(recursive) {
    FilePath root_virtual_path = root_path.internal_path();
    FileId file_id;

    if (!db_->GetFileWithPath(root_virtual_path, &file_id))
      return;

    FileRecord record = { file_id, root_virtual_path };
    recurse_queue_.push(record);
  }

  virtual ~ObfuscatedFileEnumerator() {}

  virtual FilePath Next() OVERRIDE {
    ProcessRecurseQueue();
    if (display_stack_.empty())
      return FilePath();

    current_file_id_ = display_stack_.back();
    display_stack_.pop_back();

    FileInfo file_info;
    FilePath platform_file_path;
    base::PlatformFileError error =
        obfuscated_file_util_->GetFileInfoInternal(
            db_, context_, origin_, type_, current_file_id_,
            &file_info, &current_platform_file_info_, &platform_file_path);
    if (error != base::PLATFORM_FILE_OK)
      return Next();

    FilePath virtual_path =
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

  virtual bool IsLink() OVERRIDE {
    return false;
  }

 private:
  typedef FileSystemDirectoryDatabase::FileId FileId;
  typedef FileSystemDirectoryDatabase::FileInfo FileInfo;

  struct FileRecord {
    FileId file_id;
    FilePath virtual_path;
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
  FilePath current_parent_virtual_path_;

  FileId current_file_id_;
  base::PlatformFileInfo current_platform_file_info_;
};

class ObfuscatedOriginEnumerator
    : public ObfuscatedFileUtil::AbstractOriginEnumerator {
 public:
  typedef FileSystemOriginDatabase::OriginRecord OriginRecord;
  ObfuscatedOriginEnumerator(
      FileSystemOriginDatabase* origin_database,
      const FilePath& base_file_path)
      : base_file_path_(base_file_path) {
    if (origin_database)
      origin_database->ListAllOrigins(&origins_);
  }

  ~ObfuscatedOriginEnumerator() {}

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
    FilePath::StringType type_string =
        ObfuscatedFileUtil::GetDirectoryNameForType(type);
    if (type_string.empty()) {
      NOTREACHED();
      return false;
    }
    FilePath path = base_file_path_.Append(current_.path).Append(type_string);
    return file_util::DirectoryExists(path);
  }

 private:
  std::vector<OriginRecord> origins_;
  OriginRecord current_;
  FilePath base_file_path_;
};

ObfuscatedFileUtil::ObfuscatedFileUtil(
    const FilePath& file_system_directory,
    FileSystemFileUtil* underlying_file_util)
    : FileSystemFileUtil(underlying_file_util),
      file_system_directory_(file_system_directory) {
}

PlatformFileError ObfuscatedFileUtil::CreateOrOpen(
    FileSystemOperationContext* context,
    const FileSystemPath& path, int file_flags,
    PlatformFile* file_handle, bool* created) {
  DCHECK(!(file_flags & (base::PLATFORM_FILE_DELETE_ON_CLOSE |
        base::PLATFORM_FILE_HIDDEN | base::PLATFORM_FILE_EXCLUSIVE_READ |
        base::PLATFORM_FILE_EXCLUSIVE_WRITE)));
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      path.origin(), path.type(), true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;
  FileId file_id;
  if (!db->GetFileWithPath(path.internal_path(), &file_id)) {
    // The file doesn't exist.
    if (!(file_flags & (base::PLATFORM_FILE_CREATE |
        base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_OPEN_ALWAYS)))
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    FileId parent_id;
    if (!db->GetFileWithPath(path.internal_path().DirName(),
                             &parent_id))
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    FileInfo file_info;
    InitFileInfo(&file_info, parent_id,
                 VirtualPath::BaseName(path.internal_path()).value());

    int64 growth = UsageForPath(file_info.name.size());
    if (!AllocateQuota(context, growth))
      return base::PLATFORM_FILE_ERROR_NO_SPACE;
    PlatformFileError error = CreateFile(
        context, FilePath(),
        path.origin(), path.type(), &file_info,
        file_flags, file_handle);
    if (created && base::PLATFORM_FILE_OK == error) {
      *created = true;
      UpdateUsage(context, path.origin(), path.type(), growth);
    }
    return error;
  }
  if (file_flags & base::PLATFORM_FILE_CREATE)
    return base::PLATFORM_FILE_ERROR_EXISTS;

  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info)) {
    NOTREACHED();
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  if (file_info.is_directory())
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;
  FileSystemPath underlying_path = DataPathToUnderlyingPath(
      path.origin(), path.type(), file_info.data_path);
  base::PlatformFileError error = underlying_file_util()->CreateOrOpen(
      context, underlying_path, file_flags, file_handle, created);
  if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND) {
    // TODO(tzik): Also invalidate on-memory usage cache in UsageTracker.
    // TODO(tzik): Delete database entry after ensuring the file lost.
    InvalidateUsageCache(context, path.origin(), path.type());
    LOG(WARNING) << "Lost a backing file.";
    error = base::PLATFORM_FILE_ERROR_FAILED;
  }
  return error;
}

PlatformFileError ObfuscatedFileUtil::EnsureFileExists(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    bool* created) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      path.origin(), path.type(), true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;

  FileId file_id;
  if (db->GetFileWithPath(path.internal_path(), &file_id)) {
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
  if (!db->GetFileWithPath(path.internal_path().DirName(), &parent_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  FileInfo file_info;
  InitFileInfo(&file_info, parent_id,
               VirtualPath::BaseName(path.internal_path()).value());

  int64 growth = UsageForPath(file_info.name.size());
  if (!AllocateQuota(context, growth))
    return base::PLATFORM_FILE_ERROR_NO_SPACE;
  PlatformFileError error = CreateFile(
      context, FilePath(), path.origin(), path.type(), &file_info, 0, NULL);
  if (created && base::PLATFORM_FILE_OK == error) {
    *created = true;
    UpdateUsage(context, path.origin(), path.type(), growth);
  }
  return error;
}

PlatformFileError ObfuscatedFileUtil::CreateDirectory(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    bool exclusive,
    bool recursive) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      path.origin(), path.type(), true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;

  FileId file_id;
  if (db->GetFileWithPath(path.internal_path(), &file_id)) {
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

  std::vector<FilePath::StringType> components;
  VirtualPath::GetComponents(path.internal_path(), &components);
  FileId parent_id = 0;
  size_t index;
  for (index = 0; index < components.size(); ++index) {
    FilePath::StringType name = components[index];
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
    UpdateUsage(context, path.origin(), path.type(), growth);
    if (first) {
      first = false;
      TouchDirectory(db, file_info.parent_id);
    }
  }
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileUtil::GetFileInfo(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    base::PlatformFileInfo* file_info,
    FilePath* platform_file_path) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      path.origin(), path.type(), false);
  if (!db)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileId file_id;
  if (!db->GetFileWithPath(path.internal_path(), &file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileInfo local_info;
  return GetFileInfoInternal(db, context,
                             path.origin(), path.type(),
                             file_id, &local_info,
                             file_info, platform_file_path);
}

FileSystemFileUtil::AbstractFileEnumerator*
ObfuscatedFileUtil::CreateFileEnumerator(
    FileSystemOperationContext* context,
    const FileSystemPath& root_path,
    bool recursive) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      root_path.origin(), root_path.type(), false);
  if (!db)
    return new FileSystemFileUtil::EmptyFileEnumerator();
  return new ObfuscatedFileEnumerator(
      db, context, this, root_path, recursive);
}

PlatformFileError ObfuscatedFileUtil::GetLocalFilePath(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    FilePath* local_file_path) {
  FileSystemPath underlying_path = GetUnderlyingPath(path);
  if (underlying_path.internal_path().empty())
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  *local_file_path = underlying_path.internal_path();
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileUtil::Touch(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      path.origin(), path.type(), false);
  if (!db)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileId file_id;
  if (!db->GetFileWithPath(path.internal_path(), &file_id))
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
  FileSystemPath underlying_path = DataPathToUnderlyingPath(
      path.origin(), path.type(), file_info.data_path);
  return underlying_file_util()->Touch(
      context, underlying_path,
      last_access_time, last_modified_time);
}

PlatformFileError ObfuscatedFileUtil::Truncate(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    int64 length) {
  base::PlatformFileInfo file_info;
  FilePath local_path;
  base::PlatformFileError error =
      GetFileInfo(context, path, &file_info, &local_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  int64 growth = length - file_info.size;
  if (!AllocateQuota(context, growth))
    return base::PLATFORM_FILE_ERROR_NO_SPACE;
  error = underlying_file_util()->Truncate(
      context, path.WithInternalPath(local_path), length);
  if (error == base::PLATFORM_FILE_OK)
    UpdateUsage(context, path.origin(), path.type(), growth);
  return error;
}

bool ObfuscatedFileUtil::PathExists(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      path.origin(), path.type(), false);
  if (!db)
    return false;
  FileId file_id;
  return db->GetFileWithPath(path.internal_path(), &file_id);
}

bool ObfuscatedFileUtil::DirectoryExists(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  if (IsRootDirectory(path)) {
    // It's questionable whether we should return true or false for the
    // root directory of nonexistent origin, but here we return true
    // as the current implementation of ReadDirectory always returns an empty
    // array (rather than erroring out with NOT_FOUND_ERR even) for
    // nonexistent origins.
    // Note: if you're going to change this behavior please also consider
    // changiing the ReadDirectory's behavior!
    return true;
  }
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      path.origin(), path.type(), false);
  if (!db)
    return false;
  FileId file_id;
  if (!db->GetFileWithPath(path.internal_path(), &file_id))
    return false;
  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info)) {
    NOTREACHED();
    return false;
  }
  return file_info.is_directory();
}

bool ObfuscatedFileUtil::IsDirectoryEmpty(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      path.origin(), path.type(), false);
  if (!db)
    return true;  // Not a great answer, but it's what others do.
  FileId file_id;
  if (!db->GetFileWithPath(path.internal_path(), &file_id))
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

PlatformFileError ObfuscatedFileUtil::CopyOrMoveFile(
    FileSystemOperationContext* context,
    const FileSystemPath& src_path,
    const FileSystemPath& dest_path,
    bool copy) {
  // Cross-filesystem copies and moves should be handled via CopyInForeignFile.
  DCHECK(src_path.origin() == dest_path.origin());
  DCHECK(src_path.type() == dest_path.type());

  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      src_path.origin(), src_path.type(), true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;

  FileId src_file_id;
  if (!db->GetFileWithPath(src_path.internal_path(), &src_file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  FileId dest_file_id;
  bool overwrite = db->GetFileWithPath(dest_path.internal_path(),
                                       &dest_file_id);

  FileInfo src_file_info;
  base::PlatformFileInfo src_platform_file_info;
  FilePath src_local_file_path;
  base::PlatformFileError error = GetFileInfoInternal(
      db, context, src_path.origin(), src_path.type(), src_file_id,
      &src_file_info, &src_platform_file_info, &src_local_file_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;
  if (src_file_info.is_directory())
    return base::PLATFORM_FILE_ERROR_FAILED;

  FileInfo dest_file_info;
  base::PlatformFileInfo dest_platform_file_info;  // overwrite case only
  FileSystemPath dest_underlying_path;
  if (overwrite) {
    FilePath dest_local_file_path;
    base::PlatformFileError error = GetFileInfoInternal(
        db, context, dest_path.origin(), dest_path.type(), dest_file_id,
        &dest_file_info, &dest_platform_file_info, &dest_local_file_path);
    if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
      overwrite = false;  // fallback to non-overwrite case
    else if (error != base::PLATFORM_FILE_OK)
      return error;
    else if (dest_file_info.is_directory())
      return base::PLATFORM_FILE_ERROR_FAILED;
    else
      dest_underlying_path = dest_path.WithInternalPath(dest_local_file_path);
  }
  if (!overwrite) {
    FileId dest_parent_id;
    if (!db->GetFileWithPath(dest_path.internal_path().DirName(),
                             &dest_parent_id)) {
      NOTREACHED();  // We shouldn't be called in this case.
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    }

    dest_file_info = src_file_info;
    dest_file_info.parent_id = dest_parent_id;
    dest_file_info.name =
        VirtualPath::BaseName(dest_path.internal_path()).value();
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
      error = underlying_file_util()->CopyOrMoveFile(
          context,
          src_path.WithInternalPath(src_local_file_path),
          dest_underlying_path,
          true /* copy */);
    } else {  // non-overwrite
      error = CreateFile(context, src_local_file_path,
                         dest_path.origin(), dest_path.type(),
                         &dest_file_info, 0, NULL);
    }
  } else {
    if (overwrite) {
      if (db->OverwritingMoveFile(src_file_id, dest_file_id)) {
        if (base::PLATFORM_FILE_OK !=
            underlying_file_util()->DeleteFile(context, dest_underlying_path))
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

  if (!copy)
    TouchDirectory(db, src_file_info.parent_id);
  TouchDirectory(db, dest_file_info.parent_id);

  UpdateUsage(context, dest_path.origin(), dest_path.type(), growth);
  return error;
}

PlatformFileError ObfuscatedFileUtil::CopyInForeignFile(
    FileSystemOperationContext* context,
    const FilePath& src_file_path,
    const FileSystemPath& dest_path) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      dest_path.origin(), dest_path.type(), true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;

  base::PlatformFileInfo src_platform_file_info;
  if (!file_util::GetFileInfo(src_file_path, &src_platform_file_info))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  FileId dest_file_id;
  bool overwrite = db->GetFileWithPath(dest_path.internal_path(),
                                       &dest_file_id);

  FileInfo dest_file_info;
  base::PlatformFileInfo dest_platform_file_info;  // overwrite case only
  if (overwrite) {
    FilePath dest_local_file_path;
    base::PlatformFileError error = GetFileInfoInternal(
        db, context, dest_path.origin(), dest_path.type(), dest_file_id,
        &dest_file_info, &dest_platform_file_info, &dest_local_file_path);
    if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
      overwrite = false;  // fallback to non-overwrite case
    else if (error != base::PLATFORM_FILE_OK)
      return error;
    else if (dest_file_info.is_directory())
      return base::PLATFORM_FILE_ERROR_FAILED;
  }
  if (!overwrite) {
    FileId dest_parent_id;
    if (!db->GetFileWithPath(dest_path.internal_path().DirName(),
                             &dest_parent_id) ||
        !dest_file_info.is_directory()) {
      NOTREACHED();
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    }
    InitFileInfo(&dest_file_info, dest_parent_id,
                 VirtualPath::BaseName(dest_path.internal_path()).value());
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
    FileSystemPath dest_underlying_path = DataPathToUnderlyingPath(
        dest_path.origin(), dest_path.type(), dest_file_info.data_path);
    error = underlying_file_util()->CopyInForeignFile(
        context, src_file_path, dest_underlying_path);
  } else {
    error = CreateFile(context, src_file_path,
                       dest_path.origin(), dest_path.type(),
                       &dest_file_info, 0, NULL);
  }

  if (error != base::PLATFORM_FILE_OK)
    return error;

  UpdateUsage(context, dest_path.origin(), dest_path.type(), growth);
  TouchDirectory(db, dest_file_info.parent_id);
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileUtil::DeleteFile(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      path.origin(), path.type(), true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;
  FileId file_id;
  if (!db->GetFileWithPath(path.internal_path(), &file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  FileInfo file_info;
  base::PlatformFileInfo platform_file_info;
  FilePath local_file_path;
  base::PlatformFileError error = GetFileInfoInternal(
      db, context, path.origin(), path.type(), file_id,
      &file_info, &platform_file_info, &local_file_path);
  if (error != base::PLATFORM_FILE_ERROR_NOT_FOUND &&
      error != base::PLATFORM_FILE_OK)
    return error;

  if (file_info.is_directory()) {
    NOTREACHED();
    return base::PLATFORM_FILE_ERROR_FAILED;
  }

  int64 growth = -UsageForPath(file_info.name.size()) - platform_file_info.size;
  AllocateQuota(context, growth);
  if (!db->RemoveFileInfo(file_id)) {
    NOTREACHED();
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  UpdateUsage(context, path.origin(), path.type(), growth);
  TouchDirectory(db, file_info.parent_id);

  if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
    return base::PLATFORM_FILE_OK;

  error = underlying_file_util()->DeleteFile(
          context, path.WithInternalPath(local_file_path));
  if (base::PLATFORM_FILE_OK != error)
    LOG(WARNING) << "Leaked a backing file.";
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileUtil::DeleteSingleDirectory(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      path.origin(), path.type(), true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;

  FileId file_id;
  if (!db->GetFileWithPath(path.internal_path(), &file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info) || !file_info.is_directory()) {
    NOTREACHED();
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  if (!db->RemoveFileInfo(file_id))
    return base::PLATFORM_FILE_ERROR_NOT_EMPTY;
  int64 growth = -UsageForPath(file_info.name.size());
  AllocateQuota(context, growth);
  UpdateUsage(context, path.origin(), path.type(), growth);
  TouchDirectory(db, file_info.parent_id);
  return base::PLATFORM_FILE_OK;
}

FilePath ObfuscatedFileUtil::GetDirectoryForOriginAndType(
    const GURL& origin,
    FileSystemType type,
    bool create,
    base::PlatformFileError* error_code) {
  FilePath origin_dir = GetDirectoryForOrigin(origin, create, error_code);
  if (origin_dir.empty())
    return FilePath();
  FilePath::StringType type_string = GetDirectoryNameForType(type);
  if (type_string.empty()) {
    LOG(WARNING) << "Unknown filesystem type requested:" << type;

    if (error_code)
      *error_code = base::PLATFORM_FILE_ERROR_INVALID_URL;
    return FilePath();
  }
  FilePath path = origin_dir.Append(type_string);
  if (!file_util::DirectoryExists(path) &&
      (!create || !file_util::CreateDirectory(path))) {
    if (error_code) {
      *error_code = create ?
          base::PLATFORM_FILE_ERROR_FAILED :
          base::PLATFORM_FILE_ERROR_NOT_FOUND;
    }
    return FilePath();
  }

  if (error_code)
    *error_code = base::PLATFORM_FILE_OK;
  return path;
}

bool ObfuscatedFileUtil::DeleteDirectoryForOriginAndType(
    const GURL& origin, FileSystemType type) {
  FilePath origin_type_path = GetDirectoryForOriginAndType(origin, type, false);
  if (!file_util::PathExists(origin_type_path))
    return true;

  // TODO(dmikurube): Consider the return value of DestroyDirectoryDatabase.
  // We ignore its error now since 1) it doesn't matter the final result, and
  // 2) it always returns false in Windows because of LevelDB's implementation.
  // Information about failure would be useful for debugging.
  DestroyDirectoryDatabase(origin, type);
  if (!file_util::Delete(origin_type_path, true /* recursive */))
    return false;

  FilePath origin_path = origin_type_path.DirName();
  DCHECK_EQ(origin_path.value(),
            GetDirectoryForOrigin(origin, false, NULL).value());

  // Delete the origin directory if the deleted one was the last remaining
  // type for the origin.
  if (file_util::Delete(origin_path, false /* recursive */)) {
    InitOriginDatabase(false);
    if (origin_database_.get())
      origin_database_->RemovePathForOrigin(GetOriginIdentifierFromURL(origin));
  }

  // At this point we are sure we had successfully deleted the origin/type
  // directory, so just returning true here.
  return true;
}

bool ObfuscatedFileUtil::MigrateFromOldSandbox(
    const GURL& origin_url, FileSystemType type, const FilePath& src_root) {
  if (!DestroyDirectoryDatabase(origin_url, type))
    return false;
  FilePath dest_root = GetDirectoryForOriginAndType(origin_url, type, true);
  if (dest_root.empty())
    return false;
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      origin_url, type, true);
  if (!db)
    return false;

  file_util::FileEnumerator file_enum(src_root, true,
      static_cast<file_util::FileEnumerator::FileType>(
          file_util::FileEnumerator::FILES |
          file_util::FileEnumerator::DIRECTORIES));
  FilePath src_full_path;
  size_t root_path_length = src_root.value().length() + 1;  // +1 for the slash
  while (!(src_full_path = file_enum.Next()).empty()) {
    file_util::FileEnumerator::FindInfo info;
    file_enum.GetFindInfo(&info);
    FilePath relative_virtual_path =
        FilePath(src_full_path.value().substr(root_path_length));
    if (relative_virtual_path.empty()) {
      LOG(WARNING) << "Failed to convert path to relative: " <<
          src_full_path.value();
      return false;
    }
    FileId file_id;
    if (db->GetFileWithPath(relative_virtual_path, &file_id)) {
      NOTREACHED();  // File already exists.
      return false;
    }
    if (!db->GetFileWithPath(relative_virtual_path.DirName(), &file_id)) {
      NOTREACHED();  // Parent doesn't exist.
      return false;
    }

    FileInfo file_info;
    file_info.name = VirtualPath::BaseName(src_full_path).value();
    if (file_util::FileEnumerator::IsDirectory(info)) {
#if defined(OS_WIN)
      file_info.modification_time =
          base::Time::FromFileTime(info.ftLastWriteTime);
#elif defined(OS_POSIX)
      file_info.modification_time = base::Time::FromTimeT(info.stat.st_mtime);
#endif
    } else {
      file_info.data_path =
          FilePath(kLegacyDataDirectory).Append(relative_virtual_path);
    }
    file_info.parent_id = file_id;
    if (!db->AddFileInfo(file_info, &file_id)) {
      NOTREACHED();
      return false;
    }
  }
  // TODO(ericu): Should we adjust the mtime of the root directory to match as
  // well?
  FilePath legacy_dest_dir = dest_root.Append(kLegacyDataDirectory);

  if (!file_util::Move(src_root, legacy_dest_dir)) {
    LOG(WARNING) <<
        "The final step of a migration failed; I'll try to clean up.";
    db = NULL;
    DestroyDirectoryDatabase(origin_url, type);
    return false;
  }
  return true;
}

// static
FilePath::StringType ObfuscatedFileUtil::GetDirectoryNameForType(
    FileSystemType type) {
  switch (type) {
    case kFileSystemTypeTemporary:
      return kTemporaryDirectoryName;
    case kFileSystemTypePersistent:
      return kPersistentDirectoryName;
    case kFileSystemTypeUnknown:
    default:
      return FilePath::StringType();
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

  FilePath path = GetDirectoryForOriginAndType(origin, type, false);
  if (path.empty())
    return true;
  if (!file_util::DirectoryExists(path))
    return true;
  return FileSystemDirectoryDatabase::DestroyDatabase(path);
}

// static
int64 ObfuscatedFileUtil::ComputeFilePathCost(const FilePath& path) {
  return UsageForPath(VirtualPath::BaseName(path).value().size());
}

ObfuscatedFileUtil::~ObfuscatedFileUtil() {
  DropDatabases();
}

PlatformFileError ObfuscatedFileUtil::GetFileInfoInternal(
    FileSystemDirectoryDatabase* db,
    FileSystemOperationContext* context,
    const GURL& origin,
    FileSystemType type,
    FileId file_id,
    FileInfo* local_info,
    base::PlatformFileInfo* file_info,
    FilePath* platform_file_path) {
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
    *platform_file_path = FilePath();
    // We don't fill in ctime or atime.
    return base::PLATFORM_FILE_OK;
  }
  if (local_info->data_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  FileSystemPath underlying_path = DataPathToUnderlyingPath(
      origin, type, local_info->data_path);
  base::PlatformFileError error = underlying_file_util()->GetFileInfo(
      context, underlying_path, file_info, platform_file_path);
  if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND) {
    LOG(WARNING) << "Lost a backing file.";
    InvalidateUsageCache(context, origin, type);
    if (!db->RemoveFileInfo(file_id))
      return base::PLATFORM_FILE_ERROR_FAILED;
  }
  return error;
}

PlatformFileError ObfuscatedFileUtil::CreateFile(
    FileSystemOperationContext* context,
    const FilePath& src_file_path,
    const GURL& dest_origin,
    FileSystemType dest_type,
    FileInfo* dest_file_info, int file_flags, PlatformFile* handle) {
  if (handle)
    *handle = base::kInvalidPlatformFileValue;
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      dest_origin, dest_type, true);

  FileSystemPath dest_path;
  PlatformFileError error = GenerateNewUnderlyingPath(
      db, context, dest_origin, dest_type, &dest_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  FilePath data_path = UnderlyingPathToDataPath(dest_path);
  if (data_path.empty())
    return base::PLATFORM_FILE_ERROR_FAILED;

  bool created = false;
  if (!src_file_path.empty()) {
    DCHECK(!file_flags);
    DCHECK(!handle);
    error = underlying_file_util()->CopyInForeignFile(
        context, src_file_path, dest_path);
    created = true;
  } else {
    FilePath path;
    underlying_file_util()->GetLocalFilePath(context, dest_path, &path);
    if (file_util::PathExists(path)) {
      if (!file_util::Delete(path, true)) {
        NOTREACHED();
        return base::PLATFORM_FILE_ERROR_FAILED;
      }
      LOG(WARNING) << "A stray file detected";
      InvalidateUsageCache(context, dest_origin, dest_type);
    }

    if (handle) {
      error = underlying_file_util()->CreateOrOpen(
        context, dest_path, file_flags, handle, &created);
      // If this succeeds, we must close handle on any subsequent error.
    } else {
      DCHECK(!file_flags);  // file_flags is only used by CreateOrOpen.
      error = underlying_file_util()->EnsureFileExists(
          context, dest_path, &created);
    }
  }
  if (error != base::PLATFORM_FILE_OK)
    return error;

  if (!created) {
    NOTREACHED();
    if (handle) {
      DCHECK_NE(base::kInvalidPlatformFileValue, *handle);
      base::ClosePlatformFile(*handle);
      underlying_file_util()->DeleteFile(context, dest_path);
    }
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  dest_file_info->data_path = data_path;
  FileId file_id;
  if (!db->AddFileInfo(*dest_file_info, &file_id)) {
    if (handle) {
      DCHECK_NE(base::kInvalidPlatformFileValue, *handle);
      base::ClosePlatformFile(*handle);
    }
    underlying_file_util()->DeleteFile(context, dest_path);
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  TouchDirectory(db, dest_file_info->parent_id);

  return base::PLATFORM_FILE_OK;
}

FileSystemPath ObfuscatedFileUtil::GetUnderlyingPath(
    const FileSystemPath& path) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      path.origin(), path.type(), false);
  if (!db)
    return FileSystemPath();
  FileId file_id;
  if (!db->GetFileWithPath(path.internal_path(), &file_id))
    return FileSystemPath();
  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info) || file_info.is_directory()) {
    NOTREACHED();
    return FileSystemPath();  // Directories have no underlying path.
  }
  return DataPathToUnderlyingPath(path.origin(),
                                  path.type(),
                                  file_info.data_path);
}

FileSystemPath ObfuscatedFileUtil::DataPathToUnderlyingPath(
    const GURL& origin, FileSystemType type, const FilePath& data_path) {
  FilePath root = GetDirectoryForOriginAndType(origin, type, false);
  if (root.empty())
    return FileSystemPath(origin, type, root);
  return FileSystemPath(origin, type, root.Append(data_path));
}

FilePath ObfuscatedFileUtil::UnderlyingPathToDataPath(
    const FileSystemPath& underlying_path) {
  FilePath root = GetDirectoryForOriginAndType(
      underlying_path.origin(), underlying_path.type(), false);
  if (root.empty())
    return root;
  // This removes the root, including the trailing slash, leaving a relative
  // path.
  FilePath local_file_path = underlying_path.internal_path();
  return FilePath(local_file_path.value().substr(root.value().length() + 1));
}

// TODO: How to do the whole validation-without-creation thing?  We may not have
// quota even to create the database.  Ah, in that case don't even get here?
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

  FilePath path = GetDirectoryForOriginAndType(origin, type, create);
  if (path.empty())
    return NULL;
  if (!file_util::DirectoryExists(path)) {
    if (!file_util::CreateDirectory(path)) {
      LOG(WARNING) << "Failed to origin+type directory: " << path.value();
      return NULL;
    }
  }
  MarkUsed();
  FileSystemDirectoryDatabase* database = new FileSystemDirectoryDatabase(path);
  directories_[key] = database;
  return database;
}

FilePath ObfuscatedFileUtil::GetDirectoryForOrigin(
    const GURL& origin, bool create, base::PlatformFileError* error_code) {
  if (!InitOriginDatabase(create)) {
    if (error_code) {
      *error_code = create ?
          base::PLATFORM_FILE_ERROR_FAILED :
          base::PLATFORM_FILE_ERROR_NOT_FOUND;
    }
    return FilePath();
  }
  FilePath directory_name;
  std::string id = GetOriginIdentifierFromURL(origin);

  bool exists_in_db = origin_database_->HasOriginPath(id);
  if (!exists_in_db && !create) {
    if (error_code)
      *error_code = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    return FilePath();
  }
  if (!origin_database_->GetPathForOrigin(id, &directory_name)) {
    if (error_code)
      *error_code = base::PLATFORM_FILE_ERROR_FAILED;
    return FilePath();
  }

  FilePath path = file_system_directory_.Append(directory_name);
  bool exists_in_fs = file_util::DirectoryExists(path);
  if (!exists_in_db && exists_in_fs) {
    if (!file_util::Delete(path, true)) {
      if (error_code)
        *error_code = base::PLATFORM_FILE_ERROR_FAILED;
      return FilePath();
    }
    exists_in_fs = false;
  }

  if (!exists_in_fs) {
    if (!create || !file_util::CreateDirectory(path)) {
      if (error_code)
        *error_code = create ?
            base::PLATFORM_FILE_ERROR_FAILED :
            base::PLATFORM_FILE_ERROR_NOT_FOUND;
      return FilePath();
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

PlatformFileError ObfuscatedFileUtil::GenerateNewUnderlyingPath(
    FileSystemDirectoryDatabase* db,
    FileSystemOperationContext* context,
    const GURL& origin,
    FileSystemType type,
    FileSystemPath* underlying_path) {
  DCHECK(underlying_path);
  int64 number;
  if (!db || !db->GetNextInteger(&number))
    return base::PLATFORM_FILE_ERROR_FAILED;

  // We use the third- and fourth-to-last digits as the directory.
  int64 directory_number = number % 10000 / 100;
  // TODO(ericu): local_file_path is an OS path;
  // underlying_file_util_ isn't guaranteed to understand OS paths.
  FilePath local_file_path = GetDirectoryForOriginAndType(origin, type, false);
  if (local_file_path.empty())
    return base::PLATFORM_FILE_ERROR_FAILED;

  local_file_path = local_file_path.AppendASCII(
      StringPrintf("%02" PRId64, directory_number));

  PlatformFileError error = underlying_file_util()->CreateDirectory(
      context, FileSystemPath(origin, type, local_file_path),
      false /* exclusive */, false /* recursive */);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  local_file_path = local_file_path.AppendASCII(
      StringPrintf("%08" PRId64, number));

  *underlying_path = FileSystemPath(origin, type, local_file_path);
  return base::PLATFORM_FILE_OK;
}

}  // namespace fileapi
