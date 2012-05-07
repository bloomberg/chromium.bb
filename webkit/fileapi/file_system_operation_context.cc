// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_operation_context.h"

#include "webkit/fileapi/file_system_context.h"

namespace fileapi {

FileSystemOperationContext::FileSystemOperationContext(
    FileSystemContext* context)
    : file_system_context_(context),
      allowed_bytes_growth_(0) {}

FileSystemOperationContext::~FileSystemOperationContext() {}

base::SequencedTaskRunner*
FileSystemOperationContext::file_task_runner() const {
  return file_system_context_->file_task_runner();
}

}  // namespace fileapi
