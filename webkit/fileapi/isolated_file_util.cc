// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/isolated_file_util.h"

#include <string>
#include <vector>

#include "webkit/blob/shareable_file_reference.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/native_file_util.h"

using base::PlatformFileError;
using base::PlatformFileInfo;

namespace fileapi {

typedef IsolatedContext::FileInfo FileInfo;

namespace {

// Simply enumerate each path from a given fileinfo set.
// Used to enumerate top-level paths of an isolated filesystem.
class SetFileEnumerator : public FileSystemFileUtil::AbstractFileEnumerator {
 public:
  explicit SetFileEnumerator(const std::vector<FileInfo>& files)
      : files_(files) {
    file_iter_ = files_.begin();
  }
  virtual ~SetFileEnumerator() {}

  // AbstractFileEnumerator overrides.
  virtual FilePath Next() OVERRIDE {
    if (file_iter_ == files_.end())
      return FilePath();
    FilePath platform_file = (file_iter_++)->path;
    NativeFileUtil::GetFileInfo(platform_file, &file_info_);
    return platform_file;
  }
  virtual int64 Size() OVERRIDE { return file_info_.size; }
  virtual bool IsDirectory() OVERRIDE { return file_info_.is_directory; }
  virtual base::Time LastModifiedTime() OVERRIDE {
    return file_info_.last_modified;
  }

 private:
  std::vector<FileInfo> files_;
  std::vector<FileInfo>::const_iterator file_iter_;
  base::PlatformFileInfo file_info_;
};

// Recursively enumerate each path from a given paths set.
class RecursiveSetFileEnumerator
    : public FileSystemFileUtil::AbstractFileEnumerator {
 public:
  explicit RecursiveSetFileEnumerator(const std::vector<FileInfo>& files)
      : files_(files) {
    file_iter_ = files_.begin();
    current_enumerator_.reset(new SetFileEnumerator(files));
  }
  virtual ~RecursiveSetFileEnumerator() {}

  // AbstractFileEnumerator overrides.
  virtual FilePath Next() OVERRIDE;
  virtual int64 Size() OVERRIDE {
    DCHECK(current_enumerator_.get());
    return current_enumerator_->Size();
  }
  virtual bool IsDirectory() OVERRIDE {
    DCHECK(current_enumerator_.get());
    return current_enumerator_->IsDirectory();
  }
  virtual base::Time LastModifiedTime() OVERRIDE {
    DCHECK(current_enumerator_.get());
    return current_enumerator_->LastModifiedTime();
  }

 private:
  std::vector<FileInfo> files_;
  std::vector<FileInfo>::iterator file_iter_;
  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> current_enumerator_;
};

FilePath RecursiveSetFileEnumerator::Next() {
  if (current_enumerator_.get()) {
    FilePath path = current_enumerator_->Next();
    if (!path.empty())
      return path;
  }

  // We reached the end.
  if (file_iter_ == files_.end())
    return FilePath();

  // Enumerates subdirectories of the next path.
  FileInfo& next_file = *file_iter_++;
  current_enumerator_ = NativeFileUtil::CreateFileEnumerator(
      next_file.path, true /* recursive */);
  DCHECK(current_enumerator_.get());
  return current_enumerator_->Next();
}

}  // namespace

//-------------------------------------------------------------------------

IsolatedFileUtil::IsolatedFileUtil() {}

PlatformFileError IsolatedFileUtil::GetLocalFilePath(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    FilePath* local_file_path) {
  DCHECK(local_file_path);
  DCHECK(url.is_valid());
  if (url.path().empty()) {
    // Root direcory case, which should not be accessed.
    return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
  }
  *local_file_path = url.path();
  return base::PLATFORM_FILE_OK;
}

//-------------------------------------------------------------------------

DraggedFileUtil::DraggedFileUtil() {}

PlatformFileError DraggedFileUtil::GetFileInfo(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    PlatformFileInfo* file_info,
    FilePath* platform_path) {
  DCHECK(file_info);
  std::string filesystem_id;
  DCHECK(url.is_valid());
  if (url.path().empty()) {
    // The root directory case.
    // For now we leave three time fields (modified/accessed/creation time)
    // NULL as it is not really clear what to be set for this virtual directory.
    // TODO(kinuko): Maybe we want to set the time when this filesystem is
    // created (i.e. when the files/directories are dropped).
    file_info->is_directory = true;
    file_info->is_symbolic_link = false;
    file_info->size = 0;
    return base::PLATFORM_FILE_OK;
  }
  base::PlatformFileError error =
      NativeFileUtil::GetFileInfo(url.path(), file_info);
  if (file_util::IsLink(url.path()) && !FilePath().IsParent(url.path())) {
    // Don't follow symlinks unless it's the one that are selected by the user.
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  }
  if (error == base::PLATFORM_FILE_OK)
    *platform_path = url.path();
  return error;
}

scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator>
    DraggedFileUtil::CreateFileEnumerator(
        FileSystemOperationContext* context,
        const FileSystemURL& root,
        bool recursive) {
  DCHECK(root.is_valid());
  if (!root.path().empty())
    return NativeFileUtil::CreateFileEnumerator(root.path(), recursive);

  // Root path case.
  std::vector<FileInfo> toplevels;
  IsolatedContext::GetInstance()->GetDraggedFileInfo(
      root.filesystem_id(), &toplevels);
  if (!recursive) {
    return make_scoped_ptr(new SetFileEnumerator(toplevels))
        .PassAs<FileSystemFileUtil::AbstractFileEnumerator>();
  }
  return make_scoped_ptr(new RecursiveSetFileEnumerator(toplevels))
      .PassAs<FileSystemFileUtil::AbstractFileEnumerator>();
}

bool DraggedFileUtil::IsDirectoryEmpty(
    FileSystemOperationContext* context,
    const FileSystemURL& url) {
  DCHECK(url.is_valid());
  if (url.path().empty()) {
    // The root directory case.
    std::vector<FileInfo> toplevels;
    bool success = IsolatedContext::GetInstance()->GetDraggedFileInfo(
        url.filesystem_id(), &toplevels);
    DCHECK(success);
    return toplevels.empty();
  }
  return NativeFileUtil::IsDirectoryEmpty(url.path());
}

}  // namespace fileapi
