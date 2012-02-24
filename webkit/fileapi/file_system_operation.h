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
  virtual void CreateFile(const GURL& path,
                          bool exclusive,
                          const StatusCallback& callback) OVERRIDE;
  virtual void CreateDirectory(const GURL& path,
                               bool exclusive,
                               bool recursive,
                               const StatusCallback& callback) OVERRIDE;
  virtual void Copy(const GURL& src_path,
                    const GURL& dest_path,
                    const StatusCallback& callback) OVERRIDE;
  virtual void Move(const GURL& src_path,
                    const GURL& dest_path,
                    const StatusCallback& callback) OVERRIDE;
  virtual void DirectoryExists(const GURL& path,
                               const StatusCallback& callback) OVERRIDE;
  virtual void FileExists(const GURL& path,
                          const StatusCallback& callback) OVERRIDE;
  virtual void GetMetadata(const GURL& path,
                           const GetMetadataCallback& callback) OVERRIDE;
  virtual void ReadDirectory(const GURL& path,
                             const ReadDirectoryCallback& callback) OVERRIDE;
  virtual void Remove(const GURL& path, bool recursive,
                      const StatusCallback& callback) OVERRIDE;
  virtual void Write(const net::URLRequestContext* url_request_context,
                     const GURL& path,
                     const GURL& blob_url,
                     int64 offset,
                     const WriteCallback& callback) OVERRIDE;
  virtual void Truncate(const GURL& path, int64 length,
                        const StatusCallback& callback) OVERRIDE;
  virtual void TouchFile(const GURL& path,
                         const base::Time& last_access_time,
                         const base::Time& last_modified_time,
                         const StatusCallback& callback) OVERRIDE;
  virtual void OpenFile(const GURL& path,
                        int file_flags,
                        base::ProcessHandle peer_handle,
                        const OpenFileCallback& callback) OVERRIDE;
  virtual void Cancel(const StatusCallback& cancel_callback) OVERRIDE;
  virtual FileSystemOperation* AsFileSystemOperation() OVERRIDE;

  // Synchronously gets the platform path for the given |path|.
  void SyncGetPlatformPath(const GURL& path, FilePath* platform_path);

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
    operation_context_.set_src_file_util(file_util);
    operation_context_.set_dest_file_util(file_util);
  }

  void GetUsageAndQuotaThenCallback(
      const GURL& origin_url,
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

  // Checks the validity of a given |path| for reading, cracks the path into
  // root URL and virtual path components, and returns the correct
  // FileSystemFileUtil subclass for this type.
  // Returns true if the given |path| is a valid FileSystem path.
  // Otherwise it calls dispatcher's DidFail method with
  // PLATFORM_FILE_ERROR_SECURITY and returns false.
  // (Note: this doesn't delete this when it calls DidFail and returns false;
  // it's the caller's responsibility.)
  base::PlatformFileError VerifyFileSystemPathForRead(
      const GURL& path,
      GURL* root_url,
      FileSystemType* type,
      FilePath* virtual_path,
      FileSystemFileUtil** file_util);

  // Checks the validity of a given |path| for writing, cracks the path into
  // root URL and virtual path components, and returns the correct
  // FileSystemFileUtil subclass for this type.
  // Returns true if the given |path| is a valid FileSystem path, and
  // its origin embedded in the path has the right to write.
  // Otherwise it fires dispatcher's DidFail method with
  // PLATFORM_FILE_ERROR_SECURITY if the path is not valid for writing,
  // or with PLATFORM_FILE_ERROR_NO_SPACE if the origin is not allowed to
  // write to the storage.
  // In either case it returns false after firing DidFail.
  // If |create| flag is true this also checks if the |path| contains
  // any restricted names and chars. If it does, the call fires dispatcher's
  // DidFail with PLATFORM_FILE_ERROR_SECURITY and returns false.
  // (Note: this doesn't delete this when it calls DidFail and returns false;
  // it's the caller's responsibility.)
  base::PlatformFileError VerifyFileSystemPathForWrite(
      const GURL& path,
      bool create,
      GURL* root_url,
      FileSystemType* type,
      FilePath* virtual_path,
      FileSystemFileUtil** file_util);

  // Common internal routine for VerifyFileSystemPathFor{Read,Write}.
  base::PlatformFileError VerifyFileSystemPath(const GURL& path,
                                               GURL* root_url,
                                               FileSystemType* type,
                                               FilePath* virtual_path,
                                               FileSystemFileUtil** file_util);

  // Setup*Context*() functions will call the appropriate VerifyFileSystem
  // function and store the results to operation_context_ and
  // *_virtual_path_.
  // Return the result of VerifyFileSystem*().
  base::PlatformFileError SetupSrcContextForRead(const GURL& path);
  base::PlatformFileError SetupSrcContextForWrite(const GURL& path,
                                                  bool create);
  base::PlatformFileError SetupDestContextForWrite(const GURL& path,
                                                   bool create);

  // Used only for internal assertions.
  // Returns false if there's another inflight pending operation.
  bool SetPendingOperationType(OperationType type);

  // Proxy for calling file_util_proxy methods.
  scoped_refptr<base::MessageLoopProxy> proxy_;

  FileSystemOperationContext operation_context_;

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

  // Used to keep a virtual path around while we check for quota.
  // If an operation needs only one path, use src_virtual_path_, even if it's a
  // write.
  FilePath src_virtual_path_;
  FilePath dest_virtual_path_;

  // A flag to make sure we call operation only once per instance.
  OperationType pending_operation_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemOperation);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_H_
