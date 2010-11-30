// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SANDBOXED_FILE_SYSTEM_OPERATION_H_
#define WEBKIT_FILEAPI_SANDBOXED_FILE_SYSTEM_OPERATION_H_

#include "base/scoped_callback_factory.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_types.h"

namespace fileapi {

class SandboxedFileSystemContext;

// This class provides a 'sandboxed' access to the underlying file system,
// that is:
// 1. provides OpenFileSystem method that returns a (hidden) root path
//    that is given by |file_system_context|.
// 2. enforces quota and file names/paths restrictions on each operation
//    via |file_system_context|.
class SandboxedFileSystemOperation : public FileSystemOperation {
 public:
  // This class doesn't hold a reference or ownership of |file_system_context|.
  // It is the caller's responsibility to keep the pointer alive *until*
  // it calls any of the operation methods.  The |file_system_context| won't be
  // used in the callback path and can be deleted after the operation is
  // made (e.g. after one of CreateFile, CreateDirectory, Copy, etc is called).
  SandboxedFileSystemOperation(FileSystemCallbackDispatcher* dispatcher,
                               scoped_refptr<base::MessageLoopProxy> proxy,
                               SandboxedFileSystemContext* file_system_context);

  void OpenFileSystem(const GURL& origin_url,
                      fileapi::FileSystemType type,
                      bool create);

  // FileSystemOperation's methods.
  virtual void CreateFile(const FilePath& path,
                          bool exclusive);
  virtual void CreateDirectory(const FilePath& path,
                               bool exclusive,
                               bool recursive);
  virtual void Copy(const FilePath& src_path,
                    const FilePath& dest_path);
  virtual void Move(const FilePath& src_path,
                    const FilePath& dest_path);
  virtual void DirectoryExists(const FilePath& path);
  virtual void FileExists(const FilePath& path);
  virtual void GetMetadata(const FilePath& path);
  virtual void ReadDirectory(const FilePath& path);
  virtual void Remove(const FilePath& path, bool recursive);
  virtual void Write(
      scoped_refptr<URLRequestContext> url_request_context,
      const FilePath& path, const GURL& blob_url, int64 offset);
  virtual void Truncate(const FilePath& path, int64 length);
  virtual void TouchFile(const FilePath& path,
                 const base::Time& last_access_time,
                 const base::Time& last_modified_time);

 private:
  enum SandboxedFileSystemOperationType {
    kOperationOpenFileSystem = 100,
  };

  // A callback used for OpenFileSystem.
  void DidGetRootPath(bool success,
                      const FilePath& path,
                      const std::string& name);

  // Checks the validity of a given |path| for reading.
  // Returns true if the given |path| is a valid FileSystem path.
  // Otherwise it calls dispatcher's DidFail method with
  // PLATFORM_FILE_ERROR_SECURITY and returns false.
  // (Note: this doesn't delete this when it calls DidFail and returns false;
  // it's the caller's responsibility.)
  bool VerifyFileSystemPathForRead(const FilePath& path);

  // Checks the validity of a given |path| for writing.
  // Returns true if the given |path| is a valid FileSystem path, and
  // its origin embedded in the path has the right to write as much as
  // the given |growth|.
  // Otherwise it fires dispatcher's DidFail method with
  // PLATFORM_FILE_ERROR_SECURITY if the path is not valid for writing,
  // or with PLATFORM_FILE_ERROR_NO_SPACE if the origin is not allowed to
  // increase the usage by |growth|.
  // In either case it returns false after firing DidFail.
  // If |create| flag is true this also checks if the |path| contains
  // any restricted names and chars. If it does, the call fires dispatcher's
  // DidFail with PLATFORM_FILE_ERROR_SECURITY and returns false.
  // (Note: this doesn't delete this when it calls DidFail and returns false;
  // it's the caller's responsibility.)
  bool VerifyFileSystemPathForWrite(const FilePath& path,
                                    bool create,
                                    int64 growth);

  // Not owned.  See the comment at the constructor.
  SandboxedFileSystemContext* file_system_context_;

  base::ScopedCallbackFactory<SandboxedFileSystemOperation> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(SandboxedFileSystemOperation);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SANDBOXED_FILE_SYSTEM_OPERATION_H_
