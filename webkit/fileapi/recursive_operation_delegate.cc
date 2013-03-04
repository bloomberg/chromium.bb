// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/recursive_operation_delegate.h"

#include "base/bind.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/local_file_system_operation.h"

namespace fileapi {

namespace {
// Don't start too many inflight operations.
const int kMaxInflightOperations = 5;
}

RecursiveOperationDelegate::RecursiveOperationDelegate(
    FileSystemContext* file_system_context,
    LocalFileSystemOperation* operation)
    : file_system_context_(file_system_context),
      operation_(operation),
      inflight_operations_(0) {
}

RecursiveOperationDelegate::~RecursiveOperationDelegate() {
}

void RecursiveOperationDelegate::StartRecursiveOperation(
    const FileSystemURL& root,
    const StatusCallback& callback) {
  callback_ = callback;
  pending_directories_.push(root);
  ProcessNextDirectory();
}

LocalFileSystemOperation* RecursiveOperationDelegate::NewNestedOperation() {
  return operation_->CreateNestedOperation();
}

void RecursiveOperationDelegate::ProcessNextDirectory() {
  DCHECK(pending_files_.empty());
  if (inflight_operations_ > 0)
    return;
  if (pending_directories_.empty()) {
    callback_.Run(base::PLATFORM_FILE_OK);
    return;
  }
  FileSystemURL url = pending_directories_.front();
  pending_directories_.pop();
  inflight_operations_++;
  ProcessDirectory(
      url, base::Bind(&RecursiveOperationDelegate::DidProcessDirectory,
                      AsWeakPtr(), url));
}

void RecursiveOperationDelegate::ProcessPendingFiles() {
  if (pending_files_.empty()) {
    ProcessNextDirectory();
    return;
  }
  while (!pending_files_.empty() &&
         inflight_operations_ < kMaxInflightOperations) {
    FileSystemURL url = pending_files_.front();
    pending_files_.pop();
    inflight_operations_++;
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(&RecursiveOperationDelegate::ProcessFile,
                   AsWeakPtr(), url,
                   base::Bind(&RecursiveOperationDelegate::DidProcessFile,
                              AsWeakPtr())));
  }
}

void RecursiveOperationDelegate::DidProcessFile(base::PlatformFileError error) {
  inflight_operations_--;
  DCHECK_GE(inflight_operations_, 0);
  if (error != base::PLATFORM_FILE_OK) {
    callback_.Run(error);
    return;
  }
  ProcessPendingFiles();
}

void RecursiveOperationDelegate::DidProcessDirectory(
    const FileSystemURL& url,
    base::PlatformFileError error) {
  if (error != base::PLATFORM_FILE_OK) {
    callback_.Run(error);
    return;
  }
  NewNestedOperation()->ReadDirectory(
      url, base::Bind(&RecursiveOperationDelegate::DidReadDirectory,
                      AsWeakPtr(), url));
}

void RecursiveOperationDelegate::DidReadDirectory(
    const FileSystemURL& parent,
    base::PlatformFileError error,
    const FileEntryList& entries,
    bool has_more) {
  if (error != base::PLATFORM_FILE_OK) {
    if (error == base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY) {
      // The given path may have been a file, so try RemoveFile now.
      ProcessFile(parent,
                  base::Bind(&RecursiveOperationDelegate::DidTryProcessFile,
                             AsWeakPtr(), error));
      return;
    }
    callback_.Run(error);
    return;
  }
  for (size_t i = 0; i < entries.size(); i++) {
    FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
        parent.origin(),
        parent.mount_type(),
        parent.virtual_path().Append(entries[i].name));
    if (entries[i].is_directory)
      pending_directories_.push(url);
    else
      pending_files_.push(url);
  }
  if (has_more)
    return;

  inflight_operations_--;
  DCHECK_GE(inflight_operations_, 0);
  ProcessPendingFiles();
}

void RecursiveOperationDelegate::DidTryProcessFile(
    base::PlatformFileError previous_error,
    base::PlatformFileError error) {
  if (error == base::PLATFORM_FILE_ERROR_NOT_A_FILE) {
    // It wasn't a file either; returns with the previous error.
    callback_.Run(previous_error);
    return;
  }
  DidProcessFile(error);
}

}  // namespace fileapi
