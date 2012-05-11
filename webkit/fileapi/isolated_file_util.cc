// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/isolated_file_util.h"

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path.h"
#include "webkit/fileapi/isolated_context.h"

using base::PlatformFileError;
using base::PlatformFileInfo;

namespace fileapi {

namespace {

// Simply enumerate each path from a given paths set.
// Used to enumerate top-level paths of an isolated filesystem.
class SetFileEnumerator : public FileSystemFileUtil::AbstractFileEnumerator {
 public:
  SetFileEnumerator(FileSystemFileUtil* underlying_file_util,
                    const FileSystemOperationContext& context,
                    const std::vector<FilePath>& paths,
                    const FileSystemPath& root)
      : underlying_file_util_(underlying_file_util),
        context_(context),
        paths_(paths),
        root_(root) {
    DCHECK(underlying_file_util_);
    path_iter_ = paths_.begin();
  }
  virtual ~SetFileEnumerator() {}

  // AbstractFileEnumerator overrides.
  virtual FilePath Next() OVERRIDE {
    if (path_iter_ == paths_.end())
      return FilePath();
    FilePath platform_path;
    underlying_file_util_->GetFileInfo(
        &context_, root_.WithInternalPath(*path_iter_++),
        &file_info_, &platform_path);
    return root_.internal_path().Append(platform_path.BaseName());
  }
  virtual int64 Size() OVERRIDE { return file_info_.size; }
  virtual bool IsDirectory() OVERRIDE { return file_info_.is_directory; }
  virtual base::Time LastModifiedTime() OVERRIDE {
    return file_info_.last_modified;
  }
  virtual bool IsLink() OVERRIDE { return file_info_.is_symbolic_link; }

 private:
  // Not owned; Enumerator is instantiated only within methods of
  // FileSystemFileUtil so it must be ok to assume the underlying_file_util
  // is alive throughout the lifetime of the enumerator.
  FileSystemFileUtil* underlying_file_util_;

  FileSystemOperationContext context_;
  std::vector<FilePath> paths_;
  std::vector<FilePath>::const_iterator path_iter_;
  FileSystemPath root_;
  base::PlatformFileInfo file_info_;
};

// A wrapper file enumerator which returns a virtual path of the path returned
// by the wrapped enumerator.
//
// A virtual path is constructed as following:
//   virtual_path = |virtual_base_path| + relative-path-to-|platform_base_path|
//
// Where |virtual_base_path| is in our case the virtual top-level directory
// that looks like: '/<filesystem_id>'.
//
// Example:
//    Suppose virtual_base_path is: '/CAFEBABE',
//           platform_base_path is: '/full/path/to/example/dir', and
//   a path returned by wrapped_is: '/full/path/to/example/dir/a/b/c',
//             Next() would return: '/CAFEBABE/dir/a/b/c'.
//
class PathConverterEnumerator
    : public FileSystemFileUtil::AbstractFileEnumerator {
 public:
  PathConverterEnumerator(
      FileSystemFileUtil::AbstractFileEnumerator* wrapped,
      const FilePath& virtual_base_path,
      const FilePath& platform_base_path)
      : wrapped_(wrapped),
        virtual_base_path_(virtual_base_path),
        platform_base_path_(platform_base_path) {}
  virtual ~PathConverterEnumerator() {}

  // AbstractFileEnumerator overrides.
  virtual FilePath Next() OVERRIDE {
    DCHECK(wrapped_.get());
    FilePath path = wrapped_->Next();
    if (path.empty())
      return path;
    FilePath virtual_path = virtual_base_path_;
    platform_base_path_.DirName().AppendRelativePath(path, &virtual_path);
    return virtual_path;
  }
  virtual int64 Size() OVERRIDE { return wrapped_->Size(); }
  virtual bool IsDirectory() OVERRIDE { return wrapped_->IsDirectory(); }
  virtual base::Time LastModifiedTime() OVERRIDE {
    return wrapped_->LastModifiedTime();
  }
  virtual bool IsLink() OVERRIDE { return wrapped_->IsLink(); }

 private:
  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> wrapped_;
  FilePath virtual_base_path_;
  FilePath platform_base_path_;
};

// Recursively enumerate each path from a given paths set.
class RecursiveSetFileEnumerator
    : public FileSystemFileUtil::AbstractFileEnumerator {
 public:
  RecursiveSetFileEnumerator(FileSystemFileUtil* underlying_file_util,
                             const FileSystemOperationContext& context,
                             const FilePath& virtual_base_path,
                             const std::vector<FilePath>& paths,
                             const FileSystemPath& root)
      : underlying_file_util_(underlying_file_util),
        context_(context),
        virtual_base_path_(virtual_base_path),
        paths_(paths),
        root_(root) {
    DCHECK(underlying_file_util_);
    path_iter_ = paths_.begin();
    current_enumerator_.reset(
        new SetFileEnumerator(underlying_file_util, context, paths, root));
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
  virtual bool IsLink() OVERRIDE {
    DCHECK(current_enumerator_.get());
    return current_enumerator_->IsLink();
  }

 private:
  // Not owned.
  FileSystemFileUtil* underlying_file_util_;

  FileSystemOperationContext context_;
  FilePath virtual_base_path_;
  std::vector<FilePath> paths_;
  std::vector<FilePath>::iterator path_iter_;
  base::PlatformFileInfo file_info_;
  FileSystemPath root_;
  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> current_enumerator_;
};

FilePath RecursiveSetFileEnumerator::Next() {
  if (current_enumerator_.get()) {
    FilePath path = current_enumerator_->Next();
    if (!path.empty())
      return path;
  }

  // We reached the end.
  if (path_iter_ == paths_.end())
    return FilePath();

  // Enumerates subdirectories of the next path.
  FilePath next_path = *path_iter_++;
  current_enumerator_.reset(
      new PathConverterEnumerator(
          underlying_file_util_->CreateFileEnumerator(
              &context_, root_.WithInternalPath(next_path),
              true /* recursive */),
          virtual_base_path_, next_path));
  DCHECK(current_enumerator_.get());
  return current_enumerator_->Next();
}

}  // namespace

IsolatedFileUtil::IsolatedFileUtil(FileSystemFileUtil* underlying_file_util)
    : FileSystemFileUtil(underlying_file_util) {
  DCHECK(underlying_file_util);
}

PlatformFileError IsolatedFileUtil::CreateOrOpen(
    FileSystemOperationContext* context,
    const FileSystemPath& path, int file_flags,
    PlatformFile* file_handle, bool* created) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError IsolatedFileUtil::Close(
    FileSystemOperationContext* context,
    PlatformFile file_handle) {
  // We don't allow open thus Close won't be called.
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError IsolatedFileUtil::EnsureFileExists(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    bool* created) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError IsolatedFileUtil::CreateDirectory(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    bool exclusive,
    bool recursive) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError IsolatedFileUtil::GetFileInfo(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    PlatformFileInfo* file_info,
    FilePath* platform_path) {
  DCHECK(file_info);
  std::string filesystem_id;
  FilePath root_unused, cracked_path;
  if (!IsolatedContext::GetInstance()->CrackIsolatedPath(
          path.internal_path(), &filesystem_id, &root_unused, &cracked_path))
    return base::PLATFORM_FILE_ERROR_SECURITY;
  if (cracked_path.empty()) {
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
  return underlying_file_util()->GetFileInfo(
      context, path.WithInternalPath(cracked_path), file_info, platform_path);
}

FileSystemFileUtil::AbstractFileEnumerator*
IsolatedFileUtil::CreateFileEnumerator(
    FileSystemOperationContext* context,
    const FileSystemPath& root,
    bool recursive) {
  std::string filesystem_id;
  FilePath root_path, cracked_path;
  if (!IsolatedContext::GetInstance()->CrackIsolatedPath(
          root.internal_path(), &filesystem_id, &root_path, &cracked_path))
    return NULL;

  FilePath virtual_base_path =
      IsolatedContext::GetInstance()->CreateVirtualPath(filesystem_id,
                                                        FilePath());

  if (!cracked_path.empty()) {
    return new PathConverterEnumerator(
        underlying_file_util()->CreateFileEnumerator(
            context, root.WithInternalPath(cracked_path), recursive),
        virtual_base_path, root_path);
  }

  // Root path case.
  std::vector<FilePath> toplevels;
  IsolatedContext::GetInstance()->GetTopLevelPaths(filesystem_id, &toplevels);
  if (!recursive)
    return new SetFileEnumerator(underlying_file_util(), *context,
                                 toplevels, root);
  return new RecursiveSetFileEnumerator(underlying_file_util(), *context,
                                        virtual_base_path, toplevels, root);
}

PlatformFileError IsolatedFileUtil::Touch(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError IsolatedFileUtil::Truncate(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    int64 length) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

bool IsolatedFileUtil::PathExists(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  FilePath platform_path;
  if (!GetPlatformPath(path, &platform_path))
    return false;
  if (platform_path.empty()) {
    // The root directory case.
    return true;
  }
  return underlying_file_util()->PathExists(
      context, path.WithInternalPath(platform_path));
}

bool IsolatedFileUtil::DirectoryExists(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  FilePath platform_path;
  if (!GetPlatformPath(path, &platform_path))
    return false;
  if (platform_path.empty()) {
    // The root directory case.
    return true;
  }
  return underlying_file_util()->DirectoryExists(
      context, path.WithInternalPath(platform_path));
}

bool IsolatedFileUtil::IsDirectoryEmpty(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  std::string filesystem_id;
  FilePath platform_path;
  if (!GetPlatformPath(path, &platform_path))
    return false;
  if (platform_path.empty()) {
    // The root directory case.
    std::vector<FilePath> toplevels;
    bool success = IsolatedContext::GetInstance()->GetTopLevelPaths(
        filesystem_id, &toplevels);
    DCHECK(success);
    return toplevels.empty();
  }
  return underlying_file_util()->IsDirectoryEmpty(
      context, path.WithInternalPath(platform_path));
}

PlatformFileError IsolatedFileUtil::CopyOrMoveFile(
      FileSystemOperationContext* context,
      const FileSystemPath& src_path,
      const FileSystemPath& dest_path,
      bool copy) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError IsolatedFileUtil::CopyInForeignFile(
      FileSystemOperationContext* context,
      const FilePath& src_file_path,
      const FileSystemPath& dest_path) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError IsolatedFileUtil::DeleteFile(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError IsolatedFileUtil::DeleteSingleDirectory(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

bool IsolatedFileUtil::GetPlatformPath(const FileSystemPath& virtual_path,
                                       FilePath* platform_path) const {
  DCHECK(platform_path);
  std::string filesystem_id;
  FilePath root_path;
  if (!IsolatedContext::GetInstance()->CrackIsolatedPath(
          virtual_path.internal_path(), &filesystem_id,
          &root_path, platform_path))
    return false;
  return true;
}

}  // namespace
