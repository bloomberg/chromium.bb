// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/remove_operation_delegate.h"

#include "base/bind.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/local_file_system_operation.h"

namespace fileapi {

RemoveOperationDelegate::RemoveOperationDelegate(
    LocalFileSystemOperation* original_operation,
    const StatusCallback& callback)
    : original_operation_(original_operation),
      callback_(callback),
      inflight_operations_(0) {
}

RemoveOperationDelegate::~RemoveOperationDelegate() {}

void RemoveOperationDelegate::Run(const FileSystemURL& url) {
  LocalFileSystemOperation* operation = NewOperation(url);
  if (!operation)
    return;
  operation->RemoveFile(url, base::Bind(
      &RemoveOperationDelegate::DidTryRemoveFile, AsWeakPtr(), url));
}

void RemoveOperationDelegate::RunRecursively(const FileSystemURL& url) {
  DCHECK(pending_directories_.empty());
  pending_directories_.push(url);
  ProcessNextDirectory(base::PLATFORM_FILE_OK);
}

void RemoveOperationDelegate::DidTryRemoveFile(
    const FileSystemURL& url,
    base::PlatformFileError error) {
  if (error == base::PLATFORM_FILE_OK ||
      error != base::PLATFORM_FILE_ERROR_NOT_A_FILE) {
    callback_.Run(error);
    return;
  }
  LocalFileSystemOperation* operation = NewOperation(url);
  if (!operation)
    return;
  operation->RemoveDirectory(url, callback_);
}

void RemoveOperationDelegate::ProcessNextDirectory(
    base::PlatformFileError error) {
  if (error != base::PLATFORM_FILE_OK) {
    callback_.Run(error);
    return;
  }
  if (inflight_operations_ > 0)
    return;
  if (pending_directories_.empty()) {
    RemoveNextDirectory(error);
    return;
  }
  FileSystemURL url = pending_directories_.front();
  pending_directories_.pop();
  LocalFileSystemOperation* operation = NewOperation(url);
  if (!operation)
    return;
  inflight_operations_++;
  operation->ReadDirectory(
      url, base::Bind(&RemoveOperationDelegate::DidReadDirectory,
                      AsWeakPtr(), url));
}

void RemoveOperationDelegate::DidReadDirectory(
    const FileSystemURL& parent,
    base::PlatformFileError error,
    const FileEntryList& entries,
    bool has_more) {
  if (error != base::PLATFORM_FILE_OK) {
    if (error == base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY) {
      // The given path may have been a file, so try RemoveFile.
      inflight_operations_--;
      DCHECK_GE(inflight_operations_, 0);
      RemoveFile(parent);
      return;
    }
    callback_.Run(error);
    return;
  }
  for (size_t i = 0; i < entries.size(); i++) {
    FileSystemURL url = parent.WithPath(parent.path().Append(entries[i].name));
    if (entries[i].is_directory) {
      pending_directories_.push(url);
      continue;
    }
    RemoveFile(url);
  }
  if (has_more)
    return;

  to_remove_directories_.push(parent);
  inflight_operations_--;
  DCHECK_GE(inflight_operations_, 0);
  ProcessNextDirectory(base::PLATFORM_FILE_OK);
}

void RemoveOperationDelegate::RemoveFile(const FileSystemURL& url) {
  LocalFileSystemOperation* operation = NewOperation(url);
  if (!operation)
    return;
  inflight_operations_++;
  operation->RemoveFile(url, base::Bind(
      &RemoveOperationDelegate::DidRemoveFile, AsWeakPtr()));
}

void RemoveOperationDelegate::DidRemoveFile(base::PlatformFileError error) {
  inflight_operations_--;
  DCHECK_GE(inflight_operations_, 0);
  if (error != base::PLATFORM_FILE_OK &&
      error != base::PLATFORM_FILE_ERROR_NOT_FOUND) {
    callback_.Run(error);
    return;
  }
  ProcessNextDirectory(error);
}

void RemoveOperationDelegate::RemoveNextDirectory(
    base::PlatformFileError error) {
  DCHECK_EQ(0, inflight_operations_);
  DCHECK(pending_directories_.empty());
  if (error != base::PLATFORM_FILE_OK ||
      to_remove_directories_.empty()) {
    callback_.Run(error);
    return;
  }
  FileSystemURL url = to_remove_directories_.top();
  to_remove_directories_.pop();
  LocalFileSystemOperation* operation = NewOperation(url);
  if (!operation)
    return;
  operation->RemoveDirectory(url, base::Bind(
      &RemoveOperationDelegate::RemoveNextDirectory,
      AsWeakPtr()));
}

LocalFileSystemOperation* RemoveOperationDelegate::NewOperation(
    const FileSystemURL& url) {
  base::PlatformFileError error;
  FileSystemOperation* operation = original_operation_->file_system_context()->
      CreateFileSystemOperation(url, &error);
  if (error != base::PLATFORM_FILE_OK) {
    callback_.Run(error);
    return NULL;
  }
  LocalFileSystemOperation* local_operation =
      operation->AsLocalFileSystemOperation();
  DCHECK(local_operation);

  // Let the new operation inherit from the original operation.
  local_operation->set_overriding_operation_context(
      original_operation_->operation_context());
  return local_operation;
}

}  // namespace fileapi
