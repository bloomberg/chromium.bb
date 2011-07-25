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
      allowed_bytes_growth_(0),
      do_not_write_actually_(false) {
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
  context->set_do_not_write_actually(do_not_write_actually_);
  context->set_src_virtual_path(dest_virtual_path_);
  return context;
}

FileSystemOperationContext*
FileSystemOperationContext::CreateInheritedContextWithNewVirtualPaths(
    const FilePath& new_src_virtual_path,
    const FilePath& new_dest_virtual_path) const {
  FileSystemOperationContext* context = new FileSystemOperationContext(*this);
  context->set_src_virtual_path(new_src_virtual_path);
  context->set_dest_virtual_path(new_dest_virtual_path);
  return context;
}

void FileSystemOperationContext::ImportAllowedBytesGrowth(
    const FileSystemOperationContext& other) {
  allowed_bytes_growth_ = other.allowed_bytes_growth();
}

}  // namespace fileapi
