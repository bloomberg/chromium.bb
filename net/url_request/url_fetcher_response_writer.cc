// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_fetcher_response_writer.h"

#include "base/file_util.h"
#include "base/location.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

URLFetcherStringWriter::URLFetcherStringWriter(std::string* string)
    : string_(string) {
}

URLFetcherStringWriter::~URLFetcherStringWriter() {
}

int URLFetcherStringWriter::Initialize(const CompletionCallback& callback) {
  // Do nothing.
  return OK;
}

int URLFetcherStringWriter::Write(IOBuffer* buffer,
                                  int num_bytes,
                                  const CompletionCallback& callback) {
  string_->append(buffer->data(), num_bytes);
  return num_bytes;
}

int URLFetcherStringWriter::Finish(const CompletionCallback& callback) {
  // Do nothing.
  return OK;
}

URLFetcherFileWriter::URLFetcherFileWriter(
    scoped_refptr<base::TaskRunner> file_task_runner)
    : error_code_(OK),
      weak_factory_(this),
      file_task_runner_(file_task_runner),
      owns_file_(false),
      total_bytes_written_(0) {
  DCHECK(file_task_runner_.get());
}

URLFetcherFileWriter::~URLFetcherFileWriter() {
  CloseAndDeleteFile();
}

int URLFetcherFileWriter::Initialize(const CompletionCallback& callback) {
  DCHECK(!file_stream_);
  DCHECK(!owns_file_);

  file_stream_.reset(new FileStream(NULL));

  int result = ERR_IO_PENDING;
  if (file_path_.empty()) {
    base::FilePath* temp_file_path = new base::FilePath;
    base::PostTaskAndReplyWithResult(
        file_task_runner_.get(),
        FROM_HERE,
        base::Bind(&file_util::CreateTemporaryFile, temp_file_path),
        base::Bind(&URLFetcherFileWriter::DidCreateTempFile,
                   weak_factory_.GetWeakPtr(),
                   callback,
                   base::Owned(temp_file_path)));
  } else {
    result = file_stream_->Open(
        file_path_,
        base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_ASYNC |
        base::PLATFORM_FILE_CREATE_ALWAYS,
        base::Bind(&URLFetcherFileWriter::DidOpenFile,
                   weak_factory_.GetWeakPtr(),
                   callback));
    DCHECK_NE(OK, result);
    if (result != ERR_IO_PENDING)
      error_code_ = result;
  }
  return result;
}

int URLFetcherFileWriter::Write(IOBuffer* buffer,
                                int num_bytes,
                                const CompletionCallback& callback) {
  DCHECK(file_stream_);
  DCHECK(owns_file_);

  int result = file_stream_->Write(buffer, num_bytes,
                                   base::Bind(&URLFetcherFileWriter::DidWrite,
                                              weak_factory_.GetWeakPtr(),
                                              callback));
  if (result < 0 && result != ERR_IO_PENDING) {
    error_code_ = result;
    CloseAndDeleteFile();
  }
  return result;
}

int URLFetcherFileWriter::Finish(const CompletionCallback& callback) {
  file_stream_.reset();
  return OK;
}

void URLFetcherFileWriter::DidWrite(const CompletionCallback& callback,
                                    int result) {
  if (result < 0) {
    error_code_ = result;
    CloseAndDeleteFile();
  }
  callback.Run(result);
}

void URLFetcherFileWriter::DisownFile() {
  // Disowning is done by the delegate's OnURLFetchComplete method.
  // The file should be closed by the time that method is called.
  DCHECK(!file_stream_);

  owns_file_ = false;
}

void URLFetcherFileWriter::CloseAndDeleteFile() {
  if (!owns_file_)
    return;

  file_stream_.reset();
  DisownFile();
  file_task_runner_->PostTask(FROM_HERE,
                              base::Bind(base::IgnoreResult(&base::DeleteFile),
                                         file_path_,
                                         false /* recursive */));
}

void URLFetcherFileWriter::DidCreateTempFile(const CompletionCallback& callback,
                                             base::FilePath* temp_file_path,
                                             bool success) {
  if (!success) {
    error_code_ = ERR_FILE_NOT_FOUND;
    callback.Run(error_code_);
    return;
  }
  file_path_ = *temp_file_path;
  owns_file_ = true;
  const int result = file_stream_->Open(
      file_path_,
      base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_ASYNC |
      base::PLATFORM_FILE_OPEN,
      base::Bind(&URLFetcherFileWriter::DidOpenFile,
                 weak_factory_.GetWeakPtr(),
                 callback));
  if (result != ERR_IO_PENDING)
    DidOpenFile(callback, result);
}

void URLFetcherFileWriter::DidOpenFile(const CompletionCallback& callback,
                                       int result) {
  if (result == OK) {
    total_bytes_written_ = 0;
    owns_file_ = true;
  } else {
    error_code_ = result;
    CloseAndDeleteFile();
  }
  callback.Run(result);
}

}  // namespace net
