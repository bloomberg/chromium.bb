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
}

namespace fileapi {

using base::MessageLoopProxy;
using base::PlatformFile;
using base::PlatformFileError;
using base::PlatformFileInfo;

// This class provides relay methods for supporting asynchronous access to
// FileSystem API operations.  (Most of necessary relay methods are provided
// by base::FileUtilProxy, but there are a few operations that are not
// covered or are slightly different from the version of base::FileUtilProxy.
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

  typedef base::Callback<PlatformFileError(bool* /* created */
                                           )> EnsureFileExistsTask;
  typedef base::Callback<PlatformFileError(PlatformFileInfo*,
                                           FilePath*)> GetFileInfoTask;
  typedef base::Callback<PlatformFileError(std::vector<Entry>*
                                           )> ReadDirectoryTask;

  // Calls EnsureFileExistsTask |task| on the given |message_loop_proxy|.
  static bool RelayEnsureFileExists(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const EnsureFileExistsTask& task,
      const EnsureFileExistsCallback& callback);

  // Calls GetFileInfoTask |task| on the given |message_loop_proxy|.
  static bool RelayGetFileInfo(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const GetFileInfoTask& task,
      const GetFileInfoCallback& callback);

  // Calls ReadDirectoryTask |task| on the given |message_loop_proxy|.
  // TODO: this should support returning entries in multiple chunks.
  static bool RelayReadDirectory(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const ReadDirectoryTask& task,
      const ReadDirectoryCallback& callback);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FileSystemFileUtilProxy);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_PROXY_H_
