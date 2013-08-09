// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/remove_operation_delegate.h"

#include "base/bind.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"

namespace fileapi {

RemoveOperationDelegate::RemoveOperationDelegate(
    FileSystemContext* file_system_context,
    const FileSystemURL& url,
    const StatusCallback& callback)
    : RecursiveOperationDelegate(file_system_context),
      url_(url),
      callback_(callback),
      weak_factory_(this) {
}

RemoveOperationDelegate::~RemoveOperationDelegate() {}

void RemoveOperationDelegate::Run() {
  operation_runner()->RemoveFile(url_, base::Bind(
      &RemoveOperationDelegate::DidTryRemoveFile, weak_factory_.GetWeakPtr()));
}

void RemoveOperationDelegate::RunRecursively() {
  StartRecursiveOperation(
      url_,
      base::Bind(&RemoveOperationDelegate::RemoveNextDirectory,
                 weak_factory_.GetWeakPtr()));
}

void RemoveOperationDelegate::ProcessFile(const FileSystemURL& url,
                                          const StatusCallback& callback) {
  if (to_remove_directories_.size() == 1u &&
      to_remove_directories_.top() == url) {
    // We seem to have been re-directed from ProcessDirectory.
    to_remove_directories_.pop();
  }
  operation_runner()->RemoveFile(url, base::Bind(
      &RemoveOperationDelegate::DidRemoveFile,
      weak_factory_.GetWeakPtr(), callback));
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
  operation_runner()->RemoveDirectory(url_, callback_);
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
  operation_runner()->RemoveDirectory(url, base::Bind(
      &RemoveOperationDelegate::RemoveNextDirectory,
      weak_factory_.GetWeakPtr()));
}

}  // namespace fileapi
