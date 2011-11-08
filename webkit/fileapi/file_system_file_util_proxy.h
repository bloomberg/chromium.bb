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

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FileSystemFileUtilProxy);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_PROXY_H_
