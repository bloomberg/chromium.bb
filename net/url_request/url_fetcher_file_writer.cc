// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_fetcher_file_writer.h"

#include "base/files/file_util_proxy.h"
#include "base/single_thread_task_runner.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

URLFetcherFileWriter::URLFetcherFileWriter(
    scoped_refptr<base::TaskRunner> file_task_runner)
    : error_code_(base::PLATFORM_FILE_OK),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      file_task_runner_(file_task_runner),
      file_handle_(base::kInvalidPlatformFileValue),
      total_bytes_written_(0) {
  DCHECK(file_task_runner_.get());
}

URLFetcherFileWriter::~URLFetcherFileWriter() {
  CloseAndDeleteFile();
}

void URLFetcherFileWriter::CreateFileAtPath(
    const base::FilePath& file_path,
    const CompletionCallback& callback) {
  base::FileUtilProxy::CreateOrOpen(
      file_task_runner_,
      file_path,
      base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE,
      base::Bind(&URLFetcherFileWriter::DidCreateFile,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 file_path));
}

void URLFetcherFileWriter::CreateTempFile(const CompletionCallback& callback) {
  base::FileUtilProxy::CreateTemporary(
      file_task_runner_,
      0,  // No additional file flags.
      base::Bind(&URLFetcherFileWriter::DidCreateTempFile,
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void URLFetcherFileWriter::Write(scoped_refptr<IOBuffer> buffer,
                                 int num_bytes,
                                 const CompletionCallback& callback) {
  ContinueWrite(new DrainableIOBuffer(buffer, num_bytes), callback,
                base::PLATFORM_FILE_OK, 0);
}

void URLFetcherFileWriter::ContinueWrite(
    scoped_refptr<DrainableIOBuffer> buffer,
    const CompletionCallback& callback,
    base::PlatformFileError error_code,
    int bytes_written) {
  if (file_handle_ == base::kInvalidPlatformFileValue) {
    // While a write was being done on the file thread, a request
    // to close or disown the file occured on the IO thread.  At
    // this point a request to close the file is pending on the
    // file thread.
    return;
  }

  if (base::PLATFORM_FILE_OK != error_code) {
    error_code_ = error_code;
    CloseAndDeleteFile();
    callback.Run(PlatformFileErrorToNetError(error_code));
    return;
  }

  total_bytes_written_ += bytes_written;
  buffer->DidConsume(bytes_written);

  if (buffer->BytesRemaining() > 0) {
    base::FileUtilProxy::Write(
        file_task_runner_, file_handle_,
        total_bytes_written_,  // Append to the end
        buffer->data(), buffer->BytesRemaining(),
        base::Bind(&URLFetcherFileWriter::ContinueWrite,
                   weak_factory_.GetWeakPtr(),
                   buffer,
                   callback));
  } else {
    // Finished writing buffer to the file.
    callback.Run(OK);
  }
}

void URLFetcherFileWriter::DisownFile() {
  // Disowning is done by the delegate's OnURLFetchComplete method.
  // The file should be closed by the time that method is called.
  DCHECK(file_handle_ == base::kInvalidPlatformFileValue);

  // Forget about any file by reseting the path.
  file_path_.clear();
}

void URLFetcherFileWriter::CloseFile(const CompletionCallback& callback) {
  if (file_handle_ != base::kInvalidPlatformFileValue) {
    base::FileUtilProxy::Close(
        file_task_runner_, file_handle_,
        base::Bind(&URLFetcherFileWriter::DidCloseFile,
                   weak_factory_.GetWeakPtr(),
                   callback));
    file_handle_ = base::kInvalidPlatformFileValue;
  }
}

void URLFetcherFileWriter::CloseAndDeleteFile() {
  if (file_handle_ == base::kInvalidPlatformFileValue) {
    DeleteFile(base::PLATFORM_FILE_OK);
    return;
  }
  // Close the file if it is open.
  base::FileUtilProxy::Close(
      file_task_runner_, file_handle_,
      base::Bind(&URLFetcherFileWriter::DeleteFile,
                 weak_factory_.GetWeakPtr()));
  file_handle_ = base::kInvalidPlatformFileValue;
}

void URLFetcherFileWriter::DeleteFile(
    base::PlatformFileError error_code) {
  if (file_path_.empty())
    return;

  base::FileUtilProxy::Delete(
      file_task_runner_, file_path_,
      false,  // No need to recurse, as the path is to a file.
      base::FileUtilProxy::StatusCallback());
  DisownFile();
}

void URLFetcherFileWriter::DidCreateFile(
    const CompletionCallback& callback,
    const base::FilePath& file_path,
    base::PlatformFileError error_code,
    base::PassPlatformFile file_handle,
    bool created) {
  DidCreateFileInternal(callback, file_path, error_code, file_handle);
}

void URLFetcherFileWriter::DidCreateTempFile(
    const CompletionCallback& callback,
    base::PlatformFileError error_code,
    base::PassPlatformFile file_handle,
    const base::FilePath& file_path) {
  DidCreateFileInternal(callback, file_path, error_code, file_handle);
}

void URLFetcherFileWriter::DidCreateFileInternal(
    const CompletionCallback& callback,
    const base::FilePath& file_path,
    base::PlatformFileError error_code,
    base::PassPlatformFile file_handle) {
  if (base::PLATFORM_FILE_OK == error_code) {
    file_path_ = file_path;
    file_handle_ = file_handle.ReleaseValue();
    total_bytes_written_ = 0;
  } else {
    error_code_ = error_code;
    CloseAndDeleteFile();
  }
  callback.Run(PlatformFileErrorToNetError(error_code));
}

void URLFetcherFileWriter::DidCloseFile(
    const CompletionCallback& callback,
    base::PlatformFileError error_code) {
  if (base::PLATFORM_FILE_OK != error_code) {
    error_code_ = error_code;
    CloseAndDeleteFile();
  }
  callback.Run(PlatformFileErrorToNetError(error_code));
}

}  // namespace net
