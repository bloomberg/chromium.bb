// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_H_

#include <vector>

#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/ref_counted.h"
#include "base/scoped_callback_factory.h"
#include "base/scoped_ptr.h"
#include "googleurl/src/gurl.h"

namespace base {
class Time;
}

class GURL;
class URLRequest;
class URLRequestContext;

namespace fileapi {

class FileSystemCallbackDispatcher;
class FileWriterDelegate;

// This class is designed to serve one-time file system operation per instance.
// Only one method(CreateFile, CreateDirectory, Copy, Move, DirectoryExists,
// GetMetadata, ReadDirectory and Remove) may be called during the lifetime of
// this object and it should be called no more than once.
class FileSystemOperation {
 public:
  FileSystemOperation(FileSystemCallbackDispatcher* dispatcher,
                      scoped_refptr<base::MessageLoopProxy> proxy);
  ~FileSystemOperation();

  void CreateFile(const FilePath& path,
                  bool exclusive);

  void CreateDirectory(const FilePath& path,
                       bool exclusive,
                       bool recursive);

  void Copy(const FilePath& src_path,
            const FilePath& dest_path);

  void Move(const FilePath& src_path,
            const FilePath& dest_path);

  void DirectoryExists(const FilePath& path);

  void FileExists(const FilePath& path);

  void GetMetadata(const FilePath& path);

  void ReadDirectory(const FilePath& path);

  void Remove(const FilePath& path, bool recursive);

  void Write(
      scoped_refptr<URLRequestContext> url_request_context,
      const FilePath& path, const GURL& blob_url, int64 offset);

  void Truncate(const FilePath& path, int64 length);

  void TouchFile(const FilePath& path,
                 const base::Time& last_access_time,
                 const base::Time& last_modified_time);

  // Try to cancel the current operation [we support cancelling write or
  // truncate only].  Report failure for the current operation, then tell the
  // passed-in operation to report success.
  void Cancel(FileSystemOperation* cancel_operation);

 protected:
  // Proxy for calling file_util_proxy methods.
  scoped_refptr<base::MessageLoopProxy> proxy_;

 private:
  // Callbacks for above methods.
  void DidCreateFileExclusive(
      base::PlatformFileError rv, base::PassPlatformFile file, bool created);

  // Returns success even if the file already existed.
  void DidCreateFileNonExclusive(
      base::PlatformFileError rv, base::PassPlatformFile file, bool created);

  // Generic callback that translates platform errors to WebKit error codes.
  void DidFinishFileOperation(base::PlatformFileError rv);

  void DidDirectoryExists(base::PlatformFileError rv,
                          const base::PlatformFileInfo& file_info);

  void DidFileExists(base::PlatformFileError rv,
                     const base::PlatformFileInfo& file_info);

  void DidGetMetadata(base::PlatformFileError rv,
                      const base::PlatformFileInfo& file_info);

  void DidReadDirectory(
      base::PlatformFileError rv,
      const std::vector<base::file_util_proxy::Entry>& entries);

  void DidWrite(
      base::PlatformFileError rv,
      int64 bytes,
      bool complete);

  void DidTouchFile(base::PlatformFileError rv);

  // Helper for Write().
  void OnFileOpenedForWrite(
      base::PlatformFileError rv,
      base::PassPlatformFile file,
      bool created);

  scoped_ptr<FileSystemCallbackDispatcher> dispatcher_;
  base::ScopedCallbackFactory<FileSystemOperation> callback_factory_;

  // These are all used only by Write().
  friend class FileWriterDelegate;
  scoped_ptr<FileWriterDelegate> file_writer_delegate_;
  scoped_ptr<URLRequest> blob_request_;
  FileSystemOperation* cancel_operation_;

#ifndef NDEBUG
  enum OperationType {
    kOperationNone,
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
    kOperationCancel,
  };

  // A flag to make sure we call operation only once per instance.
  OperationType pending_operation_;
#endif

  DISALLOW_COPY_AND_ASSIGN(FileSystemOperation);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_H_
