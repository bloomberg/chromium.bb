// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_util_helper.h"

#include <stack>

#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path.h"

using base::PlatformFileError;

namespace fileapi {

namespace {

// A helper class for cross-FileUtil Copy/Move operations.
class CrossFileUtilHelper {
 public:
  enum Operation {
    OPERATION_COPY,
    OPERATION_MOVE
  };

  CrossFileUtilHelper(FileSystemOperationContext* context,
                      FileSystemFileUtil* src_util,
                      FileSystemFileUtil* dest_util,
                      const FileSystemPath& src_path,
                      const FileSystemPath& dest_path,
                      Operation operation);
  ~CrossFileUtilHelper();

  base::PlatformFileError DoWork();

 private:
  // Performs common pre-operation check and preparation.
  // This may delete the destination directory if it's empty.
  base::PlatformFileError PerformErrorCheckAndPreparation();

  // This assumes that the root exists.
  bool ParentExists(const FileSystemPath& path, FileSystemFileUtil* file_util);

  // Performs recursive copy or move by calling CopyOrMoveFile for individual
  // files. Operations for recursive traversal are encapsulated in this method.
  // It assumes src_path and dest_path have passed
  // PerformErrorCheckAndPreparationForMoveAndCopy().
  base::PlatformFileError CopyOrMoveDirectory(
      const FileSystemPath& src_path,
      const FileSystemPath& dest_path);

  // Determines whether a simple same-filesystem move or copy can be done.  If
  // so, it delegates to CopyOrMoveFile.  Otherwise it looks up the true
  // platform path of the source file, delegates to CopyInForeignFile, and [for
  // move] calls DeleteFile on the source file.
  base::PlatformFileError CopyOrMoveFile(
      const FileSystemPath& src_path,
      const FileSystemPath& dest_path);

  FileSystemOperationContext* context_;
  FileSystemFileUtil* src_util_;  // Not owned.
  FileSystemFileUtil* dest_util_;  // Not owned.
  const FileSystemPath& src_root_path_;
  const FileSystemPath& dest_root_path_;
  Operation operation_;
  bool same_file_system_;

  DISALLOW_COPY_AND_ASSIGN(CrossFileUtilHelper);
};

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
      operation_(operation) {
  same_file_system_ =
      src_root_path_.origin() == dest_root_path_.origin() &&
      src_root_path_.type() == dest_root_path_.type();
}

CrossFileUtilHelper::~CrossFileUtilHelper() {}

base::PlatformFileError CrossFileUtilHelper::DoWork() {
  base::PlatformFileError error = PerformErrorCheckAndPreparation();
  if (error != base::PLATFORM_FILE_OK)
    return error;
  if (src_util_->DirectoryExists(context_, src_root_path_))
    return CopyOrMoveDirectory(src_root_path_, dest_root_path_);
  return CopyOrMoveFile(src_root_path_, dest_root_path_);
}

PlatformFileError CrossFileUtilHelper::PerformErrorCheckAndPreparation() {
  // Exits earlier if the source path does not exist.
  if (!src_util_->PathExists(context_, src_root_path_))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  // The parent of the |dest_root_path_| does not exist.
  if (!ParentExists(dest_root_path_, dest_util_))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  // It is an error to try to copy/move an entry into its child.
  if (same_file_system_ && src_root_path_.IsParent(dest_root_path_))
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
  if (same_file_system_ &&
      src_root_path_.internal_path() == dest_root_path_.internal_path())
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
  DCHECK(!same_file_system_ ||
         !src_path.IsParent(dest_path));

  PlatformFileError error = dest_util_->CreateDirectory(
      context_, dest_path, false, false);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum(
      src_util_->CreateFileEnumerator(context_, src_path, true));
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
  if (same_file_system_) {
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
      context_, platform_file_path, dest_path);

  if (operation_ == OPERATION_COPY || error != base::PLATFORM_FILE_OK)
    return error;
  return src_util_->DeleteFile(context_, src_path);
}

}  // anonymous namespace

// static
base::PlatformFileError FileUtilHelper::Copy(
    FileSystemOperationContext* context,
    FileSystemFileUtil* src_file_util,
    FileSystemFileUtil* dest_file_util,
    const FileSystemPath& src_root_path,
    const FileSystemPath& dest_root_path) {
  return CrossFileUtilHelper(context, src_file_util, dest_file_util,
                             src_root_path, dest_root_path,
                             CrossFileUtilHelper::OPERATION_COPY).DoWork();
}

// static
base::PlatformFileError FileUtilHelper::Move(
    FileSystemOperationContext* context,
    FileSystemFileUtil* src_file_util,
    FileSystemFileUtil* dest_file_util,
    const FileSystemPath& src_root_path,
    const FileSystemPath& dest_root_path) {
  return CrossFileUtilHelper(context, src_file_util, dest_file_util,
                             src_root_path, dest_root_path,
                             CrossFileUtilHelper::OPERATION_MOVE).DoWork();
}

// static
base::PlatformFileError FileUtilHelper::Delete(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemPath& path,
    bool recursive) {
  if (file_util->DirectoryExists(context, path)) {
    if (!recursive)
      return file_util->DeleteSingleDirectory(context, path);
    else
      return DeleteDirectoryRecursive(context, file_util, path);
  } else {
    return file_util->DeleteFile(context, path);
  }
}

// static
base::PlatformFileError FileUtilHelper::ReadDirectory(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemPath& path,
    std::vector<base::FileUtilProxy::Entry>* entries) {
  DCHECK(entries);

  // TODO(kkanetkar): Implement directory read in multiple chunks.
  if (!file_util->DirectoryExists(context, path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum(
      file_util->CreateFileEnumerator(context, path, false /* recursive */));

  FilePath current;
  while (!(current = file_enum->Next()).empty()) {
    // TODO(rkc): Fix this also once we've refactored file_util
    // http://code.google.com/p/chromium-os/issues/detail?id=15948
    // This currently just prevents a file from showing up at all
    // if it's a link, hence preventing arbitary 'read' exploits.
    if (file_enum->IsLink())
      continue;

    base::FileUtilProxy::Entry entry;
    entry.is_directory = file_enum->IsDirectory();
    entry.name = current.BaseName().value();
    entry.size = file_enum->Size();
    entry.last_modified_time = file_enum->LastModifiedTime();
    entries->push_back(entry);
  }
  return base::PLATFORM_FILE_OK;
}

// static
base::PlatformFileError FileUtilHelper::DeleteDirectoryRecursive(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemPath& path) {

  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum(
      file_util->CreateFileEnumerator(context, path, true /* recursive */));
  FilePath file_path_each;
  std::stack<FilePath> directories;
  while (!(file_path_each = file_enum->Next()).empty()) {
    if (file_enum->IsDirectory()) {
      directories.push(file_path_each);
    } else {
      PlatformFileError error = file_util->DeleteFile(
          context, path.WithInternalPath(file_path_each));
      if (error != base::PLATFORM_FILE_ERROR_NOT_FOUND &&
          error != base::PLATFORM_FILE_OK)
        return error;
    }
  }

  while (!directories.empty()) {
    PlatformFileError error = file_util->DeleteSingleDirectory(
        context, path.WithInternalPath(directories.top()));
    if (error != base::PLATFORM_FILE_ERROR_NOT_FOUND &&
        error != base::PLATFORM_FILE_OK)
      return error;
    directories.pop();
  }
  return file_util->DeleteSingleDirectory(context, path);
}

}  // namespace fileapi
