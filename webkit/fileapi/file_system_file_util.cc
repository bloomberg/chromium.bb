// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_file_util.h"

#include <stack>

#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"

namespace fileapi {

namespace {

// This assumes that the root exists.
bool ParentExists(FileSystemOperationContext* context,
                  const FileSystemPath& path) {
  // If path is in the root, path.DirName() will be ".",
  // since we use paths with no leading '/'.
  FilePath parent = path.internal_path().DirName();
  if (parent == FilePath(FILE_PATH_LITERAL(".")))
    return true;
  return path.file_util()->DirectoryExists(
      context, path.WithInternalPath(parent));
}

}  // namespace

FileSystemFileUtil::FileSystemFileUtil() {
}

FileSystemFileUtil::FileSystemFileUtil(FileSystemFileUtil* underlying_file_util)
    : underlying_file_util_(underlying_file_util) {
}

FileSystemFileUtil::~FileSystemFileUtil() {
}

PlatformFileError FileSystemFileUtil::Copy(
    FileSystemOperationContext* context,
    const FileSystemPath& src_path,
    const FileSystemPath& dest_path) {
  PlatformFileError error_code;
  error_code =
      PerformCommonCheckAndPreparationForMoveAndCopy(
          context, src_path, dest_path);
  if (error_code != base::PLATFORM_FILE_OK)
    return error_code;

  if (DirectoryExists(context, src_path))
    return CopyOrMoveDirectory(context, src_path, dest_path,
                               true /* copy */);
  return CopyOrMoveFileHelper(context, src_path, dest_path,
                              true /* copy */);
}

PlatformFileError FileSystemFileUtil::Move(
    FileSystemOperationContext* context,
    const FileSystemPath& src_path,
    const FileSystemPath& dest_path) {
  PlatformFileError error_code;
  error_code =
      PerformCommonCheckAndPreparationForMoveAndCopy(
          context, src_path, dest_path);
  if (error_code != base::PLATFORM_FILE_OK)
    return error_code;

  // TODO(dmikurube): ReplaceFile if in the same domain and filesystem type.
  if (DirectoryExists(context, src_path))
    return CopyOrMoveDirectory(context, src_path, dest_path,
                               false /* copy */);
  return CopyOrMoveFileHelper(context, src_path, dest_path,
                              false /* copy */);
}

PlatformFileError FileSystemFileUtil::Delete(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    bool recursive) {
  if (DirectoryExists(context, path)) {
    if (!recursive)
      return DeleteSingleDirectory(context, path);
    else
      return DeleteDirectoryRecursive(context, path);
  } else {
    return DeleteFile(context, path);
  }
}

PlatformFileError FileSystemFileUtil::CreateOrOpen(
    FileSystemOperationContext* context,
    const FileSystemPath& path, int file_flags,
    PlatformFile* file_handle, bool* created) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->CreateOrOpen(
        context, path, file_flags, file_handle, created);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::Close(
    FileSystemOperationContext* context,
    PlatformFile file_handle) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->Close(context, file_handle);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::EnsureFileExists(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    bool* created) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->EnsureFileExists(context, path, created);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::CreateDirectory(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    bool exclusive,
    bool recursive) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->CreateDirectory(
        context, path, exclusive, recursive);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::GetFileInfo(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    base::PlatformFileInfo* file_info,
    FilePath* platform_file_path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->GetFileInfo(
        context, path, file_info, platform_file_path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::ReadDirectory(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    std::vector<base::FileUtilProxy::Entry>* entries) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->ReadDirectory(context, path, entries);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

FileSystemFileUtil::AbstractFileEnumerator*
FileSystemFileUtil::CreateFileEnumerator(
    FileSystemOperationContext* context,
    const FileSystemPath& root_path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->CreateFileEnumerator(context, root_path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return NULL;
}

PlatformFileError FileSystemFileUtil::GetLocalFilePath(
    FileSystemOperationContext* context,
    const FileSystemPath& file_system_path,
    FilePath* local_file_path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->GetLocalFilePath(
        context, file_system_path, local_file_path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::Touch(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->Touch(
        context, path, last_access_time, last_modified_time);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::Truncate(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    int64 length) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->Truncate(context, path, length);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}


bool FileSystemFileUtil::PathExists(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->PathExists(context, path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return false;
}

bool FileSystemFileUtil::DirectoryExists(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->DirectoryExists(context, path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return false;
}

bool FileSystemFileUtil::IsDirectoryEmpty(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->IsDirectoryEmpty(context, path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return false;
}

PlatformFileError FileSystemFileUtil::CopyOrMoveFile(
    FileSystemOperationContext* context,
    const FileSystemPath& src_path,
    const FileSystemPath& dest_path,
    bool copy) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->CopyOrMoveFile(
        context, src_path, dest_path, copy);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::CopyInForeignFile(
      FileSystemOperationContext* context,
      const FileSystemPath& underlying_src_path,
      const FileSystemPath& dest_path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->CopyInForeignFile(
        context, underlying_src_path, dest_path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::DeleteFile(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->DeleteFile(context, path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::DeleteSingleDirectory(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->DeleteSingleDirectory(context, path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError
FileSystemFileUtil::PerformCommonCheckAndPreparationForMoveAndCopy(
    FileSystemOperationContext* context,
    const FileSystemPath& src_path,
    const FileSystemPath& dest_path) {
  // Exits earlier if the source path does not exist.
  if (!PathExists(context, src_path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  // It is an error to copy/move an entry into the same path.
  if (src_path == dest_path)
    return base::PLATFORM_FILE_ERROR_EXISTS;

  bool same_file_system =
     (src_path.origin() == dest_path.origin()) &&
     (src_path.type() == dest_path.type());
  FileSystemFileUtil* dest_util = dest_path.file_util();
  DCHECK(dest_util);

  // The parent of the |dest_path| does not exist.
  if (!ParentExists(context, dest_path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  // It is an error to try to copy/move an entry into its child.
  if (same_file_system && src_path.IsParent(dest_path))
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;

  // Now it is ok to return if the |dest_path| does not exist.
  if (!dest_util->PathExists(context, dest_path))
    return base::PLATFORM_FILE_OK;

  // |src_path| exists and is a directory.
  // |dest_path| exists and is a file.
  bool src_is_directory = DirectoryExists(context, src_path);
  bool dest_is_directory =
      dest_util->DirectoryExists(context, dest_path);
  if (src_is_directory && !dest_is_directory)
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;

  // |src_path| exists and is a file.
  // |dest_path| exists and is a directory.
  if (!src_is_directory && dest_is_directory)
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;

  if (dest_is_directory) {
    // It is an error to copy/move an entry to a non-empty directory.
    // Otherwise the copy/move attempt must overwrite the destination, but
    // the file_util's Copy or Move method doesn't perform overwrite
    // on all platforms, so we delete the destination directory here.
    // TODO(kinuko): may be better to change the file_util::{Copy,Move}.
    if (base::PLATFORM_FILE_OK !=
        dest_util->Delete(context, dest_path, false /* recursive */)) {
      if (!dest_util->IsDirectoryEmpty(context, dest_path))
        return base::PLATFORM_FILE_ERROR_NOT_EMPTY;
      return base::PLATFORM_FILE_ERROR_FAILED;
    }
  }
  return base::PLATFORM_FILE_OK;
}

PlatformFileError FileSystemFileUtil::CopyOrMoveDirectory(
      FileSystemOperationContext* context,
      const FileSystemPath& src_path,
      const FileSystemPath& dest_path,
      bool copy) {
  FileSystemFileUtil* dest_util = dest_path.file_util();

  // Re-check PerformCommonCheckAndPreparationForMoveAndCopy() by DCHECK.
  DCHECK(DirectoryExists(context, src_path));
  DCHECK(ParentExists(context, dest_path));
  DCHECK(!dest_util->PathExists(context, dest_path));
  if ((src_path.origin() == dest_path.origin()) &&
      (src_path.type() == dest_path.type()))
    DCHECK(!src_path.IsParent(dest_path));

  if (!dest_util->DirectoryExists(context, dest_path)) {
    PlatformFileError error = dest_util->CreateDirectory(
        context, dest_path, false, false);
    if (error != base::PLATFORM_FILE_OK)
      return error;
  }

  scoped_ptr<AbstractFileEnumerator> file_enum(
      CreateFileEnumerator(context, src_path));
  FilePath src_file_path_each;
  while (!(src_file_path_each = file_enum->Next()).empty()) {
    FilePath dest_file_path_each(dest_path.internal_path());
    src_path.internal_path().AppendRelativePath(
        src_file_path_each, &dest_file_path_each);

    if (file_enum->IsDirectory()) {
      PlatformFileError error = dest_util->CreateDirectory(
          context,
          dest_path.WithInternalPath(dest_file_path_each),
          false, false);
      if (error != base::PLATFORM_FILE_OK)
        return error;
    } else {
      PlatformFileError error = CopyOrMoveFileHelper(
          context,
          src_path.WithInternalPath(src_file_path_each),
          dest_path.WithInternalPath(dest_file_path_each),
          copy);
      if (error != base::PLATFORM_FILE_OK)
        return error;
    }
  }

  if (!copy) {
    PlatformFileError error = Delete(context, src_path, true);
    if (error != base::PLATFORM_FILE_OK)
      return error;
  }

  return base::PLATFORM_FILE_OK;
}

PlatformFileError FileSystemFileUtil::CopyOrMoveFileHelper(
    FileSystemOperationContext* context,
    const FileSystemPath& src_path,
    const FileSystemPath& dest_path,
    bool copy) {
  // CopyOrMoveFile here is the virtual overridden member function.
  if ((src_path.origin() == dest_path.origin()) &&
      (src_path.type() == dest_path.type())) {
    DCHECK(src_path.file_util() == dest_path.file_util());
    return CopyOrMoveFile(context, src_path, dest_path, copy);
  }

  base::PlatformFileInfo file_info;
  FilePath underlying_file_path;
  PlatformFileError error_code;
  error_code = src_path.file_util()->GetFileInfo(
      context, src_path, &file_info, &underlying_file_path);
  if (error_code != base::PLATFORM_FILE_OK)
    return error_code;

  DCHECK(dest_path.file_util());
  error_code = dest_path.file_util()->CopyInForeignFile(
      context, src_path.WithInternalPath(underlying_file_path), dest_path);
  if (copy || error_code != base::PLATFORM_FILE_OK)
    return error_code;
  return src_path.file_util()->DeleteFile(context, src_path);
}

PlatformFileError FileSystemFileUtil::DeleteDirectoryRecursive(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  scoped_ptr<AbstractFileEnumerator> file_enum(
      CreateFileEnumerator(context, path));
  FilePath file_path_each;
  std::stack<FilePath> directories;
  while (!(file_path_each = file_enum->Next()).empty()) {
    if (file_enum->IsDirectory()) {
      directories.push(file_path_each);
    } else {
      // DeleteFile here is the virtual overridden member function.
      PlatformFileError error = DeleteFile(
          context, path.WithInternalPath(file_path_each));
      if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
        return base::PLATFORM_FILE_ERROR_FAILED;
      else if (error != base::PLATFORM_FILE_OK)
        return error;
    }
  }

  while (!directories.empty()) {
    PlatformFileError error = DeleteSingleDirectory(
        context, path.WithInternalPath(directories.top()));
    if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
      return base::PLATFORM_FILE_ERROR_FAILED;
    else if (error != base::PLATFORM_FILE_OK)
      return error;
    directories.pop();
  }
  return DeleteSingleDirectory(context, path);
}

}  // namespace fileapi
