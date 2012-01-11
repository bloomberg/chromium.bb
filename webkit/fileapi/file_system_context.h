// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

class ExternalFileSystemMountPointProvider;
class FileSystemCallbackDispatcher;
class FileSystemContext;
class FileSystemFileUtil;
class FileSystemMountPointProvider;
class FileSystemOptions;
class FileSystemPathManager;
class FileSystemQuotaUtil;
class SandboxMountPointProvider;

struct DefaultContextDeleter;

// This class keeps and provides a file system context for FileSystem API.
// An instance of this class is created and owned by profile.
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
      const FileSystemOptions& options);
  ~FileSystemContext();

  // This method can be called on any thread.
  bool DeleteDataForOriginOnFileThread(const GURL& origin_url);
  bool DeleteDataForOriginAndTypeOnFileThread(const GURL& origin_url,
                                              FileSystemType type);

  quota::QuotaManagerProxy* quota_manager_proxy() const {
    return quota_manager_proxy_.get();
  }

  // Returns a quota util for a given filesystem type.  This may
  // return NULL if the type does not support the usage tracking or
  // it is not a quota-managed storage.
  FileSystemQuotaUtil* GetQuotaUtil(FileSystemType type) const;

  // Returns the appropriate FileUtil instance for the given |type|.
  // This may return NULL if it is given an invalid or unsupported filesystem
  // type.
  FileSystemFileUtil* GetFileUtil(FileSystemType type) const;

  // Returns the mount point provider instance for the given |type|.
  // This may return NULL if it is given an invalid or unsupported filesystem
  // type.
  FileSystemMountPointProvider* GetMountPointProvider(
      FileSystemType type) const;

  // Returns a FileSystemMountPointProvider instance for sandboxed filesystem
  // types (e.g. TEMPORARY or PERSISTENT).  This is equivalent to calling
  // GetMountPointProvider(kFileSystemType{Temporary, Persistent}).
  SandboxMountPointProvider* sandbox_provider() const;

  // Returns a FileSystemMountPointProvider instance for external filesystem
  // type, which is used only by chromeos for now.  This is equivalent to
  // calling GetMountPointProvider(kFileSystemTypeExternal).
  ExternalFileSystemMountPointProvider* external_provider() const;

  // Opens the filesystem for the given |origin_url| and |type|, and dispatches
  // the DidOpenFileSystem callback of the given |dispatcher|.
  // If |create| is true this may actually set up a filesystem instance
  // (e.g. by creating the root directory or initializing the database
  // entry etc).
  // TODO(kinuko): replace the dispatcher with a regular callback.
  void OpenFileSystem(
      const GURL& origin_url,
      FileSystemType type,
      bool create,
      scoped_ptr<FileSystemCallbackDispatcher> dispatcher);

 private:
  friend struct DefaultContextDeleter;
  void DeleteOnCorrectThread() const;

  scoped_refptr<base::MessageLoopProxy> file_message_loop_;
  scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  scoped_refptr<quota::QuotaManagerProxy> quota_manager_proxy_;

  // Mount point providers.
  scoped_ptr<SandboxMountPointProvider> sandbox_provider_;
  scoped_ptr<ExternalFileSystemMountPointProvider> external_provider_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FileSystemContext);
};

struct DefaultContextDeleter {
  static void Destruct(const FileSystemContext* context) {
    context->DeleteOnCorrectThread();
  }
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_CONTEXT_H_
