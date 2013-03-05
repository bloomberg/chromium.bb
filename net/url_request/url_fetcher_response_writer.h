// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_FETCHER_RESPONSE_WRITER_H_
#define NET_URL_REQUEST_URL_FETCHER_RESPONSE_WRITER_H_

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
// bytes to the destination.
class URLFetcherResponseWriter {
 public:
  virtual ~URLFetcherResponseWriter() {}

  // Initializes this instance. If ERR_IO_PENDING is returned, |callback| will
  // be run later with the result.
  virtual int Initialize(const CompletionCallback& callback) = 0;

  // Writes |num_bytes| bytes in |buffer|, and returns the number of bytes
  // written or an error code. If ERR_IO_PENDING is returned, |callback| will be
  // run later with the result.
  virtual int Write(IOBuffer* buffer,
                    int num_bytes,
                    const CompletionCallback& callback) = 0;

  // Finishes writing. If ERR_IO_PENDING is returned, |callback| will be run
  // later with the result.
  virtual int Finish(const CompletionCallback& callback) = 0;
};

// URLFetcherResponseWriter implementation for std::string.
class URLFetcherStringWriter : public URLFetcherResponseWriter {
 public:
  URLFetcherStringWriter(std::string* string);
  virtual ~URLFetcherStringWriter();

  // URLFetcherResponseWriter overrides:
  virtual int Initialize(const CompletionCallback& callback) OVERRIDE;
  virtual int Write(IOBuffer* buffer,
                    int num_bytes,
                    const CompletionCallback& callback) OVERRIDE;
  virtual int Finish(const CompletionCallback& callback) OVERRIDE;

 private:
  std::string* const string_;

  DISALLOW_COPY_AND_ASSIGN(URLFetcherStringWriter);
};

// URLFetcherResponseWriter implementation for files.
class URLFetcherFileWriter : public URLFetcherResponseWriter {
 public:
  URLFetcherFileWriter(scoped_refptr<base::TaskRunner> file_task_runner);
  virtual ~URLFetcherFileWriter();

  // Sets destination file path.
  // Call this method before Initialize() to set the destination path,
  // if this method is not called before Initialize(), Initialize() will create
  // a temporary file.
  void set_destination_file_path(const base::FilePath& file_path) {
    file_path_ = file_path;
  }

  // URLFetcherResponseWriter overrides:
  virtual int Initialize(const CompletionCallback& callback) OVERRIDE;
  virtual int Write(IOBuffer* buffer,
                    int num_bytes,
                    const CompletionCallback& callback) OVERRIDE;
  virtual int Finish(const CompletionCallback& callback) OVERRIDE;

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
  // |callback| is run with the result upon completion.
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
  // |callback| is run with the result.
  void DidCreateFileInternal(const CompletionCallback& callback,
                             const base::FilePath& file_path,
                             base::PlatformFileError error_code,
                             base::PassPlatformFile file_handle);

  // Callback which gets the result of closing the file.
  // |callback| is run with the result.
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

  // Destination file path.
  // Initialize() creates a temporary file if this variable is empty.
  base::FilePath file_path_;

  // True when this instance is responsible to delete the file at |file_path_|.
  bool owns_file_;

  // Handle to the file.
  base::PlatformFile file_handle_;

  // We always append to the file.  Track the total number of bytes
  // written, so that writes know the offset to give.
  int64 total_bytes_written_;

  DISALLOW_COPY_AND_ASSIGN(URLFetcherFileWriter);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_FETCHER_RESPONSE_WRITER_H_
