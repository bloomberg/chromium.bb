// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/obfuscated_file_system_file_util.h"

#include <queue>

#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "base/stl_util-inl.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/quota_file_util.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"

// TODO(ericu): Every instance of FileSystemFileUtil in this file should switch
// to QuotaFileUtil as soon as I sort out FileSystemPathManager's and
// SandboxMountPointProvider's lookups of the root path for a filesystem.
namespace {

const int64 kFlushDelaySeconds = 10 * 60;  // 10 minutes

const char kOriginDatabaseName[] = "Origins";
const char kDirectoryDatabaseName[] = "Paths";

void InitFileInfo(
    fileapi::FileSystemDirectoryDatabase::FileInfo* file_info,
    fileapi::FileSystemDirectoryDatabase::FileId parent_id,
    const FilePath::StringType& file_name) {
  DCHECK(file_info);
  file_info->parent_id = parent_id;
  file_info->name = file_name;
}

const FilePath::CharType kLegacyDataDirectory[] = FILE_PATH_LITERAL("Legacy");

const FilePath::CharType kTemporaryDirectoryName[] = FILE_PATH_LITERAL("t");
const FilePath::CharType kPersistentDirectoryName[] = FILE_PATH_LITERAL("p");

}  // namespace

namespace fileapi {

using base::PlatformFile;
using base::PlatformFileError;

ObfuscatedFileSystemFileUtil::ObfuscatedFileSystemFileUtil(
    const FilePath& file_system_directory)
    : file_system_directory_(file_system_directory) {
}

ObfuscatedFileSystemFileUtil::~ObfuscatedFileSystemFileUtil() {
  DropDatabases();
}

PlatformFileError ObfuscatedFileSystemFileUtil::CreateOrOpen(
    FileSystemOperationContext* context,
    const FilePath& virtual_path, int file_flags,
    PlatformFile* file_handle, bool* created) {
  DCHECK(!(file_flags & (base::PLATFORM_FILE_DELETE_ON_CLOSE |
        base::PLATFORM_FILE_HIDDEN | base::PLATFORM_FILE_EXCLUSIVE_READ |
        base::PLATFORM_FILE_EXCLUSIVE_WRITE)));
  FileSystemDirectoryDatabase* db =
      GetDirectoryDatabase(context->src_origin_url(), context->src_type());
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;
  FileId file_id;
  if (!db->GetFileWithPath(virtual_path, &file_id)) {
    // The file doesn't exist.
    if (!(file_flags & (base::PLATFORM_FILE_CREATE |
        base::PLATFORM_FILE_CREATE_ALWAYS)))
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    FileId parent_id;
    if (!db->GetFileWithPath(virtual_path.DirName(), &parent_id))
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    FileInfo file_info;
    InitFileInfo(&file_info, parent_id, virtual_path.BaseName().value());
    PlatformFileError error = CreateFile(
        context, context->src_origin_url(), context->src_type(), FilePath(),
        &file_info, file_flags, file_handle);
    if (created && base::PLATFORM_FILE_OK == error)
      *created = true;
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
  FilePath data_path = DataPathToLocalPath(context->src_origin_url(),
    context->src_type(), file_info.data_path);
  return FileSystemFileUtil::GetInstance()->CreateOrOpen(
      context, data_path, file_flags, file_handle, created);
}

PlatformFileError ObfuscatedFileSystemFileUtil::EnsureFileExists(
    FileSystemOperationContext* context,
    const FilePath& virtual_path,
    bool* created) {
  FileSystemDirectoryDatabase* db =
      GetDirectoryDatabase(context->src_origin_url(), context->src_type());
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;
  FileId file_id;
  if (db->GetFileWithPath(virtual_path, &file_id)) {
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
  if (!db->GetFileWithPath(virtual_path.DirName(), &parent_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  FileInfo file_info;
  InitFileInfo(&file_info, parent_id, virtual_path.BaseName().value());
  PlatformFileError error = CreateFile(context, context->src_origin_url(),
      context->src_type(), FilePath(), &file_info, 0, NULL);
  if (created && base::PLATFORM_FILE_OK == error)
    *created = true;
  return error;
}

PlatformFileError ObfuscatedFileSystemFileUtil::GetLocalFilePath(
    FileSystemOperationContext* context,
    const FilePath& virtual_path,
    FilePath* local_path) {
  FilePath path =
      GetLocalPath(context->src_origin_url(), context->src_type(),
                   virtual_path);
  if (path.empty())
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  *local_path = path;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileSystemFileUtil::GetFileInfo(
    FileSystemOperationContext* context,
    const FilePath& virtual_path,
    base::PlatformFileInfo* file_info,
    FilePath* platform_file_path) {
  FileSystemDirectoryDatabase* db =
      GetDirectoryDatabase(context->src_origin_url(), context->src_type());
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;
  FileId file_id;
  if (!db->GetFileWithPath(virtual_path, &file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileInfo local_info;
  if (!db->GetFileInfo(file_id, &local_info)) {
    NOTREACHED();
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  if (local_info.is_directory()) {
    file_info->is_directory = true;
    file_info->is_symbolic_link = false;
    file_info->last_modified = local_info.modification_time;
    *platform_file_path = FilePath();
    // We don't fill in ctime or atime.
    return base::PLATFORM_FILE_OK;
  }
  if (local_info.data_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  FilePath data_path = DataPathToLocalPath(context->src_origin_url(),
    context->src_type(), local_info.data_path);
  return FileSystemFileUtil::GetInstance()->GetFileInfo(
      context, data_path, file_info, platform_file_path);
}

PlatformFileError ObfuscatedFileSystemFileUtil::ReadDirectory(
    FileSystemOperationContext* context,
    const FilePath& virtual_path,
    std::vector<base::FileUtilProxy::Entry>* entries) {
  // TODO(kkanetkar): Implement directory read in multiple chunks.
  FileSystemDirectoryDatabase* db =
      GetDirectoryDatabase(context->src_origin_url(), context->src_type());
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;
  FileId file_id;
  if (!db->GetFileWithPath(virtual_path, &file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info)) {
    DCHECK(!file_id);
    // It's the root directory and the database hasn't been initialized yet.
    entries->clear();
    return base::PLATFORM_FILE_OK;
  }
  if (!file_info.is_directory())
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  std::vector<FileId> children;
  if (!db->ListChildren(file_id, &children)) {
    NOTREACHED();
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  std::vector<FileId>::iterator iter;
  for (iter = children.begin(); iter != children.end(); ++iter) {
    if (!db->GetFileInfo(*iter, &file_info)) {
      NOTREACHED();
      return base::PLATFORM_FILE_ERROR_FAILED;
    }
    base::FileUtilProxy::Entry entry;
    entry.name = file_info.name;
    entry.is_directory = file_info.is_directory();
    entries->push_back(entry);
  }
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileSystemFileUtil::CreateDirectory(
    FileSystemOperationContext* context,
    const FilePath& virtual_path,
    bool exclusive,
    bool recursive) {
  FileSystemDirectoryDatabase* db =
      GetDirectoryDatabase(context->src_origin_url(), context->src_type());
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;
  FileId file_id;
  if (db->GetFileWithPath(virtual_path, &file_id)) {
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
  virtual_path.GetComponents(&components);
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
  for (; index < components.size(); ++index) {
    FileInfo file_info;
    file_info.name = components[index];
    if (file_info.name == FILE_PATH_LITERAL("/"))
      continue;
    file_info.modification_time = base::Time::Now();
    file_info.parent_id = parent_id;
    if (!db->AddFileInfo(file_info, &parent_id)) {
      NOTREACHED();
      return base::PLATFORM_FILE_ERROR_FAILED;
    }
  }
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileSystemFileUtil::CopyOrMoveFile(
    FileSystemOperationContext* context,
    const FilePath& src_file_path,
    const FilePath& dest_file_path,
    bool copy) {
  // TODO(ericu): Handle multi-db move+copy, where src and dest aren't in the
  // same database.  Currently we'll just fail badly.  This may get handled from
  // higher-level code, though, and as we don't have cross-filesystem
  // transactions that's not any less efficient than doing it here.
  FileSystemDirectoryDatabase* db =
      GetDirectoryDatabase(context->src_origin_url(), context->src_type());
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;
  FileId src_file_id;
  if (!db->GetFileWithPath(src_file_path, &src_file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileId dest_file_id;
  bool overwrite = db->GetFileWithPath(dest_file_path, &dest_file_id);
  FileInfo src_file_info;
  FileInfo dest_file_info;
  if (!db->GetFileInfo(src_file_id, &src_file_info) ||
      src_file_info.is_directory()) {
    NOTREACHED();
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  if (overwrite) {
    if (!db->GetFileInfo(dest_file_id, &dest_file_info) ||
        dest_file_info.is_directory()) {
      NOTREACHED();
      return base::PLATFORM_FILE_ERROR_FAILED;
    }
  }
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
  if (copy) {
    FilePath src_data_path = DataPathToLocalPath(context->src_origin_url(),
      context->src_type(), src_file_info.data_path);
    if (overwrite) {
      FilePath dest_data_path = DataPathToLocalPath(context->src_origin_url(),
        context->src_type(), dest_file_info.data_path);
      return FileSystemFileUtil::GetInstance()->CopyOrMoveFile(context,
          src_data_path, dest_data_path, copy);
    } else {
      FileId dest_parent_id;
      if (!db->GetFileWithPath(dest_file_path.DirName(), &dest_parent_id)) {
        NOTREACHED();  // We shouldn't be called in this case.
        return base::PLATFORM_FILE_ERROR_NOT_FOUND;
      }
      InitFileInfo(&dest_file_info, dest_parent_id,
          dest_file_path.BaseName().value());
      return CreateFile(context, context->dest_origin_url(),
          context->dest_type(), src_data_path, &dest_file_info, 0,
          NULL);
    }
  } else {  // It's a move.
    if (overwrite) {
      if (!db->OverwritingMoveFile(src_file_id, dest_file_id))
        return base::PLATFORM_FILE_ERROR_FAILED;
      FilePath dest_data_path = DataPathToLocalPath(context->src_origin_url(),
        context->src_type(), dest_file_info.data_path);
      if (base::PLATFORM_FILE_OK !=
          FileSystemFileUtil::GetInstance()->DeleteFile(
              context, dest_data_path))
        LOG(WARNING) << "Leaked a backing file.";
      return base::PLATFORM_FILE_OK;
    } else {
      FileId dest_parent_id;
      if (!db->GetFileWithPath(dest_file_path.DirName(), &dest_parent_id)) {
        NOTREACHED();
        return base::PLATFORM_FILE_ERROR_FAILED;
      }
      src_file_info.parent_id = dest_parent_id;
      src_file_info.name = dest_file_path.BaseName().value();
      if (!db->UpdateFileInfo(src_file_id, src_file_info))
        return base::PLATFORM_FILE_ERROR_FAILED;
      return base::PLATFORM_FILE_OK;
    }
  }
  NOTREACHED();
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError ObfuscatedFileSystemFileUtil::DeleteFile(
    FileSystemOperationContext* context,
    const FilePath& virtual_path) {
  FileSystemDirectoryDatabase* db =
      GetDirectoryDatabase(context->src_origin_url(), context->src_type());
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;
  FileId file_id;
  if (!db->GetFileWithPath(virtual_path, &file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info) || file_info.is_directory()) {
    NOTREACHED();
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  if (!db->RemoveFileInfo(file_id)) {
    NOTREACHED();
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  FilePath data_path = DataPathToLocalPath(context->src_origin_url(),
    context->src_type(), file_info.data_path);
  if (base::PLATFORM_FILE_OK !=
      FileSystemFileUtil::GetInstance()->DeleteFile(context, data_path))
    LOG(WARNING) << "Leaked a backing file.";
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileSystemFileUtil::DeleteSingleDirectory(
    FileSystemOperationContext* context,
    const FilePath& virtual_path) {
  FileSystemDirectoryDatabase* db =
      GetDirectoryDatabase(context->src_origin_url(), context->src_type());
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;
  FileId file_id;
  if (!db->GetFileWithPath(virtual_path, &file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info) || !file_info.is_directory()) {
    NOTREACHED();
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  if (!db->RemoveFileInfo(file_id))
    return base::PLATFORM_FILE_ERROR_NOT_EMPTY;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileSystemFileUtil::Touch(
    FileSystemOperationContext* context,
    const FilePath& virtual_path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  FileSystemDirectoryDatabase* db =
      GetDirectoryDatabase(context->src_origin_url(), context->src_type());
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;
  FileId file_id;
  if (db->GetFileWithPath(virtual_path, &file_id)) {
    FileInfo file_info;
    if (!db->GetFileInfo(file_id, &file_info)) {
      NOTREACHED();
      return base::PLATFORM_FILE_ERROR_FAILED;
    }
    if (file_info.is_directory()) {
      file_info.modification_time = last_modified_time;
      if (!db->UpdateFileInfo(file_id, file_info))
        return base::PLATFORM_FILE_ERROR_FAILED;
      return base::PLATFORM_FILE_OK;
    }
    FilePath data_path = DataPathToLocalPath(context->src_origin_url(),
      context->src_type(), file_info.data_path);
    return FileSystemFileUtil::GetInstance()->Touch(
        context, data_path, last_access_time, last_modified_time);
  }
  FileId parent_id;
  if (!db->GetFileWithPath(virtual_path.DirName(), &parent_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  FileInfo file_info;
  InitFileInfo(&file_info, parent_id, virtual_path.BaseName().value());
  // In the event of a sporadic underlying failure, we might create a new file,
  // but fail to update its mtime + atime.
  PlatformFileError error = CreateFile(context, context->src_origin_url(),
      context->src_type(), FilePath(), &file_info, 0, NULL);
  if (base::PLATFORM_FILE_OK != error)
    return error;

  FilePath data_path = DataPathToLocalPath(context->src_origin_url(),
    context->src_type(), file_info.data_path);
  return FileSystemFileUtil::GetInstance()->Touch(context, data_path,
    last_access_time, last_modified_time);
}

PlatformFileError ObfuscatedFileSystemFileUtil::Truncate(
    FileSystemOperationContext* context,
    const FilePath& virtual_path,
    int64 length) {
  FilePath local_path =
      GetLocalPath(context->src_origin_url(), context->src_type(),
          virtual_path);
  if (local_path.empty())
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  return FileSystemFileUtil::GetInstance()->Truncate(
      context, local_path, length);
}

bool ObfuscatedFileSystemFileUtil::PathExists(
    FileSystemOperationContext* context,
    const FilePath& virtual_path) {
  FileSystemDirectoryDatabase* db =
      GetDirectoryDatabase(context->src_origin_url(), context->src_type());
  if (!db)
    return false;
  FileId file_id;
  return db->GetFileWithPath(virtual_path, &file_id);
}

bool ObfuscatedFileSystemFileUtil::DirectoryExists(
    FileSystemOperationContext* context,
    const FilePath& virtual_path) {
  FileSystemDirectoryDatabase* db =
      GetDirectoryDatabase(context->src_origin_url(), context->src_type());
  if (!db)
    return false;
  FileId file_id;
  if (!db->GetFileWithPath(virtual_path, &file_id))
    return false;
  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info)) {
    NOTREACHED();
    return false;
  }
  return file_info.is_directory();
}

bool ObfuscatedFileSystemFileUtil::IsDirectoryEmpty(
    FileSystemOperationContext* context,
    const FilePath& virtual_path) {
  FileSystemDirectoryDatabase* db =
      GetDirectoryDatabase(context->src_origin_url(), context->src_type());
  if (!db)
    return false;
  FileId file_id;
  if (!db->GetFileWithPath(virtual_path, &file_id))
    return true;  // Not a great answer, but it's what others do.
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

class ObfuscatedFileSystemFileEnumerator
    : public FileSystemFileUtil::AbstractFileEnumerator {
 public:
  ObfuscatedFileSystemFileEnumerator(
      FileSystemDirectoryDatabase* db, const FilePath& virtual_root_path)
      : db_(db) {
    FileId file_id;
    FileInfo file_info;
    if (!db_->GetFileWithPath(virtual_root_path, &file_id))
      return;
    if (!db_->GetFileInfo(file_id, &file_info))
      return;
    if (!file_info.is_directory())
      return;
    FileRecord record = { file_id, file_info, virtual_root_path };
    display_queue_.push(record);
    Next();  // Enumerators don't include the directory itself.
  }

  ~ObfuscatedFileSystemFileEnumerator() {}

  virtual FilePath Next() {
    ProcessRecurseQueue();
    if (display_queue_.empty())
      return FilePath();
    current_ = display_queue_.front();
    display_queue_.pop();
    if (current_.file_info.is_directory())
      recurse_queue_.push(current_);
    return current_.file_path;
  }

  virtual bool IsDirectory() {
    return current_.file_info.is_directory();
  }

 private:
  typedef FileSystemDirectoryDatabase::FileId FileId;
  typedef FileSystemDirectoryDatabase::FileInfo FileInfo;

  struct FileRecord {
    FileId file_id;
    FileInfo file_info;
    FilePath file_path;
  };

  void ProcessRecurseQueue() {
    while (display_queue_.empty() && !recurse_queue_.empty()) {
      FileRecord directory = recurse_queue_.front();
      std::vector<FileId> children;
      recurse_queue_.pop();
      if (!db_->ListChildren(directory.file_id, &children))
        return;
      std::vector<FileId>::iterator iter;
      for (iter = children.begin(); iter != children.end(); ++iter) {
        FileRecord child;
        child.file_id = *iter;
        if (!db_->GetFileInfo(child.file_id, &child.file_info))
          return;
        child.file_path = directory.file_path.Append(child.file_info.name);
        display_queue_.push(child);
      }
    }
  }

  std::queue<FileRecord> display_queue_;
  std::queue<FileRecord> recurse_queue_;
  FileRecord current_;
  FileSystemDirectoryDatabase* db_;
};

FileSystemFileUtil::AbstractFileEnumerator*
ObfuscatedFileSystemFileUtil::CreateFileEnumerator(
    FileSystemOperationContext* context,
    const FilePath& root_path) {
  FileSystemDirectoryDatabase* db =
      GetDirectoryDatabase(context->src_origin_url(), context->src_type());
  if (!db)
    return new FileSystemFileUtil::EmptyFileEnumerator();
  return new ObfuscatedFileSystemFileEnumerator(db, root_path);
}

PlatformFileError ObfuscatedFileSystemFileUtil::CreateFile(
    FileSystemOperationContext* context,
    const GURL& origin_url, FileSystemType type, const FilePath& source_path,
    FileInfo* file_info, int file_flags, PlatformFile* handle) {
  if (handle)
    *handle = base::kInvalidPlatformFileValue;
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(origin_url, type);
  int64 number;
  if (!db || !db->GetNextInteger(&number))
    return base::PLATFORM_FILE_ERROR_FAILED;
  // We use the third- and fourth-to-last digits as the directory.
  int64 directory_number = number % 10000 / 100;
  FilePath path =
      GetDirectoryForOriginAndType(origin_url, type, false);
  if (path.empty())
    return base::PLATFORM_FILE_ERROR_FAILED;

  path = path.AppendASCII(StringPrintf("%02" PRIu64, directory_number));
  PlatformFileError error;
  error = FileSystemFileUtil::GetInstance()->CreateDirectory(
      context, path, false /* exclusive */, false /* recursive */);
  if (base::PLATFORM_FILE_OK != error)
    return error;
  path = path.AppendASCII(StringPrintf("%08" PRIu64, number));
  FilePath data_path = LocalPathToDataPath(origin_url, type, path);
  if (data_path.empty())
    return base::PLATFORM_FILE_ERROR_FAILED;
  bool created = false;
  if (!source_path.empty()) {
    DCHECK(!file_flags);
    DCHECK(!handle);
    error = FileSystemFileUtil::GetInstance()->CopyOrMoveFile(
      context, source_path, path, true /* copy */);
    created = true;
  } else {
    if (handle) {
      error = FileSystemFileUtil::GetInstance()->CreateOrOpen(
        context, path, file_flags, handle, &created);
      // If this succeeds, we must close handle on any subsequent error.
    } else {
      DCHECK(!file_flags);  // file_flags is only used by CreateOrOpen.
      error = FileSystemFileUtil::GetInstance()->EnsureFileExists(
          context, path, &created);
    }
  }
  if (error != base::PLATFORM_FILE_OK)
    return error;

  if (!created) {
    NOTREACHED();
    if (handle) {
      base::ClosePlatformFile(*handle);
      FileSystemFileUtil::GetInstance()->DeleteFile(context, path);
    }
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  file_info->data_path = data_path;
  FileId file_id;
  if (!db->AddFileInfo(*file_info, &file_id)) {
    if (handle)
      base::ClosePlatformFile(*handle);
    FileSystemFileUtil::GetInstance()->DeleteFile(context, path);
    return base::PLATFORM_FILE_ERROR_FAILED;
  }

  return base::PLATFORM_FILE_OK;
}

FilePath ObfuscatedFileSystemFileUtil::GetLocalPath(
    const GURL& origin_url,
    FileSystemType type,
    const FilePath& virtual_path) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(origin_url, type);
  if (!db)
    return FilePath();
  FileId file_id;
  if (!db->GetFileWithPath(virtual_path, &file_id))
    return FilePath();
  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info) || file_info.is_directory()) {
    NOTREACHED();
    return FilePath();  // Directories have no local path.
  }
  return DataPathToLocalPath(origin_url, type, file_info.data_path);
}

FilePath ObfuscatedFileSystemFileUtil::GetDirectoryForOriginAndType(
    const GURL& origin, FileSystemType type, bool create) {
  FilePath origin_dir = GetDirectoryForOrigin(origin, create);
  if (origin_dir.empty())
    return FilePath();
  FilePath::StringType type_string = GetDirectoryNameForType(type);
  if (type_string.empty()) {
    LOG(WARNING) << "Unknown filesystem type requested:" << type;
    return FilePath();
  }
  return origin_dir.Append(type_string);
}

FilePath ObfuscatedFileSystemFileUtil::GetDirectoryForOrigin(
    const GURL& origin, bool create) {
  if (!origin_database_.get()) {
    if (!create && !file_util::DirectoryExists(file_system_directory_)) {
      return FilePath();
    }
    if (!file_util::CreateDirectory(file_system_directory_)) {
      LOG(WARNING) << "Failed to create FileSystem directory: " <<
          file_system_directory_.value();
      return FilePath();
    }
    origin_database_.reset(
        new FileSystemOriginDatabase(
            file_system_directory_.AppendASCII(kOriginDatabaseName)));
  }
  FilePath directory_name;
  // TODO(ericu): This should probably be using GetOriginIdentifierFromURL from
  // sandbox_mount_point_provider.cc, instead of just using origin.spec().
  if (!create && !origin_database_->HasOriginPath(origin.spec()))
    return FilePath();
  if (!origin_database_->GetPathForOrigin(origin.spec(), &directory_name))
    return FilePath();
  return file_system_directory_.Append(directory_name);
}

bool ObfuscatedFileSystemFileUtil::MigrateFromOldSandbox(
    const GURL& origin_url, FileSystemType type, const FilePath& src_root) {
  if (!DestroyDirectoryDatabase(origin_url, type))
    return false;
  FilePath dest_root = GetDirectoryForOriginAndType(origin_url, type, true);
  if (dest_root.empty())
    return false;
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(origin_url, type);
  if (!db)
    return false;

  file_util::FileEnumerator file_enum(src_root, true,
      static_cast<file_util::FileEnumerator::FILE_TYPE>(
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
    file_info.name = src_full_path.BaseName().value();
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
  return file_util::Move(src_root, legacy_dest_dir);
}

FilePath::StringType ObfuscatedFileSystemFileUtil::GetDirectoryNameForType(
    FileSystemType type) const {
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

FilePath ObfuscatedFileSystemFileUtil::DataPathToLocalPath(
    const GURL& origin, FileSystemType type, const FilePath& data_path) {
  FilePath root = GetDirectoryForOriginAndType(origin, type, false);
  if (root.empty())
    return root;
  return root.Append(data_path);
}

FilePath ObfuscatedFileSystemFileUtil::LocalPathToDataPath(
    const GURL& origin, FileSystemType type, const FilePath& local_path) {
  FilePath root = GetDirectoryForOriginAndType(origin, type, false);
  if (root.empty())
    return root;
  // This removes the root, including the trailing slash, leaving a relative
  // path.
  return FilePath(local_path.value().substr(root.value().length() + 1));
}

// TODO: How to do the whole validation-without-creation thing?  We may not have
// quota even to create the database.  Ah, in that case don't even get here?
// Still doesn't answer the quota issue, though.
FileSystemDirectoryDatabase* ObfuscatedFileSystemFileUtil::GetDirectoryDatabase(
    const GURL& origin, FileSystemType type) {

  MarkUsed();
  std::string type_string =
      FileSystemPathManager::GetFileSystemTypeString(type);
  if (type_string.empty()) {
    LOG(WARNING) << "Unknown filesystem type requested:" << type;
    return NULL;
  }
  // TODO(ericu): This should probably be using GetOriginIdentifierFromURL from
  // sandbox_mount_point_provider.cc, instead of just using origin.spec().
  std::string key = origin.spec() + type_string;
  DirectoryMap::iterator iter = directories_.find(key);
  if (iter != directories_.end())
    return iter->second;

  FilePath path = GetDirectoryForOriginAndType(origin, type, true);
  if (path.empty())
    return NULL;
  if (!file_util::DirectoryExists(path)) {
    if (!file_util::CreateDirectory(path)) {
      LOG(WARNING) << "Failed to origin+type directory: " << path.value();
      return NULL;
    }
  }
  path = path.AppendASCII(kDirectoryDatabaseName);
  FileSystemDirectoryDatabase* database = new FileSystemDirectoryDatabase(path);
  directories_[key] = database;
  return database;
}

void ObfuscatedFileSystemFileUtil::MarkUsed() {
  if (timer_.IsRunning())
    timer_.Reset();
  else
    timer_.Start(base::TimeDelta::FromSeconds(kFlushDelaySeconds), this,
      &ObfuscatedFileSystemFileUtil::DropDatabases);
}

void ObfuscatedFileSystemFileUtil::DropDatabases() {
  origin_database_.reset();
  STLDeleteContainerPairSecondPointers(
      directories_.begin(), directories_.end());
  directories_.clear();
}

bool ObfuscatedFileSystemFileUtil::DestroyDirectoryDatabase(
    const GURL& origin, FileSystemType type) {
  std::string type_string =
      FileSystemPathManager::GetFileSystemTypeString(type);
  if (type_string.empty()) {
    LOG(WARNING) << "Unknown filesystem type requested:" << type;
    return true;
  }
  // TODO(ericu): This should probably be using GetOriginIdentifierFromURL from
  // sandbox_mount_point_provider.cc, instead of just using origin.spec().
  std::string key = origin.spec() + type_string;
  DirectoryMap::iterator iter = directories_.find(key);
  if (iter != directories_.end())
    directories_.erase(iter);

  FilePath path = GetDirectoryForOriginAndType(origin, type, false);
  if (path.empty())
    return true;
  if (!file_util::DirectoryExists(path))
    return true;
  path = path.AppendASCII(kDirectoryDatabaseName);
  return FileSystemDirectoryDatabase::DestroyDatabase(path);
}

}  // namespace fileapi
