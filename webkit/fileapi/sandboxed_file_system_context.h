// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SANDBOXED_FILE_SYSTEM_CONTEXT_H_
#define WEBKIT_FILEAPI_SANDBOXED_FILE_SYSTEM_CONTEXT_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"

class FilePath;

namespace base {
class MessageLoopProxy;
}

namespace fileapi {

class FileSystemPathManager;
class FileSystemQuotaManager;

// This class keeps and provides a sandboxed file system context.
class SandboxedFileSystemContext {
 public:
  SandboxedFileSystemContext(
      scoped_refptr<base::MessageLoopProxy> file_message_loop,
      const FilePath& profile_path,
      bool is_incognito,
      bool allow_file_access_from_files,
      bool unlimited_quota);
  ~SandboxedFileSystemContext();

  void Shutdown();

  FileSystemPathManager* path_manager() { return path_manager_.get(); }
  FileSystemQuotaManager* quota_manager() { return quota_manager_.get(); }

 private:
  bool allow_file_access_from_files_;
  scoped_ptr<FileSystemPathManager> path_manager_;
  scoped_ptr<FileSystemQuotaManager> quota_manager_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SandboxedFileSystemContext);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SANDBOXED_FILE_SYSTEM_CONTEXT_H_
