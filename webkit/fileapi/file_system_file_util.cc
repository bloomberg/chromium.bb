// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_file_util.h"

#include <stack>

#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/file_system_operation_context.h"

namespace fileapi {

namespace {

// This assumes that the root exists.
bool ParentExists(FileSystemOperationContext* context,
    FileSystemFileUtil* file_util, const FilePath& file_path) {
  // If file_path is in the root, file_path.DirName() will be ".",
  // since we use paths with no leading '/'.
  FilePath parent = file_path.DirName();
  if (parent == FilePath(FILE_PATH_LITERAL(".")))
    return true;
  return file_util->DirectoryExists(context, parent);
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
    const FilePath& src_file_path,
    const FilePath& dest_file_path) {
  PlatformFileError error_code;
  error_code =
      PerformCommonCheckAndPreparationForMoveAndCopy(
          context, src_file_path, dest_file_path);
  if (error_code != base::PLATFORM_FILE_OK)
    return error_code;

  if (DirectoryExists(context, src_file_path))
    return CopyOrMoveDirectory(context, src_file_path, dest_file_path,
                               true /* copy */);
  return CopyOrMoveFileHelper(context, src_file_path, dest_file_path,
                              true /* copy */);
}

PlatformFileError FileSystemFileUtil::Move(
    FileSystemOperationContext* context,
    const FilePath& src_file_path,
    const FilePath& dest_file_path) {
  PlatformFileError error_code;
  error_code =
      PerformCommonCheckAndPreparationForMoveAndCopy(
          context, src_file_path, dest_file_path);
  if (error_code != base::PLATFORM_FILE_OK)
    return error_code;

  // TODO(dmikurube): ReplaceFile if in the same domain and filesystem type.
  if (DirectoryExists(context, src_file_path))
    return CopyOrMoveDirectory(context, src_file_path, dest_file_path,
                               false /* copy */);
  return CopyOrMoveFileHelper(context, src_file_path, dest_file_path,
                              false /* copy */);
}

PlatformFileError FileSystemFileUtil::Delete(
    FileSystemOperationContext* context,
    const FilePath& file_path,
    bool recursive) {
  if (DirectoryExists(context, file_path)) {
    if (!recursive)
      return DeleteSingleDirectory(context, file_path);
    else
      return DeleteDirectoryRecursive(context, file_path);
  } else {
    return DeleteFile(context, file_path);
  }
}

PlatformFileError FileSystemFileUtil::CreateOrOpen(
    FileSystemOperationContext* context,
    const FilePath& file_path, int file_flags,
    PlatformFile* file_handle, bool* created) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->CreateOrOpen(
        context, file_path, file_flags, file_handle, created);
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
    const FilePath& file_path,
    bool* created) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->EnsureFileExists(context, file_path, created);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::CreateDirectory(
    FileSystemOperationContext* context,
    const FilePath& file_path,
    bool exclusive,
    bool recursive) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->CreateDirectory(
        context, file_path, exclusive, recursive);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::GetFileInfo(
    FileSystemOperationContext* context,
    const FilePath& file_path,
    base::PlatformFileInfo* file_info,
    FilePath* platform_file_path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->GetFileInfo(
        context, file_path, file_info, platform_file_path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::ReadDirectory(
    FileSystemOperationContext* context,
    const FilePath& file_path,
    std::vector<base::FileUtilProxy::Entry>* entries) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->ReadDirectory(context, file_path, entries);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

FileSystemFileUtil::AbstractFileEnumerator*
FileSystemFileUtil::CreateFileEnumerator(
    FileSystemOperationContext* context,
    const FilePath& root_path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->CreateFileEnumerator(context, root_path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return NULL;
}

PlatformFileError FileSystemFileUtil::GetLocalFilePath(
    FileSystemOperationContext* context,
    const FilePath& virtual_path,
    FilePath* local_path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->GetLocalFilePath(
        context, virtual_path, local_path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::Touch(
    FileSystemOperationContext* context,
    const FilePath& file_path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->Touch(
        context, file_path, last_access_time, last_modified_time);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::Truncate(
    FileSystemOperationContext* context,
    const FilePath& file_path,
    int64 length) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->Truncate(context, file_path, length);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}


bool FileSystemFileUtil::PathExists(
    FileSystemOperationContext* context,
    const FilePath& file_path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->PathExists(context, file_path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return false;
}

bool FileSystemFileUtil::DirectoryExists(
    FileSystemOperationContext* context,
    const FilePath& file_path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->DirectoryExists(context, file_path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return false;
}

bool FileSystemFileUtil::IsDirectoryEmpty(
    FileSystemOperationContext* context,
    const FilePath& file_path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->IsDirectoryEmpty(context, file_path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return false;
}

PlatformFileError FileSystemFileUtil::CopyOrMoveFile(
    FileSystemOperationContext* context,
    const FilePath& src_file_path,
    const FilePath& dest_file_path,
    bool copy) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->CopyOrMoveFile(
        context, src_file_path, dest_file_path, copy);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::CopyInForeignFile(
      FileSystemOperationContext* context,
      const FilePath& src_file_path,
      const FilePath& dest_file_path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->CopyInForeignFile(
        context, src_file_path, dest_file_path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::DeleteFile(
    FileSystemOperationContext* context,
    const FilePath& file_path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->DeleteFile(context, file_path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::DeleteSingleDirectory(
    FileSystemOperationContext* context,
    const FilePath& file_path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->DeleteSingleDirectory(context, file_path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError
FileSystemFileUtil::PerformCommonCheckAndPreparationForMoveAndCopy(
    FileSystemOperationContext* context,
    const FilePath& src_file_path,
    const FilePath& dest_file_path) {
  bool same_file_system =
     (context->src_origin_url() == context->dest_origin_url()) &&
     (context->src_type() == context->dest_type());
  FileSystemFileUtil* dest_util = context->dest_file_util();
  DCHECK(dest_util);
  scoped_ptr<FileSystemOperationContext> local_dest_context;
  FileSystemOperationContext* dest_context = NULL;
  if (same_file_system) {
    dest_context = context;
    DCHECK(context->src_file_util() == context->dest_file_util());
  } else {
    local_dest_context.reset(context->CreateInheritedContextForDest());
    // All the single-path virtual FSFU methods expect the context information
    // to be in the src_* variables, not the dest_* variables, so we have to
    // make a new context if we want to call them on the dest_file_path.
    dest_context = local_dest_context.get();
  }

  // Exits earlier if the source path does not exist.
  if (!PathExists(context, src_file_path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  // The parent of the |dest_file_path| does not exist.
  if (!ParentExists(dest_context, dest_util, dest_file_path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  // It is an error to try to copy/move an entry into its child.
  if (same_file_system && src_file_path.IsParent(dest_file_path))
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;

  // Now it is ok to return if the |dest_file_path| does not exist.
  if (!dest_util->PathExists(dest_context, dest_file_path))
    return base::PLATFORM_FILE_OK;

  // |src_file_path| exists and is a directory.
  // |dest_file_path| exists and is a file.
  bool src_is_directory = DirectoryExists(context, src_file_path);
  bool dest_is_directory =
      dest_util->DirectoryExists(dest_context, dest_file_path);
  if (src_is_directory && !dest_is_directory)
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;

  // |src_file_path| exists and is a file.
  // |dest_file_path| exists and is a directory.
  if (!src_is_directory && dest_is_directory)
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;

  // It is an error to copy/move an entry into the same path.
  if (same_file_system && (src_file_path.value() == dest_file_path.value()))
    return base::PLATFORM_FILE_ERROR_EXISTS;

  if (dest_is_directory) {
    // It is an error to copy/move an entry to a non-empty directory.
    // Otherwise the copy/move attempt must overwrite the destination, but
    // the file_util's Copy or Move method doesn't perform overwrite
    // on all platforms, so we delete the destination directory here.
    // TODO(kinuko): may be better to change the file_util::{Copy,Move}.
    if (base::PLATFORM_FILE_OK !=
        dest_util->Delete(dest_context, dest_file_path,
                          false /* recursive */)) {
      if (!dest_util->IsDirectoryEmpty(dest_context, dest_file_path))
        return base::PLATFORM_FILE_ERROR_NOT_EMPTY;
      return base::PLATFORM_FILE_ERROR_FAILED;
    }
    // Reflect changes in usage back to the original context.
    if (!same_file_system)
      context->set_allowed_bytes_growth(dest_context->allowed_bytes_growth());
  }
  return base::PLATFORM_FILE_OK;
}

PlatformFileError FileSystemFileUtil::CopyOrMoveDirectory(
      FileSystemOperationContext* context,
      const FilePath& src_file_path,
      const FilePath& dest_file_path,
      bool copy) {
  FileSystemFileUtil* dest_util = context->dest_file_util();
  // All the single-path virtual FSFU methods expect the context information to
  // be in the src_* variables, not the dest_* variables, so we have to make a
  // new context if we want to call them on the dest_file_path.
  scoped_ptr<FileSystemOperationContext> dest_context(
      context->CreateInheritedContextForDest());

  // Re-check PerformCommonCheckAndPreparationForMoveAndCopy() by DCHECK.
  DCHECK(DirectoryExists(context, src_file_path));
  DCHECK(ParentExists(dest_context.get(), dest_util, dest_file_path));
  DCHECK(!dest_util->PathExists(dest_context.get(), dest_file_path));
  if ((context->src_origin_url() == context->dest_origin_url()) &&
      (context->src_type() == context->dest_type()))
    DCHECK(!src_file_path.IsParent(dest_file_path));

  if (!dest_util->DirectoryExists(dest_context.get(), dest_file_path)) {
    PlatformFileError error = dest_util->CreateDirectory(dest_context.get(),
        dest_file_path, false, false);
    if (error != base::PLATFORM_FILE_OK)
      return error;
    // Reflect changes in usage back to the original context.
    context->set_allowed_bytes_growth(dest_context->allowed_bytes_growth());
  }

  scoped_ptr<AbstractFileEnumerator> file_enum(
      CreateFileEnumerator(context, src_file_path));
  FilePath src_file_path_each;
  while (!(src_file_path_each = file_enum->Next()).empty()) {
    FilePath dest_file_path_each(dest_file_path);
    src_file_path.AppendRelativePath(src_file_path_each, &dest_file_path_each);

    if (file_enum->IsDirectory()) {
      PlatformFileError error = dest_util->CreateDirectory(dest_context.get(),
          dest_file_path_each, false, false);
      if (error != base::PLATFORM_FILE_OK)
        return error;
      // Reflect changes in usage back to the original context.
      context->set_allowed_bytes_growth(dest_context->allowed_bytes_growth());
    } else {
      PlatformFileError error = CopyOrMoveFileHelper(
          context, src_file_path_each, dest_file_path_each, copy);
      if (error != base::PLATFORM_FILE_OK)
        return error;
    }
  }

  if (!copy) {
    PlatformFileError error = Delete(context, src_file_path, true);
    if (error != base::PLATFORM_FILE_OK)
      return error;
  }

  return base::PLATFORM_FILE_OK;
}

PlatformFileError FileSystemFileUtil::CopyOrMoveFileHelper(
    FileSystemOperationContext* context,
    const FilePath& src_file_path,
    const FilePath& dest_file_path,
    bool copy) {
  // CopyOrMoveFile here is the virtual overridden member function.
  if ((context->src_origin_url() == context->dest_origin_url()) &&
      (context->src_type() == context->dest_type())) {
    DCHECK(context->src_file_util() == context->dest_file_util());
    return CopyOrMoveFile(context, src_file_path, dest_file_path, copy);
  }
  base::PlatformFileInfo file_info;
  FilePath platform_file_path;
  PlatformFileError error_code;
  error_code =
      GetFileInfo(context, src_file_path, &file_info, &platform_file_path);
  if (error_code != base::PLATFORM_FILE_OK)
    return error_code;

  DCHECK(context->dest_file_util());
  error_code = context->dest_file_util()->CopyInForeignFile(
      context, platform_file_path, dest_file_path);
  if (copy || error_code != base::PLATFORM_FILE_OK)
    return error_code;
  return DeleteFile(context, src_file_path);
}

PlatformFileError FileSystemFileUtil::DeleteDirectoryRecursive(
    FileSystemOperationContext* context,
    const FilePath& file_path) {
  scoped_ptr<AbstractFileEnumerator> file_enum(
      CreateFileEnumerator(context, file_path));
  FilePath file_path_each;

  std::stack<FilePath> directories;
  while (!(file_path_each = file_enum->Next()).empty()) {
    if (file_enum->IsDirectory()) {
      directories.push(file_path_each);
    } else {
      // DeleteFile here is the virtual overridden member function.
      PlatformFileError error = DeleteFile(context, file_path_each);
      if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
        return base::PLATFORM_FILE_ERROR_FAILED;
      else if (error != base::PLATFORM_FILE_OK)
        return error;
    }
  }

  while (!directories.empty()) {
    PlatformFileError error = DeleteSingleDirectory(context, directories.top());
    if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
      return base::PLATFORM_FILE_ERROR_FAILED;
    else if (error != base::PLATFORM_FILE_OK)
      return error;
    directories.pop();
  }
  return DeleteSingleDirectory(context, file_path);
}

}  // namespace fileapi
