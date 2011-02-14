// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_CONTEXT_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_CONTEXT_H_

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
class FileSystemContext;

struct DefaultContextDeleter;

// This class keeps and provides a file system context for FileSystem API.
class FileSystemContext
    : public base::RefCountedThreadSafe<FileSystemContext,
                                        DefaultContextDeleter> {
 public:
  FileSystemContext(
      scoped_refptr<base::MessageLoopProxy> file_message_loop,
      scoped_refptr<base::MessageLoopProxy> io_message_loop,
      const FilePath& profile_path,
      bool is_incognito,
      bool allow_file_access_from_files,
      bool unlimited_quota);
  ~FileSystemContext();

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

  DISALLOW_IMPLICIT_CONSTRUCTORS(FileSystemContext);
};

struct DefaultContextDeleter {
  static void Destruct(const FileSystemContext* context) {
    context->DeleteOnCorrectThread();
  }
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_CONTEXT_H_
