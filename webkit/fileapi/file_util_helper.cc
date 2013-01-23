// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_util_helper.h"

#include <stack>
#include <utility>

#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"

using base::PlatformFileError;

namespace fileapi {

namespace {

// A helper class to delete a temporary file.
class ScopedFileDeleter {
 public:
  explicit ScopedFileDeleter(const FilePath& path) : path_(path) {}
  ~ScopedFileDeleter() {
    file_util::Delete(path_, false /* recursive */);
  }

 private:
  FilePath path_;
};

bool IsInRoot(const FileSystemURL& url) {
  // If path is in the root, path.DirName() will be ".",
  // since we use paths with no leading '/'.
  FilePath parent = url.path().DirName();
  return parent.empty() || parent == FilePath(FILE_PATH_LITERAL("."));
}

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
                      const FileSystemURL& src_url,
                      const FileSystemURL& dest_url,
                      Operation operation);
  ~CrossFileUtilHelper();

  base::PlatformFileError DoWork();

 private:
  // Performs common pre-operation check and preparation.
  // This may delete the destination directory if it's empty.
  base::PlatformFileError PerformErrorCheckAndPreparation();

  // Performs recursive copy or move by calling CopyOrMoveFile for individual
  // files. Operations for recursive traversal are encapsulated in this method.
  // It assumes src_url and dest_url have passed
  // PerformErrorCheckAndPreparationForMoveAndCopy().
  base::PlatformFileError CopyOrMoveDirectory(
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url);

  // Determines whether a simple same-filesystem move or copy can be done.  If
  // so, it delegates to CopyOrMoveFile.  Otherwise it looks up the true
  // platform path of the source file, delegates to CopyInForeignFile, and [for
  // move] calls DeleteFile on the source file.
  base::PlatformFileError CopyOrMoveFile(
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url);

  FileSystemOperationContext* context_;
  FileSystemFileUtil* src_util_;  // Not owned.
  FileSystemFileUtil* dest_util_;  // Not owned.
  const FileSystemURL& src_root_url_;
  const FileSystemURL& dest_root_url_;
  Operation operation_;
  bool same_file_system_;

  DISALLOW_COPY_AND_ASSIGN(CrossFileUtilHelper);
};

CrossFileUtilHelper::CrossFileUtilHelper(
    FileSystemOperationContext* context,
    FileSystemFileUtil* src_util,
    FileSystemFileUtil* dest_util,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    Operation operation)
    : context_(context),
      src_util_(src_util),
      dest_util_(dest_util),
      src_root_url_(src_url),
      dest_root_url_(dest_url),
      operation_(operation) {
  DCHECK(src_util_);
  DCHECK(dest_util_);
  same_file_system_ =
      src_root_url_.origin() == dest_root_url_.origin() &&
      src_root_url_.type() == dest_root_url_.type();
}

CrossFileUtilHelper::~CrossFileUtilHelper() {}

base::PlatformFileError CrossFileUtilHelper::DoWork() {
  // It is an error to try to copy/move an entry into its child.
  if (same_file_system_ && src_root_url_.path().IsParent(dest_root_url_.path()))
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;

  // It is an error to copy/move an entry into the same path.
  if (same_file_system_ &&
      src_root_url_.path() == dest_root_url_.path())
    return base::PLATFORM_FILE_ERROR_EXISTS;

  // First try to copy/move the file.
  base::PlatformFileError error = CopyOrMoveFile(src_root_url_, dest_root_url_);
  if (error == base::PLATFORM_FILE_OK ||
      error != base::PLATFORM_FILE_ERROR_NOT_A_FILE)
    return error;

  // Now we should be sure that the source (and destination if exists)
  // is directory.
  // Now let's try to remove the destination directory, this must
  // fail if the directory isn't empty or its parent doesn't exist.
  error = dest_util_->DeleteDirectory(context_, dest_root_url_);
  if (error == base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY)
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  if (error != base::PLATFORM_FILE_OK &&
      error != base::PLATFORM_FILE_ERROR_NOT_FOUND)
    return error;

  // Perform the actual work for directory copy/move.
  return CopyOrMoveDirectory(src_root_url_, dest_root_url_);
}

PlatformFileError CrossFileUtilHelper::CopyOrMoveDirectory(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url) {
  // At this point we must have gone through
  // PerformErrorCheckAndPreparationForMoveAndCopy so this must be true.
  DCHECK(!same_file_system_ ||
         !src_url.path().IsParent(dest_url.path()));

  PlatformFileError error = dest_util_->CreateDirectory(
      context_, dest_url, false /* exclusive */, false /* recursive */);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  typedef std::pair<FileSystemURL, base::Time> MovedDirectoryPair;
  typedef std::vector<MovedDirectoryPair> MovedDirectories;
  MovedDirectories directories;

  // Store modified timestamp of the root directory.
  if (operation_ == OPERATION_MOVE) {
    base::PlatformFileInfo file_info;
    FilePath platform_file_path;
    error = src_util_->GetFileInfo(
        context_, src_url, &file_info, &platform_file_path);
    if (error != base::PLATFORM_FILE_OK)
      return error;
    directories.push_back(
        std::make_pair(dest_url, file_info.last_modified));
  }

  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum(
      src_util_->CreateFileEnumerator(context_,
                                      src_url,
                                      true /* recursive */));
  FilePath src_file_path_each;
  while (!(src_file_path_each = file_enum->Next()).empty()) {
    FilePath dest_file_path_each(dest_url.path());
    src_url.path().AppendRelativePath(
        src_file_path_each, &dest_file_path_each);

    if (file_enum->IsDirectory()) {
      error = dest_util_->CreateDirectory(
          context_,
          dest_url.WithPath(dest_file_path_each),
          true /* exclusive */, false /* recursive */);
      if (error != base::PLATFORM_FILE_OK)
        return error;

      directories.push_back(std::make_pair(
          dest_url.WithPath(dest_file_path_each),
          file_enum->LastModifiedTime()));
    } else {
      error = CopyOrMoveFile(src_url.WithPath(src_file_path_each),
                             dest_url.WithPath(dest_file_path_each));
      if (error != base::PLATFORM_FILE_OK)
        return error;
    }
  }

  if (operation_ == OPERATION_MOVE) {
    // Restore modified timestamp of destination directories.
    for (MovedDirectories::const_iterator it(directories.begin());
         it != directories.end(); ++it) {
      error = dest_util_->Touch(context_, it->first, it->second, it->second);
      if (error != base::PLATFORM_FILE_OK)
        return error;
    }

    error = FileUtilHelper::Delete(
        context_, src_util_, src_url, true /* recursive */);
    if (error != base::PLATFORM_FILE_OK)
      return error;
  }

  return base::PLATFORM_FILE_OK;
}

PlatformFileError CrossFileUtilHelper::CopyOrMoveFile(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url) {
  if (same_file_system_) {
    DCHECK(src_util_ == dest_util_);
    // Source and destination are in the same FileSystemFileUtil; now we can
    // safely call FileSystemFileUtil method on src_util_ (== dest_util_).
    return src_util_->CopyOrMoveFile(context_, src_url, dest_url,
                                     operation_ == OPERATION_COPY);
  }

  // Resolve the src_url's underlying file path.
  base::PlatformFileInfo file_info;
  FilePath platform_file_path;
  FileSystemFileUtil::SnapshotFilePolicy snapshot_policy;

  PlatformFileError error = src_util_->CreateSnapshotFile(
      context_, src_url, &file_info, &platform_file_path, &snapshot_policy);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  // For now we don't support non-snapshot file case.
  // TODO(kinuko): Address this case too.
  DCHECK(!platform_file_path.empty());

  scoped_ptr<ScopedFileDeleter> file_deleter;
  if (snapshot_policy == FileSystemFileUtil::kSnapshotFileTemporary)
    file_deleter.reset(new ScopedFileDeleter(platform_file_path));

  // Call CopyInForeignFile() on the dest_util_ with the resolved source path
  // to perform limited cross-FileSystemFileUtil copy/move.
  error = dest_util_->CopyInForeignFile(
      context_, platform_file_path, dest_url);

  if (operation_ == OPERATION_COPY || error != base::PLATFORM_FILE_OK)
    return error;
  return src_util_->DeleteFile(context_, src_url);
}

}  // anonymous namespace

// static
bool FileUtilHelper::DirectoryExists(FileSystemOperationContext* context,
                                     FileSystemFileUtil* file_util,
                                     const FileSystemURL& url) {
  if (url.path().empty())
    return true;

  base::PlatformFileInfo file_info;
  FilePath platform_path;
  PlatformFileError error = file_util->GetFileInfo(
      context, url, &file_info, &platform_path);
  return error == base::PLATFORM_FILE_OK && file_info.is_directory;
}

// static
base::PlatformFileError FileUtilHelper::Copy(
    FileSystemOperationContext* context,
    FileSystemFileUtil* src_file_util,
    FileSystemFileUtil* dest_file_util,
    const FileSystemURL& src_root_url,
    const FileSystemURL& dest_root_url) {
  return CrossFileUtilHelper(context, src_file_util, dest_file_util,
                             src_root_url, dest_root_url,
                             CrossFileUtilHelper::OPERATION_COPY).DoWork();
}

// static
base::PlatformFileError FileUtilHelper::Move(
    FileSystemOperationContext* context,
    FileSystemFileUtil* src_file_util,
    FileSystemFileUtil* dest_file_util,
    const FileSystemURL& src_root_url,
    const FileSystemURL& dest_root_url) {
  return CrossFileUtilHelper(context, src_file_util, dest_file_util,
                             src_root_url, dest_root_url,
                             CrossFileUtilHelper::OPERATION_MOVE).DoWork();
}

// static
base::PlatformFileError FileUtilHelper::Delete(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemURL& url,
    bool recursive) {
  if (DirectoryExists(context, file_util, url)) {
    if (!recursive)
      return file_util->DeleteDirectory(context, url);
    else
      return DeleteDirectoryRecursive(context, file_util, url);
  } else {
    return file_util->DeleteFile(context, url);
  }
}

// static
base::PlatformFileError FileUtilHelper::ReadDirectory(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemURL& url,
    std::vector<base::FileUtilProxy::Entry>* entries) {
  DCHECK(entries);

  base::PlatformFileInfo file_info;
  FilePath platform_path;
  PlatformFileError error = file_util->GetFileInfo(
      context, url, &file_info, &platform_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;
  if (!file_info.is_directory)
    return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;

  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum(
      file_util->CreateFileEnumerator(context, url, false /* recursive */));

  FilePath current;
  while (!(current = file_enum->Next()).empty()) {
    base::FileUtilProxy::Entry entry;
    entry.is_directory = file_enum->IsDirectory();
    entry.name = VirtualPath::BaseName(current).value();
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
    const FileSystemURL& url) {

  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum(
      file_util->CreateFileEnumerator(context, url, true /* recursive */));
  FilePath file_path_each;
  std::stack<FilePath> directories;
  while (!(file_path_each = file_enum->Next()).empty()) {
    if (file_enum->IsDirectory()) {
      directories.push(file_path_each);
    } else {
      PlatformFileError error = file_util->DeleteFile(
          context, url.WithPath(file_path_each));
      if (error != base::PLATFORM_FILE_ERROR_NOT_FOUND &&
          error != base::PLATFORM_FILE_OK)
        return error;
    }
  }

  while (!directories.empty()) {
    PlatformFileError error = file_util->DeleteDirectory(
        context, url.WithPath(directories.top()));
    if (error != base::PLATFORM_FILE_ERROR_NOT_FOUND &&
        error != base::PLATFORM_FILE_OK)
      return error;
    directories.pop();
  }
  return file_util->DeleteDirectory(context, url);
}

}  // namespace fileapi
