// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_INTERFACE_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_INTERFACE_H_

#include "base/process.h"

namespace base {
class Time;
}  // namespace base

namespace net {
class URLRequest;
class URLRequestContext;
}  // namespace net

class GURL;

namespace fileapi {

class FileSystemCallbackDispatcher;

// The interface class for FileSystemOperation implementations.
//
// This interface defines file system operations required to implement
// "File API: Directories and System"
// http://www.w3.org/TR/file-system-api/
//
// DESIGN NOTES
//
// This class is designed to
//
// 1) Serve one-time file system operation per instance.  Only one
// method(CreateFile, CreateDirectory, Copy, Move, DirectoryExists,
// GetMetadata, ReadDirectory and Remove) may be called during the
// lifetime of this object and it should be called no more than once.
//
// 2) Be self-destructed, or get deleted via base::Owned() after the
// operation finishes and completion callback is called.
//
// 3) Deliver the results of operations to the client via
// FileSystemCallbackDispatcher.
// TODO(kinuko): Change the functions to take callbacks instead.
//
class FileSystemOperationInterface {
 public:
  virtual ~FileSystemOperationInterface() {}

  // Creates a file at |path|. If |exclusive| is true, an error is raised
  // in case a file is already present at the URL.
  virtual void CreateFile(const GURL& path,
                          bool exclusive) = 0;

  // Creates a directory at |path|. If |exclusive| is true, an error is
  // raised in case a directory is already present at the URL. If
  // |recursive| is true, create parent directories as needed just like
  // mkdir -p does.
  virtual void CreateDirectory(const GURL& path,
                               bool exclusive,
                               bool recursive) = 0;

  // Copes a file or directory from |src_path| to |dest_path|. If
  // |src_path| is a directory, the contents of |src_path| are copied to
  // |dest_path| recursively. A new file or directory is created at
  // |dest_path| as needed.
  virtual void Copy(const GURL& src_path,
                    const GURL& dest_path) = 0;

  // Moves a file or directory from |src_path| to |dest_path|. A new file
  // or directory is created at |dest_path| as needed.
  virtual void Move(const GURL& src_path,
                    const GURL& dest_path) = 0;

  // Checks if a directory is present at |path|.
  virtual void DirectoryExists(const GURL& path) = 0;

  // Checks if a file is present at |path|.
  virtual void FileExists(const GURL& path) = 0;

  // Gets the metadata of a file or directory at |path|.
  virtual void GetMetadata(const GURL& path) = 0;

  // Reads contents of a directory at |path|.
  virtual void ReadDirectory(const GURL& path) = 0;

  // Removes a file or directory at |path|. If |recursive| is true, remove
  // all files and directories under the directory at |path| recursively.
  virtual void Remove(const GURL& path, bool recursive) = 0;

  // Writes contents of |blob_url| to |path| at |offset|.
  // |url_request_context| is used to read contents in |blob_url|.
  virtual void Write(const net::URLRequestContext* url_request_context,
                     const GURL& path,
                     const GURL& blob_url,
                     int64 offset) = 0;

  // Truncates a file at |path| to |length|. If |length| is larger than
  // the original file size, the file will be extended, and the extended
  // part is filled with null bytes.
  virtual void Truncate(const GURL& path, int64 length) = 0;

  // Tries to cancel the current operation [we support cancelling write or
  // truncate only]. Reports failure for the current operation, then reports
  // success for the cancel operation itself via the |cancel_dispatcher|.
  //
  // E.g. a typical cancel implementation would look like:
  //
  //   virtual void SomeOperationImpl::Cancel(
  //       scoped_ptr<FileSystemCallbackDispatcher> cancel_dispatcher) {
  //     // Abort the current inflight operation first.
  //     ...
  //
  //     // Dispatch ABORT error for the current operation by calling
  //     // DidFail() callback of the dispatcher attached to this operation.
  //     // (dispatcher_ in this example)
  //     dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_ABORT);
  //
  //     // Dispatch 'success' for the cancel (or dispatch appropriate
  //     // error code with DidFail() if the cancel has somehow failed).
  //     cancel_dispatcher->DidSucceed();
  //   }
  //
  virtual void Cancel(
      scoped_ptr<FileSystemCallbackDispatcher> cancel_dispatcher) = 0;

  // Modifies timestamps of a file or directory at |path| with
  // |last_access_time| and |last_modified_time|. The function DOES NOT
  // create a file unlike 'touch' command on Linux.
  //
  // This function is used only by Pepper as of writing.
  virtual void TouchFile(const GURL& path,
                         const base::Time& last_access_time,
                         const base::Time& last_modified_time) = 0;

  // Opens a file at |path| with |file_flags|, where flags are OR'ed
  // values of base::PlatformFileFlags.
  //
  // |peer_handle| is the process handle of a pepper plugin process, which
  // is necessary for underlying IPC calls with Pepper plugins.
  //
  // This function is used only by Pepper as of writing.
  virtual void OpenFile(
      const GURL& path,
      int file_flags,
      base::ProcessHandle peer_handle) = 0;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_OPERATION_INTERFACE_H_
