// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/cross_file_util_helper.h"

#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path.h"
#include "webkit/fileapi/file_util_helper.h"

using base::PlatformFileError;

namespace fileapi {

CrossFileUtilHelper::CrossFileUtilHelper(
    FileSystemOperationContext* context,
    FileSystemFileUtil* src_util,
    FileSystemFileUtil* dest_util,
    const FileSystemPath& src_path,
    const FileSystemPath& dest_path,
    Operation operation)
    : context_(context),
      src_util_(src_util),
      dest_util_(dest_util),
      src_root_path_(src_path),
      dest_root_path_(dest_path),
      operation_(operation) {}

CrossFileUtilHelper::~CrossFileUtilHelper() {}

base::PlatformFileError CrossFileUtilHelper::DoWork() {
  base::PlatformFileError error =
      PerformErrorCheckAndPreparationForMoveAndCopy();
  if (error != base::PLATFORM_FILE_OK)
    return error;
  if (src_util_->DirectoryExists(context_, src_root_path_)) {
    return CopyOrMoveDirectory(src_root_path_, dest_root_path_);
  }
  return CopyOrMoveFile(src_root_path_, dest_root_path_);
}

PlatformFileError
CrossFileUtilHelper::PerformErrorCheckAndPreparationForMoveAndCopy() {
  // Exits earlier if the source path does not exist.
  if (!src_util_->PathExists(context_, src_root_path_))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  bool same_file_system =
      (src_root_path_.origin() == dest_root_path_.origin()) &&
      (src_root_path_.type() == dest_root_path_.type());

  // The parent of the |dest_root_path_| does not exist.
  if (!ParentExists(dest_root_path_, dest_util_))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  // It is an error to try to copy/move an entry into its child.
  if (same_file_system && src_root_path_.IsParent(dest_root_path_))
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;

  // Now it is ok to return if the |dest_root_path_| does not exist.
  if (!dest_util_->PathExists(context_, dest_root_path_))
    return base::PLATFORM_FILE_OK;

  // |src_root_path_| exists and is a directory.
  // |dest_root_path_| exists and is a file.
  bool src_is_directory = src_util_->DirectoryExists(context_, src_root_path_);
  bool dest_is_directory =
      dest_util_->DirectoryExists(context_, dest_root_path_);

  // Either one of |src_root_path_| or |dest_root_path_| is directory,
  // while the other is not.
  if (src_is_directory != dest_is_directory)
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;

  // It is an error to copy/move an entry into the same path.
  if (same_file_system && (src_root_path_.internal_path() ==
                           dest_root_path_.internal_path()))
    return base::PLATFORM_FILE_ERROR_EXISTS;

  if (dest_is_directory) {
    // It is an error to copy/move an entry to a non-empty directory.
    // Otherwise the copy/move attempt must overwrite the destination, but
    // the file_util's Copy or Move method doesn't perform overwrite
    // on all platforms, so we delete the destination directory here.
    if (base::PLATFORM_FILE_OK !=
        dest_util_->DeleteSingleDirectory(context_, dest_root_path_)) {
      if (!dest_util_->IsDirectoryEmpty(context_, dest_root_path_))
        return base::PLATFORM_FILE_ERROR_NOT_EMPTY;
      return base::PLATFORM_FILE_ERROR_FAILED;
    }
  }
  return base::PLATFORM_FILE_OK;
}

bool CrossFileUtilHelper::ParentExists(
    const FileSystemPath& path, FileSystemFileUtil* file_util) {
  // If path is in the root, path.DirName() will be ".",
  // since we use paths with no leading '/'.
  FilePath parent = path.internal_path().DirName();
  if (parent == FilePath(FILE_PATH_LITERAL(".")))
    return true;
  return file_util->DirectoryExists(
      context_, path.WithInternalPath(parent));
}

PlatformFileError CrossFileUtilHelper::CopyOrMoveDirectory(
    const FileSystemPath& src_path,
    const FileSystemPath& dest_path) {
  // At this point we must have gone through
  // PerformErrorCheckAndPreparationForMoveAndCopy so this must be true.
  DCHECK(!((src_path.origin() == dest_path.origin()) &&
           (src_path.type() == dest_path.type())) ||
         !src_path.IsParent(dest_path));

  PlatformFileError error = dest_util_->CreateDirectory(
      context_, dest_path, false, false);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum(
      src_util_->CreateFileEnumerator(context_, src_path,
                                      true /* recursive */));
  FilePath src_file_path_each;
  while (!(src_file_path_each = file_enum->Next()).empty()) {
    FilePath dest_file_path_each(dest_path.internal_path());
    src_path.internal_path().AppendRelativePath(
        src_file_path_each, &dest_file_path_each);

    if (file_enum->IsDirectory()) {
      PlatformFileError error = dest_util_->CreateDirectory(
          context_,
          dest_path.WithInternalPath(dest_file_path_each),
          true /* exclusive */, false /* recursive */);
      if (error != base::PLATFORM_FILE_OK)
        return error;
    } else {
      PlatformFileError error = CopyOrMoveFile(
          src_path.WithInternalPath(src_file_path_each),
          dest_path.WithInternalPath(dest_file_path_each));
      if (error != base::PLATFORM_FILE_OK)
        return error;
    }
  }

  if (operation_ == OPERATION_MOVE) {
    PlatformFileError error =
        FileUtilHelper::Delete(context_, src_util_,
                               src_path, true /* recursive */);
    if (error != base::PLATFORM_FILE_OK)
      return error;
  }

  return base::PLATFORM_FILE_OK;
}

PlatformFileError CrossFileUtilHelper::CopyOrMoveFile(
    const FileSystemPath& src_path,
    const FileSystemPath& dest_path) {
  if ((src_path.origin() == dest_path.origin()) &&
      (src_path.type() == dest_path.type())) {
    DCHECK(src_util_ == dest_util_);
    // Source and destination are in the same FileSystemFileUtil; now we can
    // safely call FileSystemFileUtil method on src_util_ (== dest_util_).
    return src_util_->CopyOrMoveFile(context_, src_path, dest_path,
                                     operation_ == OPERATION_COPY);
  }

  // Resolve the src_path's underlying file path.
  base::PlatformFileInfo file_info;
  FilePath platform_file_path;
  PlatformFileError error = src_util_->GetFileInfo(
      context_, src_path, &file_info, &platform_file_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  // Call CopyInForeignFile() on the dest_util_ with the resolved source path
  // to perform limited cross-FileSystemFileUtil copy/move.
  error = dest_util_->CopyInForeignFile(
      context_, src_path.WithInternalPath(platform_file_path), dest_path);

  if (operation_ == OPERATION_COPY || error != base::PLATFORM_FILE_OK)
    return error;
  return src_util_->DeleteFile(context_, src_path);
}

}  // namespace fileapi
