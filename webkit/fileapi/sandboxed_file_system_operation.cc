// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/sandboxed_file_system_operation.h"

#include "net/url_request/url_request_context.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_quota_manager.h"
#include "webkit/fileapi/sandboxed_file_system_context.h"

namespace fileapi {

SandboxedFileSystemOperation::SandboxedFileSystemOperation(
    FileSystemCallbackDispatcher* dispatcher,
    scoped_refptr<base::MessageLoopProxy> proxy,
    SandboxedFileSystemContext* file_system_context)
    : FileSystemOperation(dispatcher, proxy),
      file_system_context_(file_system_context),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(file_system_context_);
}

void SandboxedFileSystemOperation::OpenFileSystem(
    const GURL& origin_url, fileapi::FileSystemType type, bool create) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = static_cast<FileSystemOperation::OperationType>(
      kOperationOpenFileSystem);
#endif

  file_system_context_->path_manager()->GetFileSystemRootPath(
      origin_url, type, create,
      callback_factory_.NewCallback(
          &SandboxedFileSystemOperation::DidGetRootPath));
}

void SandboxedFileSystemOperation::CreateFile(
    const FilePath& path, bool exclusive) {
  if (!VerifyFileSystemPathForWrite(path, true /* create */, 0))
    return;
  FileSystemOperation::CreateFile(path, exclusive);
}

void SandboxedFileSystemOperation::CreateDirectory(
    const FilePath& path, bool exclusive, bool recursive) {
  if (!VerifyFileSystemPathForWrite(path, true /* create */, 0))
    return;
  FileSystemOperation::CreateDirectory(path, exclusive, recursive);
}

void SandboxedFileSystemOperation::Copy(
    const FilePath& src_path, const FilePath& dest_path) {
  if (!VerifyFileSystemPathForRead(src_path) ||
      !VerifyFileSystemPathForWrite(dest_path, true /* create */,
                                    FileSystemQuotaManager::kUnknownSize))
    return;
  FileSystemOperation::Copy(src_path, dest_path);
}

void SandboxedFileSystemOperation::Move(
    const FilePath& src_path, const FilePath& dest_path) {
  if (!VerifyFileSystemPathForRead(src_path) ||
      !VerifyFileSystemPathForWrite(dest_path, true /* create */,
                                    FileSystemQuotaManager::kUnknownSize))
    return;
  FileSystemOperation::Move(src_path, dest_path);
}

void SandboxedFileSystemOperation::DirectoryExists(const FilePath& path) {
  if (!VerifyFileSystemPathForRead(path))
    return;
  FileSystemOperation::DirectoryExists(path);
}

void SandboxedFileSystemOperation::FileExists(const FilePath& path) {
  if (!VerifyFileSystemPathForRead(path))
    return;
  FileSystemOperation::FileExists(path);
}

void SandboxedFileSystemOperation::GetMetadata(const FilePath& path) {
  if (!VerifyFileSystemPathForRead(path))
    return;
  FileSystemOperation::GetMetadata(path);
}

void SandboxedFileSystemOperation::ReadDirectory(const FilePath& path) {
  if (!VerifyFileSystemPathForRead(path))
    return;
  FileSystemOperation::ReadDirectory(path);
}

void SandboxedFileSystemOperation::Remove(
    const FilePath& path, bool recursive) {
  if (!VerifyFileSystemPathForWrite(path, false /* create */, 0))
    return;
  FileSystemOperation::Remove(path, recursive);
}

void SandboxedFileSystemOperation::Write(
    scoped_refptr<URLRequestContext> url_request_context,
    const FilePath& path, const GURL& blob_url, int64 offset) {
  if (!VerifyFileSystemPathForWrite(path, true /* create */,
                                    FileSystemQuotaManager::kUnknownSize))
    return;
  FileSystemOperation::Write(url_request_context, path, blob_url, offset);
}

void SandboxedFileSystemOperation::Truncate(
    const FilePath& path, int64 length) {
  if (!VerifyFileSystemPathForWrite(path, false /* create */, 0))
    return;
  FileSystemOperation::Truncate(path, length);
}

void SandboxedFileSystemOperation::TouchFile(const FilePath& path,
                const base::Time& last_access_time,
                const base::Time& last_modified_time) {
  if (!VerifyFileSystemPathForWrite(path, true /* create */, 0))
    return;
  FileSystemOperation::TouchFile(path, last_access_time, last_modified_time);
}

void SandboxedFileSystemOperation::DidGetRootPath(
    bool success, const FilePath& path, const std::string& name) {
  DCHECK(success || path.empty());
  dispatcher()->DidOpenFileSystem(name, path);
}

bool SandboxedFileSystemOperation::VerifyFileSystemPathForRead(
    const FilePath& path) {
  // We may want do more checks, but for now it just checks if the given
  // |path| is under the valid FileSystem root path for this host context.
  if (!file_system_context_->path_manager()->CrackFileSystemPath(
          path, NULL, NULL, NULL)) {
    dispatcher()->DidFail(base::PLATFORM_FILE_ERROR_SECURITY);
    return false;
  }
  return true;
}

bool SandboxedFileSystemOperation::VerifyFileSystemPathForWrite(
    const FilePath& path, bool create, int64 growth) {
  GURL origin_url;
  FilePath virtual_path;
  if (!file_system_context_->path_manager()->CrackFileSystemPath(
          path, &origin_url, NULL, &virtual_path)) {
    dispatcher()->DidFail(base::PLATFORM_FILE_ERROR_SECURITY);
    return false;
  }
  // Any write access is disallowed on the root path.
  if (virtual_path.value().length() == 0 ||
      virtual_path.DirName().value() == virtual_path.value()) {
    dispatcher()->DidFail(base::PLATFORM_FILE_ERROR_SECURITY);
    return false;
  }
  if (create && file_system_context_->path_manager()->IsRestrictedFileName(
          path.BaseName())) {
    dispatcher()->DidFail(base::PLATFORM_FILE_ERROR_SECURITY);
    return false;
  }
  // TODO(kinuko): For operations with kUnknownSize we'll eventually
  // need to resolve what amount of size it's going to write.
  if (!file_system_context_->quota_manager()->CheckOriginQuota(
          origin_url, growth)) {
    dispatcher()->DidFail(base::PLATFORM_FILE_ERROR_NO_SPACE);
    return false;
  }
  return true;
}

bool SandboxedFileSystemOperation::CheckIfFilePathIsSafe(
    const FilePath& path) {
  if (file_system_context_->path_manager()->IsRestrictedFileName(
          path.BaseName())) {
    dispatcher()->DidFail(base::PLATFORM_FILE_ERROR_SECURITY);
    return false;
  }
  return true;
}

}  // namespace fileapi
