// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/recursive_operation_delegate.h"

#include "base/bind.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"

namespace fileapi {

namespace {
// Don't start too many inflight operations.
const int kMaxInflightOperations = 5;
}

RecursiveOperationDelegate::RecursiveOperationDelegate(
    FileSystemContext* file_system_context)
    : file_system_context_(file_system_context),
      inflight_operations_(0),
      canceled_(false) {
}

RecursiveOperationDelegate::~RecursiveOperationDelegate() {
}

void RecursiveOperationDelegate::Cancel() {
  // Set the cancel flag and remove all pending tasks.
  canceled_ = true;
  while (!pending_directories_.empty())
    pending_directories_.pop();
  while (!pending_files_.empty())
    pending_files_.pop();
}

void RecursiveOperationDelegate::StartRecursiveOperation(
    const FileSystemURL& root,
    const StatusCallback& callback) {
  callback_ = callback;
  pending_directories_.push(root);
  ProcessNextDirectory();
}

FileSystemOperationRunner* RecursiveOperationDelegate::operation_runner() {
  return file_system_context_->operation_runner();
}

void RecursiveOperationDelegate::ProcessNextDirectory() {
  DCHECK(pending_files_.empty());
  DCHECK_EQ(0, inflight_operations_);

  if (pending_directories_.empty()) {
    Done(base::PLATFORM_FILE_OK);
    return;
  }
  FileSystemURL url = pending_directories_.top();
  pending_directories_.pop();
  inflight_operations_++;
  ProcessDirectory(
      url, base::Bind(&RecursiveOperationDelegate::DidProcessDirectory,
                      AsWeakPtr(), url));
}

void RecursiveOperationDelegate::ProcessPendingFiles() {
  if (pending_files_.empty() && inflight_operations_ == 0) {
    ProcessNextDirectory();
    return;
  }
  while (!pending_files_.empty() &&
         inflight_operations_ < kMaxInflightOperations) {
    FileSystemURL url = pending_files_.top();
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
    Done(error);
    return;
  }
  ProcessPendingFiles();
}

void RecursiveOperationDelegate::DidProcessDirectory(
    const FileSystemURL& url,
    base::PlatformFileError error) {
  if (canceled_ || error != base::PLATFORM_FILE_OK) {
    Done(error);
    return;
  }
  operation_runner()->ReadDirectory(
      url, base::Bind(&RecursiveOperationDelegate::DidReadDirectory,
                      AsWeakPtr(), url));
}

void RecursiveOperationDelegate::DidReadDirectory(
    const FileSystemURL& parent,
    base::PlatformFileError error,
    const FileEntryList& entries,
    bool has_more) {
  if (canceled_) {
    Done(error);
    return;
  }

  if (error != base::PLATFORM_FILE_OK) {
    if (error == base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY) {
      // The given path may have been a file, so try ProcessFile now.
      ProcessFile(parent,
                  base::Bind(&RecursiveOperationDelegate::DidTryProcessFile,
                             AsWeakPtr(), error));
      return;
    }
    Done(error);
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
    Done(previous_error);
    return;
  }
  DidProcessFile(error);
}

void RecursiveOperationDelegate::Done(base::PlatformFileError error) {
  if (canceled_ && error == base::PLATFORM_FILE_OK) {
    callback_.Run(base::PLATFORM_FILE_ERROR_ABORT);
  } else {
    callback_.Run(error);
  }
}

}  // namespace fileapi
