// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_operation_context.h"

#include "base/sequenced_task_runner.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_task_runners.h"

namespace fileapi {

FileSystemOperationContext::FileSystemOperationContext(
    FileSystemContext* context)
    : file_system_context_(context),
      task_runner_(file_system_context_->task_runners()->file_task_runner()),
      allowed_bytes_growth_(0) {}

FileSystemOperationContext::~FileSystemOperationContext() {}

void FileSystemOperationContext::set_task_runner(
    base::SequencedTaskRunner* task_runner) {
  task_runner_ = task_runner;
}

}  // namespace fileapi
