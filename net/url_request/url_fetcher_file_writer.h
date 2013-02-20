// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_FETCHER_FILE_WRITER_H_
#define NET_URL_REQUEST_URL_FETCHER_FILE_WRITER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "net/base/completion_callback.h"

namespace base {
class FilePath;
class TaskRunner;
}  // namespace base

namespace net {

class DrainableIOBuffer;
class IOBuffer;

// This class encapsulates all state involved in writing URLFetcher response
// bytes to a file.  All file operations happen on |file_task_runner_|.
class URLFetcherFileWriter {
 public:
  URLFetcherFileWriter(scoped_refptr<base::TaskRunner> file_task_runner);
  ~URLFetcherFileWriter();

  void CreateFileAtPath(const base::FilePath& file_path,
                        const CompletionCallback& callback);
  void CreateTempFile(const CompletionCallback& callback);

  // Writes |num_bytes_| response bytes in |buffer| to the file.
  void Write(scoped_refptr<IOBuffer> buffer,
             int num_bytes,
             const CompletionCallback& callback);

  // Called when a write has been done.  Continues writing if there are more
  // bytes to write.  Otherwise, runs |callback|.
  void ContinueWrite(scoped_refptr<DrainableIOBuffer> buffer,
                     const CompletionCallback& callback,
                     base::PlatformFileError error_code,
                     int bytes_written);

  // Drops ownership of the file at |file_path_|.
  // This class will not delete it or write to it again.
  void DisownFile();

  // Closes the file if it is open.
  void CloseFile(const CompletionCallback& callback);

  // Closes the file if it is open and then delete it.
  void CloseAndDeleteFile();

  const base::FilePath& file_path() const { return file_path_; }
  int64 total_bytes_written() { return total_bytes_written_; }
  base::PlatformFileError error_code() const { return error_code_; }

 private:
  // Callback which gets the result of a permanent file creation.
  void DidCreateFile(const CompletionCallback& callback,
                     const base::FilePath& file_path,
                     base::PlatformFileError error_code,
                     base::PassPlatformFile file_handle,
                     bool created);
  // Callback which gets the result of a temporary file creation.
  void DidCreateTempFile(const CompletionCallback& callback,
                         base::PlatformFileError error_code,
                         base::PassPlatformFile file_handle,
                         const base::FilePath& file_path);
  // This method is used to implement DidCreateFile and DidCreateTempFile.
  void DidCreateFileInternal(const CompletionCallback& callback,
                             const base::FilePath& file_path,
                             base::PlatformFileError error_code,
                             base::PassPlatformFile file_handle);

  // Callback which gets the result of closing the file.
  void DidCloseFile(const CompletionCallback& callback,
                    base::PlatformFileError error);

  // Callback which gets the result of closing the file. Deletes the file if
  // it has been created.
  void DeleteFile(base::PlatformFileError error_code);

  // The last error encountered on a file operation.  base::PLATFORM_FILE_OK
  // if no error occurred.
  base::PlatformFileError error_code_;

  // Callbacks are created for use with base::FileUtilProxy.
  base::WeakPtrFactory<URLFetcherFileWriter> weak_factory_;

  // Task runner on which file operations should happen.
  scoped_refptr<base::TaskRunner> file_task_runner_;

  // Path to the file.  This path is empty when there is no file.
  base::FilePath file_path_;

  // Handle to the file.
  base::PlatformFile file_handle_;

  // We always append to the file.  Track the total number of bytes
  // written, so that writes know the offset to give.
  int64 total_bytes_written_;

  DISALLOW_COPY_AND_ASSIGN(URLFetcherFileWriter);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_FETCHER_FILE_WRITER_H_
