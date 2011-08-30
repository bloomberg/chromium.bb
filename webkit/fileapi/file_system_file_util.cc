// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_file_util.h"

#include <stack>
#include <vector>

#include "base/file_util_proxy.h"
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

PlatformFileError FileSystemFileUtil::CreateOrOpen(
    FileSystemOperationContext* unused,
    const FilePath& file_path, int file_flags,
    PlatformFile* file_handle, bool* created) {
  if (!file_util::DirectoryExists(file_path.DirName())) {
    // If its parent does not exist, should return NOT_FOUND error.
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  }
  PlatformFileError error_code = base::PLATFORM_FILE_OK;
  *file_handle = base::CreatePlatformFile(file_path, file_flags,
                                          created, &error_code);
  return error_code;
}

PlatformFileError FileSystemFileUtil::Close(
    FileSystemOperationContext* unused,
    PlatformFile file_handle) {
  if (!base::ClosePlatformFile(file_handle))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError FileSystemFileUtil::EnsureFileExists(
    FileSystemOperationContext* unused,
    const FilePath& file_path,
    bool* created) {
  if (!file_util::DirectoryExists(file_path.DirName()))
    // If its parent does not exist, should return NOT_FOUND error.
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  PlatformFileError error_code = base::PLATFORM_FILE_OK;
  // Tries to create the |file_path| exclusively.  This should fail
  // with base::PLATFORM_FILE_ERROR_EXISTS if the path already exists.
  PlatformFile handle = base::CreatePlatformFile(
      file_path,
      base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_READ,
      created, &error_code);
  if (error_code == base::PLATFORM_FILE_ERROR_EXISTS) {
    // Make sure created_ is false.
    if (created)
      *created = false;
    error_code = base::PLATFORM_FILE_OK;
  }
  if (handle != base::kInvalidPlatformFileValue)
    base::ClosePlatformFile(handle);
  return error_code;
}

PlatformFileError FileSystemFileUtil::GetLocalFilePath(
    FileSystemOperationContext* context,
    const FilePath& virtual_path,
    FilePath* local_path) {
  *local_path = virtual_path;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError FileSystemFileUtil::GetFileInfo(
    FileSystemOperationContext* unused,
    const FilePath& file_path,
    base::PlatformFileInfo* file_info,
    FilePath* platform_file_path) {
  if (!file_util::PathExists(file_path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  // TODO(rkc): Fix this hack once we have refactored file_util to handle
  // symlinks correctly.
  // http://code.google.com/p/chromium-os/issues/detail?id=15948
  if (file_util::IsLink(file_path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  if (!file_util::GetFileInfo(file_path, file_info))
    return base::PLATFORM_FILE_ERROR_FAILED;
  *platform_file_path = file_path;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError FileSystemFileUtil::ReadDirectory(
    FileSystemOperationContext* unused,
    const FilePath& file_path,
    std::vector<base::FileUtilProxy::Entry>* entries) {
  // TODO(kkanetkar): Implement directory read in multiple chunks.
  if (!file_util::DirectoryExists(file_path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  file_util::FileEnumerator file_enum(
      file_path, false, static_cast<file_util::FileEnumerator::FileType>(
      file_util::FileEnumerator::FILES |
      file_util::FileEnumerator::DIRECTORIES));
  FilePath current;
  while (!(current = file_enum.Next()).empty()) {
    base::FileUtilProxy::Entry entry;
    file_util::FileEnumerator::FindInfo info;
    file_enum.GetFindInfo(&info);
    entry.is_directory = file_enum.IsDirectory(info);
    // This will just give the entry's name instead of entire path
    // if we use current.value().
    entry.name = file_util::FileEnumerator::GetFilename(info).value();
    entry.size = file_util::FileEnumerator::GetFilesize(info);
    entry.last_modified_time =
        file_util::FileEnumerator::GetLastModifiedTime(info);
    // TODO(rkc): Fix this also once we've refactored file_util
    // http://code.google.com/p/chromium-os/issues/detail?id=15948
    // This currently just prevents a file from showing up at all
    // if it's a link, hence preventing arbitary 'read' exploits.
    if (!file_util::IsLink(file_path.Append(entry.name)))
      entries->push_back(entry);
  }
  return base::PLATFORM_FILE_OK;
}

PlatformFileError FileSystemFileUtil::CreateDirectory(
    FileSystemOperationContext* unused,
    const FilePath& file_path,
    bool exclusive,
    bool recursive) {
  // If parent dir of file doesn't exist.
  if (!recursive && !file_util::PathExists(file_path.DirName()))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  bool path_exists = file_util::PathExists(file_path);
  if (exclusive && path_exists)
    return base::PLATFORM_FILE_ERROR_EXISTS;

  // If file exists at the path.
  if (path_exists && !file_util::DirectoryExists(file_path))
    return base::PLATFORM_FILE_ERROR_EXISTS;

  if (!file_util::CreateDirectory(file_path))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
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

PlatformFileError FileSystemFileUtil::Touch(
    FileSystemOperationContext* unused,
    const FilePath& file_path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  if (!file_util::TouchFile(
          file_path, last_access_time, last_modified_time))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError FileSystemFileUtil::Truncate(
    FileSystemOperationContext* unused,
    const FilePath& file_path,
    int64 length) {
  PlatformFileError error_code(base::PLATFORM_FILE_ERROR_FAILED);
  PlatformFile file =
      base::CreatePlatformFile(
          file_path,
          base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE,
          NULL,
          &error_code);
  if (error_code != base::PLATFORM_FILE_OK) {
    return error_code;
  }
  DCHECK_NE(base::kInvalidPlatformFileValue, file);
  if (!base::TruncatePlatformFile(file, length))
    error_code = base::PLATFORM_FILE_ERROR_FAILED;
  base::ClosePlatformFile(file);
  return error_code;
}

PlatformFileError
FileSystemFileUtil::PerformCommonCheckAndPreparationForMoveAndCopy(
    FileSystemOperationContext* context,
    const FilePath& src_file_path,
    const FilePath& dest_file_path) {
  bool same_file_system =
     (context->src_origin_url() == context->dest_origin_url()) &&
     (context->src_type() == context->dest_type());
  FileSystemFileUtil* dest_util = context->dest_file_system_file_util();
  DCHECK(dest_util);
  scoped_ptr<FileSystemOperationContext> local_dest_context;
  FileSystemOperationContext* dest_context = NULL;
  if (same_file_system) {
    dest_context = context;
    DCHECK(context->src_file_system_file_util() ==
         context->dest_file_system_file_util());
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

PlatformFileError FileSystemFileUtil::CopyOrMoveFile(
      FileSystemOperationContext* unused,
      const FilePath& src_file_path,
      const FilePath& dest_file_path,
      bool copy) {
  if (copy) {
    if (file_util::CopyFile(src_file_path, dest_file_path))
      return base::PLATFORM_FILE_OK;
  } else {
    DCHECK(!file_util::DirectoryExists(src_file_path));
    if (file_util::Move(src_file_path, dest_file_path))
      return base::PLATFORM_FILE_OK;
  }
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::CopyInForeignFile(
      FileSystemOperationContext* context,
      const FilePath& src_file_path,
      const FilePath& dest_file_path) {
  return CopyOrMoveFile(context, src_file_path, dest_file_path, true);
}

PlatformFileError FileSystemFileUtil::CopyOrMoveDirectory(
      FileSystemOperationContext* context,
      const FilePath& src_file_path,
      const FilePath& dest_file_path,
      bool copy) {
  FileSystemFileUtil* dest_util = context->dest_file_system_file_util();
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
    DCHECK(context->src_file_system_file_util() ==
           context->dest_file_system_file_util());
    return CopyOrMoveFile(context, src_file_path, dest_file_path, copy);
  }
  base::PlatformFileInfo file_info;
  FilePath platform_file_path;
  PlatformFileError error_code;
  error_code =
      GetFileInfo(context, src_file_path, &file_info, &platform_file_path);
  if (error_code != base::PLATFORM_FILE_OK)
    return error_code;

  DCHECK(context->dest_file_system_file_util());
  error_code = context->dest_file_system_file_util()->CopyInForeignFile(
      context, platform_file_path, dest_file_path);
  if (copy || error_code != base::PLATFORM_FILE_OK)
    return error_code;
  return DeleteFile(context, src_file_path);
}


PlatformFileError FileSystemFileUtil::DeleteFile(
    FileSystemOperationContext* unused,
    const FilePath& file_path) {
  if (!file_util::PathExists(file_path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  if (file_util::DirectoryExists(file_path))
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;
  if (!file_util::Delete(file_path, false))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError FileSystemFileUtil::DeleteSingleDirectory(
    FileSystemOperationContext* unused,
    const FilePath& file_path) {
  if (!file_util::PathExists(file_path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  if (!file_util::DirectoryExists(file_path)) {
    // TODO(dmikurube): Check if this error code is appropriate.
    return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
  }
  if (!file_util::IsDirectoryEmpty(file_path)) {
    // TODO(dmikurube): Check if this error code is appropriate.
    return base::PLATFORM_FILE_ERROR_NOT_EMPTY;
  }
  if (!file_util::Delete(file_path, false))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
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

bool FileSystemFileUtil::PathExists(
    FileSystemOperationContext* unused,
    const FilePath& file_path) {
  return file_util::PathExists(file_path);
}

bool FileSystemFileUtil::DirectoryExists(
    FileSystemOperationContext* unused,
    const FilePath& file_path) {
  return file_util::DirectoryExists(file_path);
}

bool FileSystemFileUtil::IsDirectoryEmpty(
    FileSystemOperationContext* unused,
    const FilePath& file_path) {
  return file_util::IsDirectoryEmpty(file_path);
}

class FileSystemFileEnumerator
    : public FileSystemFileUtil::AbstractFileEnumerator {
 public:
  FileSystemFileEnumerator(const FilePath& root_path,
                           bool recursive,
                           file_util::FileEnumerator::FileType file_type)
    : file_enum_(root_path, recursive, file_type) {
  }

  ~FileSystemFileEnumerator() {}

  virtual FilePath Next() OVERRIDE;
  virtual int64 Size() OVERRIDE;
  virtual bool IsDirectory() OVERRIDE;

 private:
  file_util::FileEnumerator file_enum_;
  file_util::FileEnumerator::FindInfo file_util_info_;
};

FilePath FileSystemFileEnumerator::Next() {
  FilePath rv = file_enum_.Next();
  if (!rv.empty())
    file_enum_.GetFindInfo(&file_util_info_);
  return rv;
}

int64 FileSystemFileEnumerator::Size() {
  return file_util::FileEnumerator::GetFilesize(file_util_info_);
}

bool FileSystemFileEnumerator::IsDirectory() {
  return file_util::FileEnumerator::IsDirectory(file_util_info_);
}

FileSystemFileUtil::AbstractFileEnumerator*
FileSystemFileUtil::CreateFileEnumerator(
    FileSystemOperationContext* unused,
    const FilePath& root_path) {
  return new FileSystemFileEnumerator(
      root_path, true, static_cast<file_util::FileEnumerator::FileType>(
      file_util::FileEnumerator::FILES |
      file_util::FileEnumerator::DIRECTORIES));
}

}  // namespace fileapi
