// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SANDBOXED_FILE_SYSTEM_CONTEXT_H_
#define WEBKIT_FILEAPI_SANDBOXED_FILE_SYSTEM_CONTEXT_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"

class FilePath;
class GURL;

namespace base {
class MessageLoopProxy;
}

namespace fileapi {

class FileSystemPathManager;
class FileSystemQuotaManager;
class FileSystemUsageTracker;
class SandboxedFileSystemContext;

struct DefaultContextDeleter;

// This class keeps and provides a sandboxed file system context.
class SandboxedFileSystemContext
    : public base::RefCountedThreadSafe<SandboxedFileSystemContext,
                                        DefaultContextDeleter> {
 public:
  SandboxedFileSystemContext(
      scoped_refptr<base::MessageLoopProxy> file_message_loop,
      scoped_refptr<base::MessageLoopProxy> io_message_loop,
      const FilePath& profile_path,
      bool is_incognito,
      bool allow_file_access_from_files,
      bool unlimited_quota);
  ~SandboxedFileSystemContext();

  void DeleteDataForOriginOnFileThread(const GURL& origin_url);

  // Quota related methods.
  void SetOriginQuotaUnlimited(const GURL& url);
  void ResetOriginQuotaUnlimited(const GURL& url);

  FileSystemPathManager* path_manager() { return path_manager_.get(); }
  FileSystemQuotaManager* quota_manager() { return quota_manager_.get(); }
  FileSystemUsageTracker* usage_tracker() { return usage_tracker_.get(); }

 private:
  friend struct DefaultContextDeleter;
  void DeleteOnCorrectThread() const;

  scoped_refptr<base::MessageLoopProxy> file_message_loop_;
  scoped_refptr<base::MessageLoopProxy> io_message_loop_;
  scoped_ptr<FileSystemPathManager> path_manager_;
  scoped_ptr<FileSystemQuotaManager> quota_manager_;
  scoped_ptr<FileSystemUsageTracker> usage_tracker_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SandboxedFileSystemContext);
};

struct DefaultContextDeleter {
  static void Destruct(const SandboxedFileSystemContext* context) {
    context->DeleteOnCorrectThread();
  }
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SANDBOXED_FILE_SYSTEM_CONTEXT_H_
