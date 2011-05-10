// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CONTEXT_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CONTEXT_H_

#include "base/memory/ref_counted.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_types.h"

namespace fileapi {

class FileSystemContext;

class FileSystemOperationContext {
 public:
  FileSystemOperationContext(
      FileSystemContext* context,
      FileSystemFileUtil* file_system_file_util);
  ~FileSystemOperationContext();

  FileSystemContext* file_system_context() const {
    return file_system_context_.get();
  }

  FileSystemFileUtil* file_system_file_util() const {
    return file_system_file_util_;
  }

  void set_src_origin_url(const GURL& url) {
    src_origin_url_ = url;
  }

  const GURL& src_origin_url() const {
    return src_origin_url_;
  }

  void set_dest_origin_url(const GURL& url) {
    dest_origin_url_ = url;
  }

  const GURL& dest_origin_url() const {
    return dest_origin_url_;
  }

  void set_src_virtual_path(const FilePath& path) {
    src_virtual_path_ = path;
  }

  const FilePath& src_virtual_path() const {
    return src_virtual_path_;
  }

  void set_dest_virtual_path(const FilePath& path) {
    dest_virtual_path_ = path;
  }

  const FilePath& dest_virtual_path() const {
    return dest_virtual_path_;
  }

  FileSystemType src_type() const {
    return src_type_;
  }

  void set_src_type(FileSystemType src_type) {
    src_type_ = src_type;
  }

  FileSystemType dest_type() const {
    return dest_type_;
  }

  void set_dest_type(FileSystemType dest_type) {
    dest_type_ = dest_type;
  }

  void set_allowed_bytes_growth(const int64& allowed_bytes_growth) {
    allowed_bytes_growth_ = allowed_bytes_growth;
  }

  int64 allowed_bytes_growth() const { return allowed_bytes_growth_; }

 private:
  // This file_system_file_util_ is not "owned" by FileSystemOperationContext.
  // It is supposed to be a pointer to a singleton.
  scoped_refptr<FileSystemContext> file_system_context_;
  FileSystemFileUtil* file_system_file_util_;

  GURL src_origin_url_;  // Also used for any single-path operation.
  GURL dest_origin_url_;
  FileSystemType src_type_;  // Also used for any single-path operation.
  FileSystemType dest_type_;
  int64 allowed_bytes_growth_;

  // Used for delayed operation by quota.
  FilePath src_virtual_path_;  // Also used for any single-path operation.
  FilePath dest_virtual_path_;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_CONTEXT_H_
