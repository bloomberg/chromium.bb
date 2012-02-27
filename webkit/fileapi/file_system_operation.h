// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_H_

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/process.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_operation_interface.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/quota/quota_manager.h"

namespace base {
class Time;
}

namespace chromeos {
class CrosMountPointProvider;
}

namespace net {
class URLRequest;
class URLRequestContext;
}  // namespace net

class GURL;

namespace fileapi {

class FileSystemContext;
class FileWriterDelegate;
class FileSystemOperationTest;

// FileSystemOperation implementation for local file systems.
class FileSystemOperation : public FileSystemOperationInterface {
 public:
  virtual ~FileSystemOperation();

  // FileSystemOperation overrides.
  virtual void CreateFile(const GURL& path_url,
                          bool exclusive,
                          const StatusCallback& callback) OVERRIDE;
  virtual void CreateDirectory(const GURL& path_url,
                               bool exclusive,
                               bool recursive,
                               const StatusCallback& callback) OVERRIDE;
  virtual void Copy(const GURL& src_path_url,
                    const GURL& dest_path_url,
                    const StatusCallback& callback) OVERRIDE;
  virtual void Move(const GURL& src_path_url,
                    const GURL& dest_path_url,
                    const StatusCallback& callback) OVERRIDE;
  virtual void DirectoryExists(const GURL& path_url,
                               const StatusCallback& callback) OVERRIDE;
  virtual void FileExists(const GURL& path_url,
                          const StatusCallback& callback) OVERRIDE;
  virtual void GetMetadata(const GURL& path_url,
                           const GetMetadataCallback& callback) OVERRIDE;
  virtual void ReadDirectory(const GURL& path_url,
                             const ReadDirectoryCallback& callback) OVERRIDE;
  virtual void Remove(const GURL& path_url, bool recursive,
                      const StatusCallback& callback) OVERRIDE;
  virtual void Write(const net::URLRequestContext* url_request_context,
                     const GURL& path_url,
                     const GURL& blob_url,
                     int64 offset,
                     const WriteCallback& callback) OVERRIDE;
  virtual void Truncate(const GURL& path_url, int64 length,
                        const StatusCallback& callback) OVERRIDE;
  virtual void TouchFile(const GURL& path_url,
                         const base::Time& last_access_time,
                         const base::Time& last_modified_time,
                         const StatusCallback& callback) OVERRIDE;
  virtual void OpenFile(const GURL& path_url,
                        int file_flags,
                        base::ProcessHandle peer_handle,
                        const OpenFileCallback& callback) OVERRIDE;
  virtual void Cancel(const StatusCallback& cancel_callback) OVERRIDE;
  virtual FileSystemOperation* AsFileSystemOperation() OVERRIDE;

  // Synchronously gets the platform path for the given |path_url|.
  void SyncGetPlatformPath(const GURL& path_url, FilePath* platform_path);

 protected:
  class ScopedQuotaUtilHelper;

  // Only MountPointProviders or testing class can create a
  // new operation directly.
  friend class SandboxMountPointProvider;
  friend class FileSystemTestHelper;
  friend class chromeos::CrosMountPointProvider;

  FileSystemOperation(scoped_refptr<base::MessageLoopProxy> proxy,
                      FileSystemContext* file_system_context);

  FileSystemContext* file_system_context() const {
    return operation_context_.file_system_context();
  }

  FileSystemOperationContext* file_system_operation_context() {
    return &operation_context_;
  }

  friend class FileSystemOperationTest;
  friend class FileSystemOperationWriteTest;
  friend class FileWriterDelegateTest;
  friend class FileSystemTestOriginHelper;
  friend class FileSystemQuotaTest;

  // The unit tests that need to specify and control the lifetime of the
  // file_util on their own should call this before performing the actual
  // operation. If it is given it will not be overwritten by the class.
  void set_override_file_util(FileSystemFileUtil* file_util) {
    src_util_ = file_util;
    dest_util_ = file_util;
  }

  void GetUsageAndQuotaThenCallback(
      const GURL& origin,
      FileSystemType type,
      const quota::QuotaManager::GetUsageAndQuotaCallback& callback);

  void DelayedCreateFileForQuota(const StatusCallback& callback,
                                 bool exclusive,
                                 quota::QuotaStatusCode status,
                                 int64 usage, int64 quota);
  void DelayedCreateDirectoryForQuota(const StatusCallback& callback,
                                      bool exclusive, bool recursive,
                                      quota::QuotaStatusCode status,
                                      int64 usage, int64 quota);
  void DelayedCopyForQuota(const StatusCallback& callback,
                           quota::QuotaStatusCode status,
                           int64 usage, int64 quota);
  void DelayedMoveForQuota(const StatusCallback& callback,
                           quota::QuotaStatusCode status,
                           int64 usage, int64 quota);
  void DelayedWriteForQuota(quota::QuotaStatusCode status,
                            int64 usage, int64 quota);
  void DelayedTruncateForQuota(const StatusCallback& callback,
                               int64 length,
                               quota::QuotaStatusCode status,
                               int64 usage, int64 quota);
  void DelayedOpenFileForQuota(const OpenFileCallback& callback,
                               int file_flags,
                               quota::QuotaStatusCode status,
                               int64 usage, int64 quota);

  // Callback for CreateFile for |exclusive|=true cases.
  void DidEnsureFileExistsExclusive(const StatusCallback& callback,
                                    base::PlatformFileError rv,
                                    bool created);

  // Callback for CreateFile for |exclusive|=false cases.
  void DidEnsureFileExistsNonExclusive(const StatusCallback& callback,
                                       base::PlatformFileError rv,
                                       bool created);

  // Generic callback that translates platform errors to WebKit error codes.
  void DidFinishFileOperation(const StatusCallback& callback,
                              base::PlatformFileError rv);

  void DidDirectoryExists(const StatusCallback& callback,
                          base::PlatformFileError rv,
                          const base::PlatformFileInfo& file_info,
                          const FilePath& unused);
  void DidFileExists(const StatusCallback& callback,
                     base::PlatformFileError rv,
                     const base::PlatformFileInfo& file_info,
                     const FilePath& unused);
  void DidGetMetadata(const GetMetadataCallback& callback,
                      base::PlatformFileError rv,
                      const base::PlatformFileInfo& file_info,
                      const FilePath& platform_path);
  void DidReadDirectory(
      const ReadDirectoryCallback& callback,
      base::PlatformFileError rv,
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more);
  void DidWrite(base::PlatformFileError rv,
                int64 bytes,
                bool complete);
  void DidTouchFile(const StatusCallback& callback,
                    base::PlatformFileError rv);
  void DidOpenFile(const OpenFileCallback& callback,
                   base::PlatformFileError rv,
                   base::PassPlatformFile file,
                   bool created);

  // Helper for Write().
  void OnFileOpenedForWrite(base::PlatformFileError rv,
                            base::PassPlatformFile file,
                            bool created);

  enum SetUpPathMode {
    PATH_FOR_READ,
    PATH_FOR_WRITE,
    PATH_FOR_CREATE,
  };

  // Checks the validity of a given |path_url| and and populates
  // |path| and |file_util| for |mode|.
  base::PlatformFileError SetUpFileSystemPath(
      const GURL& path_url,
      FileSystemPath* file_system_path,
      FileSystemFileUtil** file_util,
      SetUpPathMode mode);

  // Used only for internal assertions.
  // Returns false if there's another inflight pending operation.
  bool SetPendingOperationType(OperationType type);

  // Proxy for calling file_util_proxy methods.
  scoped_refptr<base::MessageLoopProxy> proxy_;

  FileSystemOperationContext operation_context_;
  FileSystemPath src_path_;
  FileSystemPath dest_path_;
  FileSystemFileUtil* src_util_;  // Not owned.
  FileSystemFileUtil* dest_util_;  // Not owned.

  scoped_ptr<ScopedQuotaUtilHelper> quota_util_helper_;

  // These are all used only by Write().
  friend class FileWriterDelegate;
  scoped_ptr<FileWriterDelegate> file_writer_delegate_;
  scoped_ptr<net::URLRequest> blob_request_;

  // write_callback is kept in this class for so that we can dispatch it when
  // the operation is cancelled. calcel_callback is kept for canceling a
  // Truncate() operation. We can't actually stop Truncate in another thread;
  // after it resumed from the working thread, cancellation takes place.
  WriteCallback write_callback_;
  StatusCallback cancel_callback_;
  void set_write_callback(const WriteCallback& write_callback) {
    write_callback_ = write_callback;
  }

  // Used only by OpenFile, in order to clone the file handle back to the
  // requesting process.
  base::ProcessHandle peer_handle_;

  // A flag to make sure we call operation only once per instance.
  OperationType pending_operation_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemOperation);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_H_
