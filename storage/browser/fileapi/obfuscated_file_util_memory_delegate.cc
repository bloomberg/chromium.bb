// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/fileapi/obfuscated_file_util_memory_delegate.h"

namespace storage {

ObfuscatedFileUtilMemoryDelegate::ObfuscatedFileUtilMemoryDelegate()
    : weak_factory_(this) {}
ObfuscatedFileUtilMemoryDelegate::~ObfuscatedFileUtilMemoryDelegate() = default;

bool ObfuscatedFileUtilMemoryDelegate::DirectoryExists(
    const base::FilePath& path) {
  NOTIMPLEMENTED();
  // return base::DirectoryExists(path);
  return false;
}

bool ObfuscatedFileUtilMemoryDelegate::DeleteFileOrDirectory(
    const base::FilePath& path,
    bool recursive) {
  NOTIMPLEMENTED();
  // return base::DeleteFile(path, recursive);
  return false;
}

bool ObfuscatedFileUtilMemoryDelegate::IsLink(const base::FilePath& file_path) {
  NOTIMPLEMENTED();
  // return base::IsLink(file_path);
  return false;
}

bool ObfuscatedFileUtilMemoryDelegate::PathExists(const base::FilePath& path) {
  NOTIMPLEMENTED();
  // return base::PathExists(path);
  return false;
}

NativeFileUtil::CopyOrMoveMode
ObfuscatedFileUtilMemoryDelegate::CopyOrMoveModeForDestination(
    const FileSystemURL& dest_url,
    bool copy) {
  NOTIMPLEMENTED();
  // return NativeFileUtil::CopyOrMoveModeForDestination(dest_url, copy);
  return NativeFileUtil::CopyOrMoveMode::MOVE;
}

base::File ObfuscatedFileUtilMemoryDelegate::CreateOrOpen(
    const base::FilePath& path,
    int file_flags) {
  NOTIMPLEMENTED();
  // return NativeFileUtil::CreateOrOpen(path, file_flags);
  return base::File();
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::EnsureFileExists(
    const base::FilePath& path,
    bool* created) {
  NOTIMPLEMENTED();
  // return NativeFileUtil::EnsureFileExists(path, created);
  return base::File::FILE_ERROR_FAILED;
}
base::File::Error ObfuscatedFileUtilMemoryDelegate::CreateDirectory(
    const base::FilePath& path,
    bool exclusive,
    bool recursive) {
  NOTIMPLEMENTED();
  // return NativeFileUtil::CreateDirectory(path, exclusive, recursive);
  return base::File::FILE_ERROR_FAILED;
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::GetFileInfo(
    const base::FilePath& path,
    base::File::Info* file_info) {
  NOTIMPLEMENTED();
  // return NativeFileUtil::GetFileInfo(path, file_info);
  return base::File::FILE_ERROR_FAILED;
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::Touch(
    const base::FilePath& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  NOTIMPLEMENTED();
  // return NativeFileUtil::Touch(path, last_access_time, last_modified_time);
  return base::File::FILE_ERROR_FAILED;
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::Truncate(
    const base::FilePath& path,
    int64_t length) {
  NOTIMPLEMENTED();
  // return NativeFileUtil::Truncate(path, length);
  return base::File::FILE_ERROR_FAILED;
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::CopyOrMoveFile(
    const base::FilePath& src_path,
    const base::FilePath& dest_path,
    FileSystemOperation::CopyOrMoveOption option,
    NativeFileUtil::CopyOrMoveMode mode) {
  NOTIMPLEMENTED();
  // return NativeFileUtil::CopyOrMoveFile(src_path, dest_path, option, mode);
  return base::File::FILE_ERROR_FAILED;
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::DeleteFile(
    const base::FilePath& path) {
  NOTIMPLEMENTED();
  // return NativeFileUtil::DeleteFile(path);
  return base::File::FILE_ERROR_FAILED;
}

}  // namespace storage
