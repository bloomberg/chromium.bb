// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/upload_file_system_file_element_reader.h"

#include "base/bind.h"
#include "net/base/net_errors.h"
#include "webkit/blob/file_stream_reader.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_url.h"

namespace fileapi {

UploadFileSystemFileElementReader::UploadFileSystemFileElementReader(
    FileSystemContext* file_system_context,
    const GURL& url,
    uint64 range_offset,
    uint64 range_length,
    const base::Time& expected_modification_time)
    : file_system_context_(file_system_context),
      url_(url),
      range_offset_(range_offset),
      range_length_(range_length),
      expected_modification_time_(expected_modification_time),
      stream_length_(0),
      position_(0),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

UploadFileSystemFileElementReader::~UploadFileSystemFileElementReader() {
}

int UploadFileSystemFileElementReader::Init(
    const net::CompletionCallback& callback) {
  // Reset states.
  weak_ptr_factory_.InvalidateWeakPtrs();
  stream_length_ = 0;
  position_ = 0;

  // Initialize the stream reader and the length.
  stream_reader_.reset(
      file_system_context_->CreateFileStreamReader(
          file_system_context_->CrackURL(url_),
          range_offset_,
          expected_modification_time_));
  DCHECK(stream_reader_);

  const int result = stream_reader_->GetLength(
      base::Bind(&UploadFileSystemFileElementReader::OnGetLength,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
  if (result >= 0) {
    stream_length_ = result;
    return net::OK;
  }
  return result;
}

uint64 UploadFileSystemFileElementReader::GetContentLength() const {
  return std::min(stream_length_, range_length_);
}

uint64 UploadFileSystemFileElementReader::BytesRemaining() const {
  return GetContentLength() - position_;
}

int UploadFileSystemFileElementReader::Read(
    net::IOBuffer* buf,
    int buf_length,
    const net::CompletionCallback& callback) {
  DCHECK_LT(0, buf_length);
  DCHECK(stream_reader_);

  const uint64 num_bytes_to_read =
      std::min(BytesRemaining(), static_cast<uint64>(buf_length));

  if (num_bytes_to_read == 0)
    return 0;

  const int result = stream_reader_->Read(
      buf, num_bytes_to_read,
      base::Bind(&UploadFileSystemFileElementReader::OnRead,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
  if (result >= 0)
    OnRead(net::CompletionCallback(), result);
  return result;
}

void UploadFileSystemFileElementReader::OnGetLength(
    const net::CompletionCallback& callback,
    int64 result) {
  if (result >= 0) {
    stream_length_ = result;
    callback.Run(net::OK);
    return;
  }
  callback.Run(result);
}

void UploadFileSystemFileElementReader::OnRead(
    const net::CompletionCallback& callback,
    int result) {
  if (result > 0) {
    position_ += result;
    DCHECK_LE(position_, GetContentLength());
  }
  if (!callback.is_null())
    callback.Run(result);
}

}  // namespace fileapi
