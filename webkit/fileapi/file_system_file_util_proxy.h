// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_PROXY_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_PROXY_H_

#include <vector>

#include "base/callback.h"
#include "base/callback_old.h"
#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "base/tracked_objects.h"

namespace base {
class MessageLoopProxy;
class Time;
}

namespace fileapi {

class FileSystemOperationContext;

using base::MessageLoopProxy;
using base::PlatformFile;
using base::PlatformFileError;
using base::PlatformFileInfo;

// This class provides asynchronous access to common file routines for the
// FileSystem API.
class FileSystemFileUtilProxy {
 public:
  typedef base::FileUtilProxy::Entry Entry;

  typedef base::FileUtilProxy::StatusCallback StatusCallback;
  typedef base::Callback<void(PlatformFileError,
                              bool /* created */
                              )> EnsureFileExistsCallback;
  typedef base::Callback<void(PlatformFileError,
                              const PlatformFileInfo&,
                              const FilePath& /* platform_path */
                              )> GetFileInfoCallback;
  typedef base::Callback<void(PlatformFileError,
                              const std::vector<Entry>&
                              )> ReadDirectoryCallback;

  typedef Callback2<PlatformFileError /* error code */,
                    const FilePath& /* local_path, where possible */
                    >::Type GetLocalPathCallback;

  // Ensures that the given |file_path| exist.  This creates a empty new file
  // at |file_path| if the |file_path| does not exist.
  // If a new file han not existed and is created at the |file_path|,
  // |created| of the callback argument is set true and |error code|
  // is set PLATFORM_FILE_OK.
  // If the file already exists, |created| is set false and |error code|
  // is set PLATFORM_FILE_OK.
  // If the file hasn't existed but it couldn't be created for some other
  // reasons, |created| is set false and |error code| indicates the error.
  static bool EnsureFileExists(
      const FileSystemOperationContext& context,
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const FilePath& file_path,
      const EnsureFileExistsCallback& callback);

  // Maps virtual file patch to its local physical location.
  static bool GetLocalPath(
      const FileSystemOperationContext& context,
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const FilePath& virtual_path,
      GetLocalPathCallback* callback);

  // Retrieves the information about a file. It is invalid to pass NULL for the
  // callback.
  static bool GetFileInfo(
      const FileSystemOperationContext& context,
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const FilePath& file_path,
      const GetFileInfoCallback& callback);

  static bool ReadDirectory(const FileSystemOperationContext& context,
                            scoped_refptr<MessageLoopProxy> message_loop_proxy,
                            const FilePath& file_path,
                            const ReadDirectoryCallback& callback);

  // Creates directory at given path. It's an error to create
  // if |exclusive| is true and dir already exists.
  static bool CreateDirectory(
      const FileSystemOperationContext& context,
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const FilePath& file_path,
      bool exclusive,
      bool recursive,
      const StatusCallback& callback);

  // Copies a file or a directory from |src_file_path| to |dest_file_path|
  // Error cases:
  // If destination file doesn't exist or destination's parent
  // doesn't exists.
  // If source dir exists but destination path is an existing file.
  // If source file exists but destination path is an existing directory.
  // If source is a parent of destination.
  // If source doesn't exists.
  static bool Copy(const FileSystemOperationContext& context,
                   scoped_refptr<MessageLoopProxy> message_loop_proxy,
                   const FilePath& src_file_path,
                   const FilePath& dest_file_path,
                   const StatusCallback& callback);

  // Moves a file or a directory from src_file_path to dest_file_path.
  // Error cases are similar to Copy method's error cases.
  static bool Move(
      const FileSystemOperationContext& context,
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const FilePath& src_file_path,
      const FilePath& dest_file_path,
      const StatusCallback& callback);

  // Deletes a file or a directory.
  // It is an error to delete a non-empty directory with recursive=false.
  static bool Delete(const FileSystemOperationContext& context,
                     scoped_refptr<MessageLoopProxy> message_loop_proxy,
                     const FilePath& file_path,
                     bool recursive,
                     const StatusCallback& callback);

  // Touches a file. The callback can be NULL.
  static bool Touch(
      const FileSystemOperationContext& context,
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const FilePath& file_path,
      const base::Time& last_access_time,
      const base::Time& last_modified_time,
      const StatusCallback& callback);

  // Truncates a file to the given length. If |length| is greater than the
  // current length of the file, the file will be extended with zeroes.
  // The callback can be NULL.
  static bool Truncate(
      const FileSystemOperationContext& context,
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const FilePath& path,
      int64 length,
      const StatusCallback& callback);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FileSystemFileUtilProxy);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_PROXY_H_
