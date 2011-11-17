// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/process.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/quota/quota_manager.h"

namespace base {
class Time;
}

namespace net {
class URLRequest;
class URLRequestContext;
}  // namespace net

class GURL;

namespace fileapi {

class FileSystemCallbackDispatcher;
class FileSystemContext;
class FileWriterDelegate;
class FileSystemOperationTest;

// This class is designed to serve one-time file system operation per instance.
// Only one method(CreateFile, CreateDirectory, Copy, Move, DirectoryExists,
// GetMetadata, ReadDirectory and Remove) may be called during the lifetime of
// this object and it should be called no more than once.
// This class is self-destructed, or get deleted via base::Owned() fater the
// operation finishes and completion callback is called.
class FileSystemOperation {
 public:
  // |dispatcher| will be owned by this class.
  FileSystemOperation(FileSystemCallbackDispatcher* dispatcher,
                      scoped_refptr<base::MessageLoopProxy> proxy,
                      FileSystemContext* file_system_context);
  virtual ~FileSystemOperation();

  void OpenFileSystem(const GURL& origin_url,
                      fileapi::FileSystemType type,
                      bool create);
  void CreateFile(const GURL& path,
                  bool exclusive);
  void CreateDirectory(const GURL& path,
                       bool exclusive,
                       bool recursive);
  void Copy(const GURL& src_path,
            const GURL& dest_path);
  void Move(const GURL& src_path,
            const GURL& dest_path);
  void DirectoryExists(const GURL& path);
  void FileExists(const GURL& path);
  void GetMetadata(const GURL& path);
  void ReadDirectory(const GURL& path);
  void Remove(const GURL& path, bool recursive);
  void Write(scoped_refptr<net::URLRequestContext> url_request_context,
             const GURL& path,
             const GURL& blob_url,
             int64 offset);
  void Truncate(const GURL& path, int64 length);
  void TouchFile(const GURL& path,
                 const base::Time& last_access_time,
                 const base::Time& last_modified_time);
  void OpenFile(
      const GURL& path,
      int file_flags,
      base::ProcessHandle peer_handle);
  void SyncGetPlatformPath(const GURL& path, FilePath* platform_path);

  // Try to cancel the current operation [we support cancelling write or
  // truncate only].  Report failure for the current operation, then tell the
  // passed-in operation to report success.
  void Cancel(FileSystemOperation* cancel_operation);

 private:
  class ScopedQuotaUtilHelper;

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

  void DelayedCreateFileForQuota(bool exclusive,
                                 quota::QuotaStatusCode status,
                                 int64 usage, int64 quota);
  void DelayedCreateDirectoryForQuota(bool exclusive, bool recursive,
                                      quota::QuotaStatusCode status,
                                      int64 usage, int64 quota);
  void DelayedCopyForQuota(quota::QuotaStatusCode status,
                           int64 usage, int64 quota);
  void DelayedMoveForQuota(quota::QuotaStatusCode status,
                           int64 usage, int64 quota);
  void DelayedWriteForQuota(quota::QuotaStatusCode status,
                            int64 usage, int64 quota);
  void DelayedTruncateForQuota(int64 length,
                               quota::QuotaStatusCode status,
                               int64 usage, int64 quota);
  void DelayedOpenFileForQuota(int file_flags,
                               quota::QuotaStatusCode status,
                               int64 usage, int64 quota);

  // A callback used for OpenFileSystem.
  void DidGetRootPath(bool success,
                      const FilePath& path,
                      const std::string& name);

  // Callback for CreateFile for |exclusive|=true cases.
  void DidEnsureFileExistsExclusive(base::PlatformFileError rv,
                                    bool created);

  // Callback for CreateFile for |exclusive|=false cases.
  void DidEnsureFileExistsNonExclusive(base::PlatformFileError rv,
                                       bool created);

  // Generic callback that translates platform errors to WebKit error codes.
  void DidFinishFileOperation(base::PlatformFileError rv);

  void DidDirectoryExists(base::PlatformFileError rv,
                          const base::PlatformFileInfo& file_info,
                          const FilePath& unused);
  void DidFileExists(base::PlatformFileError rv,
                     const base::PlatformFileInfo& file_info,
                     const FilePath& unused);
  void DidGetMetadata(base::PlatformFileError rv,
                      const base::PlatformFileInfo& file_info,
                      const FilePath& platform_path);
  void DidReadDirectory(
      base::PlatformFileError rv,
      const std::vector<base::FileUtilProxy::Entry>& entries);
  void DidWrite(
      base::PlatformFileError rv,
      int64 bytes,
      bool complete);
  void DidTouchFile(base::PlatformFileError rv);
  void DidOpenFile(
      base::PlatformFileError rv,
      base::PassPlatformFile file,
      bool created);

  // Helper for Write().
  void OnFileOpenedForWrite(
      base::PlatformFileError rv,
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
  bool VerifyFileSystemPathForRead(const GURL& path,
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
  bool VerifyFileSystemPathForWrite(const GURL& path,
                                    bool create,
                                    GURL* root_url,
                                    FileSystemType* type,
                                    FilePath* virtual_path,
                                    FileSystemFileUtil** file_util);

  // Common internal routine for VerifyFileSystemPathFor{Read,Write}.
  bool VerifyFileSystemPath(const GURL& path,
                            GURL* root_url,
                            FileSystemType* type,
                            FilePath* virtual_path,
                            FileSystemFileUtil** file_util);

  // Setup*Context*() functions will call the appropriate VerifyFileSystem
  // function and store the results to operation_context_ and
  // *_virtual_path_.
  // Return the result of VerifyFileSystem*().
  bool SetupSrcContextForRead(const GURL& path);
  bool SetupSrcContextForWrite(const GURL& path, bool create);
  bool SetupDestContextForWrite(const GURL& path, bool create);

#ifndef NDEBUG
  enum OperationType {
    kOperationNone,
    kOperationOpenFileSystem,
    kOperationCreateFile,
    kOperationCreateDirectory,
    kOperationCopy,
    kOperationMove,
    kOperationDirectoryExists,
    kOperationFileExists,
    kOperationGetMetadata,
    kOperationReadDirectory,
    kOperationRemove,
    kOperationWrite,
    kOperationTruncate,
    kOperationTouchFile,
    kOperationOpenFile,
    kOperationGetLocalPath,
    kOperationCancel,
  };

  // A flag to make sure we call operation only once per instance.
  OperationType pending_operation_;
#endif

  // Proxy for calling file_util_proxy methods.
  scoped_refptr<base::MessageLoopProxy> proxy_;

  // This can be NULL if the operation is cancelled on the way.
  scoped_ptr<FileSystemCallbackDispatcher> dispatcher_;

  FileSystemOperationContext operation_context_;

  scoped_ptr<ScopedQuotaUtilHelper> quota_util_helper_;

  // These are all used only by Write().
  friend class FileWriterDelegate;
  scoped_ptr<FileWriterDelegate> file_writer_delegate_;
  scoped_ptr<net::URLRequest> blob_request_;
  scoped_ptr<FileSystemOperation> cancel_operation_;

  // Used only by OpenFile, in order to clone the file handle back to the
  // requesting process.
  base::ProcessHandle peer_handle_;

  // Used to keep a virtual path around while we check for quota.
  // If an operation needs only one path, use src_virtual_path_, even if it's a
  // write.
  FilePath src_virtual_path_;
  FilePath dest_virtual_path_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemOperation);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_H_
