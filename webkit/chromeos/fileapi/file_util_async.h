// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHROMEOS_FILEAPI_FILE_SYSTEM_FILE_UTIL_ASYNC_H_
#define WEBKIT_CHROMEOS_FILEAPI_FILE_SYSTEM_FILE_UTIL_ASYNC_H_

#include "base/callback.h"
#include "base/files/file_util_proxy.h"
#include "base/platform_file.h"
#include "webkit/chromeos/fileapi/async_file_stream.h"

namespace fileapi {

using base::PlatformFileError;

// The interface class of asynchronous file utils. All functions are pure
// virtual.
class FileUtilAsync {
 public:
  virtual ~FileUtilAsync() {}

  typedef base::FileUtilProxy::Entry DirectoryEntry;

  // Used for GetFileInfo(). |result| is the return code of the operation,
  // and |file_info| is the obtained file info.
  typedef base::Callback<void(
      PlatformFileError result,
      const base::PlatformFileInfo& file_info)> GetFileInfoCallback;

  // Used for Create(), etc. |result| is the return code of the operation.
  typedef base::Callback<void(PlatformFileError)> StatusCallback;

  // Used for Open(). |result| is the return code of the operation, and
  // |stream| is the stream of the opened file.
  typedef base::Callback<void(
      PlatformFileError result,
      AsyncFileStream* stream)> OpenCallback;

  typedef std::vector<DirectoryEntry> FileList;

  // Used for ReadDirectoryCallback(). |result| is the return code of the
  // operation, |file_list| is the list of files read, and |completed| is
  // true if all files are read.
  typedef base::Callback<void(
      PlatformFileError result,
      const FileList& file_list,
      bool completed)> ReadDirectoryCallback;

  // Opens a file of the given |file_path| with |flags|. On success,
  // PLATFORM_FILE_OK is passed to |callback| with a pointer to newly
  // created AsyncFileStream object. The caller should delete the
  // stream. On failure, an error code is passed instead.
  virtual void Open(const base::FilePath& file_path,
                    int file_flags,  // PlatformFileFlags
                    const OpenCallback& callback) = 0;

  // Gets file info of the given |file_path|. On success,
  // PLATFORM_FILE_OK is passed to |callback| with the the obtained file
  // info. On failure, an error code is passed instead.
  virtual void GetFileInfo(const base::FilePath& file_path,
                           const GetFileInfoCallback& callback) = 0;

  // Creates a file of the given |file_path|. On success,
  // PLATFORM_FILE_OK is passed to |callback|. On failure, an error code
  // is passed instead.
  virtual void Create(const base::FilePath& file_path,
                      const StatusCallback& callback) = 0;

  // Truncates a file of the given |file_path| to |length|. On success,
  // PLATFORM_FILE_OK is passed to |callback|. On failure, an error code
  // is passed instead.
  virtual void Truncate(const base::FilePath& file_path,
                        int64 length,
                        const StatusCallback& callback) = 0;

  // Modifies the timestamps of a file of the given |file_path|. On
  // success, PLATFORM_FILE_OK is passed to |callback|. On failure, an
  // error code is passed instead.
  virtual void Touch(const base::FilePath& file_path,
                     const base::Time& last_access_time,
                     const base::Time& last_modified_time,
                     const StatusCallback& callback) = 0;

  // Removes a file or directory of the given |file_path|. If |recursive|
  // is true, removes the contents of the given directory recursively. On
  // success, PLATFORM_FILE_OK is passed to |callback|. On failure, an
  // error code is passed instead.
  virtual void Remove(const base::FilePath& file_path,
                      bool recursive,
                      const StatusCallback& callback) = 0;

  // Creates a directory of the given |dir_path|. On success,
  // PLATFORM_FILE_OK is passed to |callback|. On failure, an error code
  // is passed instead.
  virtual void CreateDirectory(const base::FilePath& dir_path,
                               const StatusCallback& callback) = 0;

  // Reads a directory of the given |dir_path|. On success,
  // PLATFORM_FILE_OK is passed to |callback| with the list of files, and
  // a boolean value indicating if all files are read. On failure, an
  // error code is passed instead.
  //
  // The ReadDirectoryCallback may be called several times, returning the
  // portions of the whole directory listing. The reference to vector with
  // DirectoryEntry objects, passed to callback is guaranteed to contain the
  // results of the operation only while callback is running.
  //
  // The implementations of FileUtilAsync should be careful to populate the
  // vector on the same thread, on which the callback is called. Otherwise,
  // if callback is called through PostTask, the data might get overwritten
  // before callback is actually called.
  //
  // TODO(olege): Maybe make it possible to read only a part of the directory.
  virtual void ReadDirectory(const base::FilePath& dir_path,
                             const ReadDirectoryCallback& callback) = 0;

  // TODO(olege): Add LocalCopy and LocalMove.
};

}  // namespace fileapi

#endif  // WEBKIT_CHROMEOS_FILEAPI_FILE_SYSTEM_FILE_UTIL_ASYNC_H_
