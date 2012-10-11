// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_CANNED_SYNCABLE_FILE_SYSTEM_H_
#define WEBKIT_FILEAPI_SYNCABLE_CANNED_SYNCABLE_FILE_SYSTEM_H_

#include <string>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/scoped_temp_dir.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/local_file_system_test_helper.h"
#include "webkit/fileapi/syncable/sync_status_code.h"

namespace base {
class Thread;
class MessageLoopProxy;
}

namespace quota {
class QuotaManager;
}

namespace fileapi {

class FileSystemContext;

// A canned syncable filesystem for testing.
// This internally creates its own QuotaManager and FileSystemContext
// (as we do so for each isolated application).
class CannedSyncableFileSystem {
 public:
  CannedSyncableFileSystem(const GURL& origin, const std::string& service);
  ~CannedSyncableFileSystem();

  // SetUp must be called before using this instance.
  void SetUp();

  // TearDown must be called before destructing this instance.
  void TearDown();

  // Creates a FileSystemURL for the given (utf8) path string.
  FileSystemURL URL(const std::string& path) const;

  // Opens a new syncable file system.
  base::PlatformFileError OpenFileSystem();

  // Accessors.
  FileSystemContext* file_system_context() {
    return file_system_context_.get();
  }
  quota::QuotaManager* quota_manager() { return quota_manager_.get(); }
  GURL origin() const { return test_helper_.origin(); }
  FileSystemType type() const { return test_helper_.type(); }
  quota::StorageType storage_type() const {
    return test_helper_.storage_type();
  }

  // Helper routines to perform file system operations.
  // (They run on the current thread and returns synchronously).
  base::PlatformFileError CreateDirectory(const FileSystemURL& url);
  base::PlatformFileError CreateFile(const FileSystemURL& url);
  base::PlatformFileError Copy(const FileSystemURL& src_url,
                               const FileSystemURL& dest_url);
  base::PlatformFileError Move(const FileSystemURL& src_url,
                               const FileSystemURL& dest_url);
  base::PlatformFileError TruncateFile(const FileSystemURL& url, int64 size);
  base::PlatformFileError Remove(const FileSystemURL& url, bool recursive);

  // Pruges the file system local storage.
  base::PlatformFileError DeleteFileSystem();

 private:
  // Callbacks.
  void DidOpenFileSystem(base::PlatformFileError result,
                         const std::string& name,
                         const GURL& root);
  void StatusCallback(base::PlatformFileError result);

  FileSystemOperationContext* NewOperationContext();

  ScopedTempDir data_dir_;
  const std::string service_name_;

  scoped_refptr<quota::QuotaManager> quota_manager_;
  scoped_refptr<FileSystemContext> file_system_context_;
  LocalFileSystemTestOriginHelper test_helper_;
  GURL root_url_;
  base::PlatformFileError result_;
  SyncStatusCode sync_status_;

  base::WeakPtrFactory<CannedSyncableFileSystem> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CannedSyncableFileSystem);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SYNCABLE_CANNED_SYNCABLE_FILE_SYSTEM_H_
