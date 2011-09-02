// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/obfuscated_file_system_file_util.h"

#include <queue>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_quota_util.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/quota/quota_manager.h"

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

bool IsRootDirectory(const FilePath& virtual_path) {
  return (virtual_path.empty() ||
          virtual_path.value() == FILE_PATH_LITERAL("/"));
}

// Costs computed as per crbug.com/86114, based on the LevelDB implementation of
// path storage under Linux.  It's not clear if that will differ on Windows, on
// which FilePath uses wide chars [since they're converted to UTF-8 for storage
// anyway], but as long as the cost is high enough that one can't cheat on quota
// by storing data in paths, it doesn't need to be all that accurate.
const int64 kPathCreationQuotaCost = 146;  // Bytes per inode, basically.
const int64 kPathByteQuotaCost = 2;  // Bytes per byte of path length in UTF-8.

int64 GetPathQuotaUsage(
    int growth_in_number_of_paths,
    int64 growth_in_bytes_of_path_length) {
  return growth_in_number_of_paths * kPathCreationQuotaCost +
      growth_in_bytes_of_path_length * kPathByteQuotaCost;
}

bool AllocateQuotaForPath(
    fileapi::FileSystemOperationContext* context,
    int growth_in_number_of_paths,
    int64 growth_in_bytes_of_path_length) {
  int64 growth = GetPathQuotaUsage(growth_in_number_of_paths,
      growth_in_bytes_of_path_length);
  int64 new_quota = context->allowed_bytes_growth() - growth;

  if (growth <= 0 || new_quota >= 0) {
    context->set_allowed_bytes_growth(new_quota);
    return true;
  }
  return false;
}

void UpdatePathQuotaUsage(
    fileapi::FileSystemOperationContext* context,
    const GURL& origin_url,
    fileapi::FileSystemType type,
    int growth_in_number_of_paths,  // -1, 0, or 1
    int64 growth_in_bytes_of_path_length) {
  int64 growth = GetPathQuotaUsage(growth_in_number_of_paths,
      growth_in_bytes_of_path_length);
  fileapi::FileSystemQuotaUtil* quota_util =
      context->file_system_context()->GetQuotaUtil(type);
  quota::QuotaManagerProxy* quota_manager_proxy =
      context->file_system_context()->quota_manager_proxy();
  quota_util->UpdateOriginUsageOnFileThread(quota_manager_proxy, origin_url,
      type, growth);
}

const FilePath::CharType kLegacyDataDirectory[] = FILE_PATH_LITERAL("Legacy");

const FilePath::CharType kTemporaryDirectoryName[] = FILE_PATH_LITERAL("t");
const FilePath::CharType kPersistentDirectoryName[] = FILE_PATH_LITERAL("p");

}  // namespace

namespace fileapi {

using base::PlatformFile;
using base::PlatformFileError;

ObfuscatedFileSystemFileUtil::ObfuscatedFileSystemFileUtil(
    const FilePath& file_system_directory,
    FileSystemFileUtil* underlying_file_util)
    : file_system_directory_(file_system_directory),
      underlying_file_util_(underlying_file_util) {
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
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      context->src_origin_url(), context->src_type(), true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;
  FileId file_id;
  if (!db->GetFileWithPath(virtual_path, &file_id)) {
    // The file doesn't exist.
    if (!(file_flags & (base::PLATFORM_FILE_CREATE |
        base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_OPEN_ALWAYS)))
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    FileId parent_id;
    if (!db->GetFileWithPath(virtual_path.DirName(), &parent_id))
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    FileInfo file_info;
    InitFileInfo(&file_info, parent_id, virtual_path.BaseName().value());
    if (!AllocateQuotaForPath(context, 1, file_info.name.size()))
      return base::PLATFORM_FILE_ERROR_NO_SPACE;
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
  FilePath local_path = DataPathToLocalPath(context->src_origin_url(),
    context->src_type(), file_info.data_path);
  base::PlatformFileError error = underlying_file_util_->CreateOrOpen(
      context, local_path, file_flags, file_handle, created);
  if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND) {
    // TODO(tzik): Also invalidate on-memory usage cache in UsageTracker.
    // TODO(tzik): Delete database entry after ensuring the file lost.
    context->file_system_context()->GetQuotaUtil(context->src_type())->
        InvalidateUsageCache(context->src_origin_url(),
                             context->src_type());
    LOG(WARNING) << "Lost a backing file.";
    error = base::PLATFORM_FILE_ERROR_FAILED;
  }
  return error;
}

PlatformFileError ObfuscatedFileSystemFileUtil::EnsureFileExists(
    FileSystemOperationContext* context,
    const FilePath& virtual_path,
    bool* created) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      context->src_origin_url(), context->src_type(), true);
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
  if (!AllocateQuotaForPath(context, 1, file_info.name.size()))
    return base::PLATFORM_FILE_ERROR_NO_SPACE;
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
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      context->src_origin_url(), context->src_type(), false);
  if (!db)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileId file_id;
  if (!db->GetFileWithPath(virtual_path, &file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileInfo local_info;
  return GetFileInfoInternal(db, context, file_id,
                             &local_info, file_info, platform_file_path);
}

PlatformFileError ObfuscatedFileSystemFileUtil::ReadDirectory(
    FileSystemOperationContext* context,
    const FilePath& virtual_path,
    std::vector<base::FileUtilProxy::Entry>* entries) {
  // TODO(kkanetkar): Implement directory read in multiple chunks.
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      context->src_origin_url(), context->src_type(), false);
  if (!db) {
    if (IsRootDirectory(virtual_path)) {
      // It's the root directory and the database hasn't been initialized yet.
      entries->clear();
      return base::PLATFORM_FILE_OK;
    }
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  }
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
    base::PlatformFileInfo platform_file_info;
    FilePath file_path;
    if (GetFileInfoInternal(db, context, *iter,
                            &file_info, &platform_file_info, &file_path) !=
        base::PLATFORM_FILE_OK) {
      NOTREACHED();
      return base::PLATFORM_FILE_ERROR_FAILED;
    }

    base::FileUtilProxy::Entry entry;
    entry.name = file_info.name;
    entry.is_directory = file_info.is_directory();
    entry.size = entry.is_directory ? 0 : platform_file_info.size;
    entry.last_modified_time = platform_file_info.last_modified;
    entries->push_back(entry);
  }
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileSystemFileUtil::CreateDirectory(
    FileSystemOperationContext* context,
    const FilePath& virtual_path,
    bool exclusive,
    bool recursive) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      context->src_origin_url(), context->src_type(), true);
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
    if (!AllocateQuotaForPath(context, 1, file_info.name.size()))
      return base::PLATFORM_FILE_ERROR_NO_SPACE;
    if (!db->AddFileInfo(file_info, &parent_id)) {
      NOTREACHED();
      return base::PLATFORM_FILE_ERROR_FAILED;
    }
    UpdatePathQuotaUsage(context, context->src_origin_url(),
      context->src_type(), 1, file_info.name.size());
  }
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileSystemFileUtil::CopyOrMoveFile(
    FileSystemOperationContext* context,
    const FilePath& src_file_path,
    const FilePath& dest_file_path,
    bool copy) {
  // Cross-filesystem copies and moves should be handled via CopyInForeignFile.
  DCHECK(context->src_origin_url() == context->dest_origin_url());
  DCHECK(context->src_type() == context->dest_type());

  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      context->src_origin_url(), context->src_type(), true);
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
    if (!underlying_file_util_->PathExists(context, src_data_path)) {
      // TODO(tzik): Also invalidate on-memory usage cache in UsageTracker.
      context->file_system_context()->GetQuotaUtil(context->src_type())->
          InvalidateUsageCache(context->src_origin_url(),
                               context->src_type());
      LOG(WARNING) << "Lost a backing file.";
      return base::PLATFORM_FILE_ERROR_FAILED;
    }

    if (overwrite) {
      FilePath dest_data_path = DataPathToLocalPath(context->src_origin_url(),
        context->src_type(), dest_file_info.data_path);
      return underlying_file_util_->CopyOrMoveFile(context,
          src_data_path, dest_data_path, copy);
    } else {
      FileId dest_parent_id;
      if (!db->GetFileWithPath(dest_file_path.DirName(), &dest_parent_id)) {
        NOTREACHED();  // We shouldn't be called in this case.
        return base::PLATFORM_FILE_ERROR_NOT_FOUND;
      }
      InitFileInfo(&dest_file_info, dest_parent_id,
          dest_file_path.BaseName().value());
      if (!AllocateQuotaForPath(context, 1, dest_file_info.name.size()))
        return base::PLATFORM_FILE_ERROR_NO_SPACE;
      return CreateFile(context, context->dest_origin_url(),
          context->dest_type(), src_data_path, &dest_file_info, 0,
          NULL);
    }
  } else {  // It's a move.
    if (overwrite) {
      AllocateQuotaForPath(context, -1,
          -static_cast<int64>(src_file_info.name.size()));
      if (!db->OverwritingMoveFile(src_file_id, dest_file_id))
        return base::PLATFORM_FILE_ERROR_FAILED;
      FilePath dest_data_path = DataPathToLocalPath(context->src_origin_url(),
        context->src_type(), dest_file_info.data_path);
      if (base::PLATFORM_FILE_OK !=
          underlying_file_util_->DeleteFile(context, dest_data_path))
        LOG(WARNING) << "Leaked a backing file.";
      UpdatePathQuotaUsage(context, context->src_origin_url(),
          context->src_type(), -1,
          -static_cast<int64>(src_file_info.name.size()));
      return base::PLATFORM_FILE_OK;
    } else {
      FileId dest_parent_id;
      if (!db->GetFileWithPath(dest_file_path.DirName(), &dest_parent_id)) {
        NOTREACHED();
        return base::PLATFORM_FILE_ERROR_FAILED;
      }
      if (!AllocateQuotaForPath(
          context, 0,
          static_cast<int64>(dest_file_path.BaseName().value().size())
              - static_cast<int64>(src_file_info.name.size())))
        return base::PLATFORM_FILE_ERROR_NO_SPACE;
      src_file_info.parent_id = dest_parent_id;
      src_file_info.name = dest_file_path.BaseName().value();
      if (!db->UpdateFileInfo(src_file_id, src_file_info))
        return base::PLATFORM_FILE_ERROR_FAILED;
      UpdatePathQuotaUsage(
          context, context->src_origin_url(), context->src_type(), 0,
          static_cast<int64>(dest_file_path.BaseName().value().size()) -
              static_cast<int64>(src_file_path.BaseName().value().size()));
      return base::PLATFORM_FILE_OK;
    }
  }
  NOTREACHED();
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError ObfuscatedFileSystemFileUtil::CopyInForeignFile(
    FileSystemOperationContext* context,
    const FilePath& src_file_path,
    const FilePath& dest_file_path) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      context->dest_origin_url(), context->dest_type(), true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;
  FileId dest_file_id;
  bool overwrite = db->GetFileWithPath(dest_file_path, &dest_file_id);
  FileInfo dest_file_info;
  if (overwrite) {
    if (!db->GetFileInfo(dest_file_id, &dest_file_info) ||
        dest_file_info.is_directory()) {
      NOTREACHED();
      return base::PLATFORM_FILE_ERROR_FAILED;
    }
    FilePath dest_data_path = DataPathToLocalPath(context->dest_origin_url(),
      context->dest_type(), dest_file_info.data_path);
    return underlying_file_util_->CopyOrMoveFile(context,
        src_file_path, dest_data_path, true /* copy */);
  } else {
    FileId dest_parent_id;
    if (!db->GetFileWithPath(dest_file_path.DirName(), &dest_parent_id)) {
      NOTREACHED();  // We shouldn't be called in this case.
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    }
    InitFileInfo(&dest_file_info, dest_parent_id,
        dest_file_path.BaseName().value());
    if (!AllocateQuotaForPath(context, 1, dest_file_info.name.size()))
      return base::PLATFORM_FILE_ERROR_NO_SPACE;
    return CreateFile(context, context->dest_origin_url(),
        context->dest_type(), src_file_path, &dest_file_info, 0, NULL);
  }
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError ObfuscatedFileSystemFileUtil::DeleteFile(
    FileSystemOperationContext* context,
    const FilePath& virtual_path) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      context->src_origin_url(), context->src_type(), true);
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
  AllocateQuotaForPath(context, -1, -static_cast<int64>(file_info.name.size()));
  UpdatePathQuotaUsage(context, context->src_origin_url(), context->src_type(),
      -1, -static_cast<int64>(file_info.name.size()));
  FilePath data_path = DataPathToLocalPath(context->src_origin_url(),
    context->src_type(), file_info.data_path);
  if (base::PLATFORM_FILE_OK !=
      underlying_file_util_->DeleteFile(context, data_path))
    LOG(WARNING) << "Leaked a backing file.";
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileSystemFileUtil::DeleteSingleDirectory(
    FileSystemOperationContext* context,
    const FilePath& virtual_path) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      context->src_origin_url(), context->src_type(), true);
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
  AllocateQuotaForPath(context, -1, -static_cast<int64>(file_info.name.size()));
  UpdatePathQuotaUsage(context, context->src_origin_url(), context->src_type(),
      -1, -static_cast<int64>(file_info.name.size()));
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileSystemFileUtil::Touch(
    FileSystemOperationContext* context,
    const FilePath& virtual_path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      context->src_origin_url(), context->src_type(), false);
  if (!db)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileId file_id;
  if (!db->GetFileWithPath(virtual_path, &file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

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
  return underlying_file_util_->Touch(
      context, data_path, last_access_time, last_modified_time);
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
  return underlying_file_util_->Truncate(
      context, local_path, length);
}

bool ObfuscatedFileSystemFileUtil::PathExists(
    FileSystemOperationContext* context,
    const FilePath& virtual_path) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      context->src_origin_url(), context->src_type(), false);
  if (!db)
    return false;
  FileId file_id;
  return db->GetFileWithPath(virtual_path, &file_id);
}

bool ObfuscatedFileSystemFileUtil::DirectoryExists(
    FileSystemOperationContext* context,
    const FilePath& virtual_path) {
  if (IsRootDirectory(virtual_path)) {
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
      context->src_origin_url(), context->src_type(), false);
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
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      context->src_origin_url(), context->src_type(), false);
  if (!db)
    return true;  // Not a great answer, but it's what others do.
  FileId file_id;
  if (!db->GetFileWithPath(virtual_path, &file_id))
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

class ObfuscatedFileSystemFileEnumerator
    : public FileSystemFileUtil::AbstractFileEnumerator {
 public:
  ObfuscatedFileSystemFileEnumerator(
      const FilePath& base_path,
      FileSystemDirectoryDatabase* db,
      FileSystemOperationContext* context,
      FileSystemFileUtil* underlying_file_util,
      const FilePath& virtual_root_path)
      : base_path_(base_path),
        db_(db),
        context_(context),
        underlying_file_util_(underlying_file_util) {
    DCHECK(db_);
    DCHECK(context_);
    DCHECK(underlying_file_util_);

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

  virtual FilePath Next() OVERRIDE {
    ProcessRecurseQueue();
    if (display_queue_.empty())
      return FilePath();
    current_ = display_queue_.front();
    display_queue_.pop();
    if (current_.file_info.is_directory())
      recurse_queue_.push(current_);
    return current_.file_path;
  }

  virtual int64 Size() OVERRIDE {
    if (IsDirectory())
      return 0;

    base::PlatformFileInfo file_info;
    FilePath platform_file_path;

    FilePath local_path = base_path_.Append(current_.file_info.data_path);
    base::PlatformFileError error = underlying_file_util_->GetFileInfo(
        context_, local_path, &file_info, &platform_file_path);
    if (error != base::PLATFORM_FILE_OK) {
      LOG(WARNING) << "Lost a backing file.";
      return 0;
    }
    return file_info.size;
  }

  virtual bool IsDirectory() OVERRIDE {
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
  FilePath base_path_;
  FileSystemDirectoryDatabase* db_;
  FileSystemOperationContext* context_;
  FileSystemFileUtil* underlying_file_util_;
};

class ObfuscatedFileSystemOriginEnumerator
    : public ObfuscatedFileSystemFileUtil::AbstractOriginEnumerator {
 public:
  typedef FileSystemOriginDatabase::OriginRecord OriginRecord;
  ObfuscatedFileSystemOriginEnumerator(
      FileSystemOriginDatabase* origin_database,
      const FilePath& base_path)
      : base_path_(base_path) {
    if (origin_database)
      origin_database->ListAllOrigins(&origins_);
  }

  ~ObfuscatedFileSystemOriginEnumerator() {}

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
        ObfuscatedFileSystemFileUtil::GetDirectoryNameForType(type);
    if (type_string.empty()) {
      NOTREACHED();
      return false;
    }
    FilePath path = base_path_.Append(current_.path).Append(type_string);
    return file_util::DirectoryExists(path);
  }

 private:
  std::vector<OriginRecord> origins_;
  OriginRecord current_;
  FilePath base_path_;
};

ObfuscatedFileSystemFileUtil::AbstractOriginEnumerator*
ObfuscatedFileSystemFileUtil::CreateOriginEnumerator() {
  std::vector<FileSystemOriginDatabase::OriginRecord> origins;

  InitOriginDatabase(false);
  return new ObfuscatedFileSystemOriginEnumerator(
      origin_database_.get(), file_system_directory_);
}

FileSystemFileUtil::AbstractFileEnumerator*
ObfuscatedFileSystemFileUtil::CreateFileEnumerator(
    FileSystemOperationContext* context,
    const FilePath& root_path) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      context->src_origin_url(), context->src_type(), false);
  if (!db)
    return new FileSystemFileUtil::EmptyFileEnumerator();
  return new ObfuscatedFileSystemFileEnumerator(
      GetDirectoryForOriginAndType(context->src_origin_url(),
                                   context->src_type(), false),
      db,
      context,
      underlying_file_util_.get(),
      root_path);
}

PlatformFileError ObfuscatedFileSystemFileUtil::GetFileInfoInternal(
    FileSystemDirectoryDatabase* db,
    FileSystemOperationContext* context,
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
    file_info->is_directory = true;
    file_info->is_symbolic_link = false;
    file_info->last_modified = local_info->modification_time;
    *platform_file_path = FilePath();
    // We don't fill in ctime or atime.
    return base::PLATFORM_FILE_OK;
  }
  if (local_info->data_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  FilePath data_path = DataPathToLocalPath(context->src_origin_url(),
    context->src_type(), local_info->data_path);
  return underlying_file_util_->GetFileInfo(
      context, data_path, file_info, platform_file_path);
}

PlatformFileError ObfuscatedFileSystemFileUtil::CreateFile(
    FileSystemOperationContext* context,
    const GURL& origin_url, FileSystemType type, const FilePath& source_path,
    FileInfo* file_info, int file_flags, PlatformFile* handle) {
  if (handle)
    *handle = base::kInvalidPlatformFileValue;
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      origin_url, type, true);
  int64 number;
  if (!db || !db->GetNextInteger(&number))
    return base::PLATFORM_FILE_ERROR_FAILED;
  // We use the third- and fourth-to-last digits as the directory.
  int64 directory_number = number % 10000 / 100;
  // TODO(ericu): local_path is an OS path; underlying_file_util_ isn't
  // guaranteed to understand OS paths.
  FilePath local_path =
      GetDirectoryForOriginAndType(origin_url, type, false);
  if (local_path.empty())
    return base::PLATFORM_FILE_ERROR_FAILED;

  local_path = local_path.AppendASCII(StringPrintf("%02" PRIu64,
                                                   directory_number));
  PlatformFileError error;
  error = underlying_file_util_->CreateDirectory(
      context, local_path, false /* exclusive */, false /* recursive */);
  if (base::PLATFORM_FILE_OK != error)
    return error;
  local_path = local_path.AppendASCII(StringPrintf("%08" PRIu64, number));
  FilePath data_path = LocalPathToDataPath(origin_url, type, local_path);
  if (data_path.empty())
    return base::PLATFORM_FILE_ERROR_FAILED;
  bool created = false;
  if (!source_path.empty()) {
    DCHECK(!file_flags);
    DCHECK(!handle);
    error = underlying_file_util_->CopyOrMoveFile(
      context, source_path, local_path, true /* copy */);
    created = true;
  } else {
    FilePath path;
    underlying_file_util_->GetLocalFilePath(context, local_path, &path);
    if (file_util::PathExists(path)) {
      if (!file_util::Delete(path, true)) {
        NOTREACHED();
        return base::PLATFORM_FILE_ERROR_FAILED;
      }
      LOG(WARNING) << "A stray file detected";
      context->file_system_context()->GetQuotaUtil(context->src_type())->
          InvalidateUsageCache(context->src_origin_url(), context->src_type());
    }

    if (handle) {
      error = underlying_file_util_->CreateOrOpen(
        context, local_path, file_flags, handle, &created);
      // If this succeeds, we must close handle on any subsequent error.
    } else {
      DCHECK(!file_flags);  // file_flags is only used by CreateOrOpen.
      error = underlying_file_util_->EnsureFileExists(
          context, local_path, &created);
    }
  }
  if (error != base::PLATFORM_FILE_OK)
    return error;

  if (!created) {
    NOTREACHED();
    if (handle) {
      DCHECK_NE(base::kInvalidPlatformFileValue, *handle);
      base::ClosePlatformFile(*handle);
      underlying_file_util_->DeleteFile(context, local_path);
    }
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  file_info->data_path = data_path;
  FileId file_id;
  if (!db->AddFileInfo(*file_info, &file_id)) {
    if (handle) {
      DCHECK_NE(base::kInvalidPlatformFileValue, *handle);
      base::ClosePlatformFile(*handle);
    }
    underlying_file_util_->DeleteFile(context, local_path);
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  UpdatePathQuotaUsage(context, origin_url, type, 1, file_info->name.size());

  return base::PLATFORM_FILE_OK;
}

FilePath ObfuscatedFileSystemFileUtil::GetLocalPath(
    const GURL& origin_url,
    FileSystemType type,
    const FilePath& virtual_path) {
  FileSystemDirectoryDatabase* db = GetDirectoryDatabase(
      origin_url, type, false);
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
  FilePath path = origin_dir.Append(type_string);
  if (!file_util::DirectoryExists(path) &&
      (!create || !file_util::CreateDirectory(path)))
    return FilePath();
  return path;
}

bool ObfuscatedFileSystemFileUtil::DeleteDirectoryForOriginAndType(
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
  DCHECK_EQ(origin_path.value(), GetDirectoryForOrigin(origin, false).value());

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

bool ObfuscatedFileSystemFileUtil::MigrateFromOldSandbox(
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
FilePath::StringType ObfuscatedFileSystemFileUtil::GetDirectoryNameForType(
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
    const GURL& origin, FileSystemType type, bool create) {
  std::string type_string =
      FileSystemPathManager::GetFileSystemTypeString(type);
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
  path = path.AppendASCII(kDirectoryDatabaseName);
  FileSystemDirectoryDatabase* database = new FileSystemDirectoryDatabase(path);
  directories_[key] = database;
  return database;
}

FilePath ObfuscatedFileSystemFileUtil::GetDirectoryForOrigin(
    const GURL& origin, bool create) {
  if (!InitOriginDatabase(create))
    return FilePath();
  FilePath directory_name;
  std::string id = GetOriginIdentifierFromURL(origin);

  bool exists_in_db = origin_database_->HasOriginPath(id);
  if (!exists_in_db && !create)
    return FilePath();
  if (!origin_database_->GetPathForOrigin(id, &directory_name))
    return FilePath();

  FilePath path = file_system_directory_.Append(directory_name);
  bool exists_in_fs = file_util::DirectoryExists(path);
  if (!exists_in_db && exists_in_fs) {
    if (!file_util::Delete(path, true))
      return FilePath();
    exists_in_fs = false;
  }

  if (!exists_in_fs) {
    if (!create || !file_util::CreateDirectory(path))
      return FilePath();
  }

  return path;
}

void ObfuscatedFileSystemFileUtil::MarkUsed() {
  if (timer_.IsRunning())
    timer_.Reset();
  else
    timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(kFlushDelaySeconds),
                 this, &ObfuscatedFileSystemFileUtil::DropDatabases);
}

void ObfuscatedFileSystemFileUtil::DropDatabases() {
  origin_database_.reset();
  STLDeleteContainerPairSecondPointers(
      directories_.begin(), directories_.end());
  directories_.clear();
}

// static
int64 ObfuscatedFileSystemFileUtil::ComputeFilePathCost(const FilePath& path) {
  return GetPathQuotaUsage(1, path.BaseName().value().size());
}

bool ObfuscatedFileSystemFileUtil::DestroyDirectoryDatabase(
    const GURL& origin, FileSystemType type) {
  std::string type_string =
      FileSystemPathManager::GetFileSystemTypeString(type);
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
  path = path.AppendASCII(kDirectoryDatabaseName);
  return FileSystemDirectoryDatabase::DestroyDatabase(path);
}

bool ObfuscatedFileSystemFileUtil::InitOriginDatabase(bool create) {
  if (!origin_database_.get()) {
    if (!create && !file_util::DirectoryExists(file_system_directory_))
      return false;
    if (!file_util::CreateDirectory(file_system_directory_)) {
      LOG(WARNING) << "Failed to create FileSystem directory: " <<
          file_system_directory_.value();
      return false;
    }
    origin_database_.reset(
        new FileSystemOriginDatabase(
            file_system_directory_.AppendASCII(kOriginDatabaseName)));
  }
  return true;
}

}  // namespace fileapi
