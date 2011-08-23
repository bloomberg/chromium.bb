// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_operation_context.h"

#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_util.h"

namespace fileapi {

FileSystemOperationContext::FileSystemOperationContext(
    FileSystemContext* context,
    FileSystemFileUtil* file_system_file_util)
    : file_system_context_(context),
      src_file_system_file_util_(file_system_file_util),
      dest_file_system_file_util_(file_system_file_util),
      src_type_(kFileSystemTypeUnknown),
      dest_type_(kFileSystemTypeUnknown),
      allowed_bytes_growth_(0) {
}

FileSystemOperationContext::~FileSystemOperationContext() {
}

FileSystemOperationContext*
FileSystemOperationContext::CreateInheritedContextForDest() const {
  FileSystemOperationContext* context = new FileSystemOperationContext(
      file_system_context_.get(), dest_file_system_file_util_);
  context->set_src_origin_url(dest_origin_url_);
  context->set_src_type(dest_type_);
  context->set_allowed_bytes_growth(allowed_bytes_growth_);
  return context;
}

}  // namespace fileapi
