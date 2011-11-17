// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_CONTEXT_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_CONTEXT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/quota/special_storage_policy.h"

class FilePath;
class GURL;

namespace base {
class MessageLoopProxy;
}

namespace quota {
class QuotaManagerProxy;
}

namespace fileapi {

class FileSystemContext;
class FileSystemPathManager;
class FileSystemQuotaUtil;
class SandboxMountPointProvider;

struct DefaultContextDeleter;

// This class keeps and provides a file system context for FileSystem API.
class FileSystemContext
    : public base::RefCountedThreadSafe<FileSystemContext,
                                        DefaultContextDeleter> {
 public:
  FileSystemContext(
      scoped_refptr<base::MessageLoopProxy> file_message_loop,
      scoped_refptr<base::MessageLoopProxy> io_message_loop,
      scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy,
      quota::QuotaManagerProxy* quota_manager_proxy,
      const FilePath& profile_path,
      bool is_incognito,
      bool allow_file_access_from_files,
      FileSystemPathManager* path_manager);
  ~FileSystemContext();

  // This method can be called on any thread.
  bool DeleteDataForOriginOnFileThread(const GURL& origin_url);
  bool DeleteDataForOriginAndTypeOnFileThread(const GURL& origin_url,
                                              FileSystemType type);

  FileSystemPathManager* path_manager() const { return path_manager_.get(); }
  quota::QuotaManagerProxy* quota_manager_proxy() const {
    return quota_manager_proxy_.get();
  }

  // Returns a quota util for a given filesystem type.  This may
  // return NULL if the type does not support the usage tracking or
  // it is not a quota-managed storage.
  FileSystemQuotaUtil* GetQuotaUtil(FileSystemType type) const;

 private:
  friend struct DefaultContextDeleter;
  void DeleteOnCorrectThread() const;
  SandboxMountPointProvider* sandbox_provider() const;

  scoped_refptr<base::MessageLoopProxy> file_message_loop_;
  scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  scoped_refptr<quota::QuotaManagerProxy> quota_manager_proxy_;

  scoped_ptr<FileSystemPathManager> path_manager_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FileSystemContext);
};

struct DefaultContextDeleter {
  static void Destruct(const FileSystemContext* context) {
    context->DeleteOnCorrectThread();
  }
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_CONTEXT_H_
