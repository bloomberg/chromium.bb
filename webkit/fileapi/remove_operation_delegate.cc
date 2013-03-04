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
    FileSystemContext* file_system_context,
    LocalFileSystemOperation* operation,
    const FileSystemURL& url,
    const StatusCallback& callback)
    : RecursiveOperationDelegate(file_system_context, operation),
      url_(url),
      callback_(callback) {
}

RemoveOperationDelegate::~RemoveOperationDelegate() {}

void RemoveOperationDelegate::Run() {
  NewNestedOperation()->RemoveFile(url_, base::Bind(
      &RemoveOperationDelegate::DidTryRemoveFile, AsWeakPtr()));
}

void RemoveOperationDelegate::RunRecursively() {
  StartRecursiveOperation(
      url_,
      base::Bind(&RemoveOperationDelegate::RemoveNextDirectory, AsWeakPtr()));
}

void RemoveOperationDelegate::ProcessFile(const FileSystemURL& url,
                                          const StatusCallback& callback) {
  if (to_remove_directories_.size() == 1u &&
      to_remove_directories_.top() == url) {
    // We seem to have been re-directed from ProcessDirectory.
    to_remove_directories_.pop();
  }
  NewNestedOperation()->RemoveFile(url, base::Bind(
      &RemoveOperationDelegate::DidRemoveFile, AsWeakPtr(), callback));
}

void RemoveOperationDelegate::ProcessDirectory(const FileSystemURL& url,
                                               const StatusCallback& callback) {
  to_remove_directories_.push(url);
  callback.Run(base::PLATFORM_FILE_OK);
}

void RemoveOperationDelegate::DidTryRemoveFile(
    base::PlatformFileError error) {
  if (error == base::PLATFORM_FILE_OK ||
      error != base::PLATFORM_FILE_ERROR_NOT_A_FILE) {
    callback_.Run(error);
    return;
  }
  NewNestedOperation()->RemoveDirectory(url_, callback_);
}

void RemoveOperationDelegate::DidRemoveFile(const StatusCallback& callback,
                                            base::PlatformFileError error) {
  if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND) {
    callback.Run(base::PLATFORM_FILE_OK);
    return;
  }
  callback.Run(error);
}

void RemoveOperationDelegate::RemoveNextDirectory(
    base::PlatformFileError error) {
  if (error != base::PLATFORM_FILE_OK ||
      to_remove_directories_.empty()) {
    callback_.Run(error);
    return;
  }
  FileSystemURL url = to_remove_directories_.top();
  to_remove_directories_.pop();
  NewNestedOperation()->RemoveDirectory(url, base::Bind(
      &RemoveOperationDelegate::RemoveNextDirectory,
      AsWeakPtr()));
}

}  // namespace fileapi
