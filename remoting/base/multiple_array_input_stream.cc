// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>

#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "remoting/base/multiple_array_input_stream.h"

namespace remoting {

MultipleArrayInputStream::MultipleArrayInputStream()
    : current_buffer_(0),
      position_(0),
      last_returned_size_(0) {
}

MultipleArrayInputStream::~MultipleArrayInputStream() {
}

void MultipleArrayInputStream::AddBuffer(net::IOBuffer* buffer, int size) {
  DCHECK_EQ(position_, 0); // Haven't started reading.
  buffers_.push_back(make_scoped_refptr(
      new net::DrainableIOBuffer(buffer, size)));
}

bool MultipleArrayInputStream::Next(const void** data, int* size) {
  if (current_buffer_ < buffers_.size()) {
    // Reply with the number of bytes remaining in the current buffer.
    scoped_refptr<net::DrainableIOBuffer> buffer = buffers_[current_buffer_];
    last_returned_size_ = buffer->BytesRemaining();
    *data = buffer->data();
    *size = last_returned_size_;

    // After reading the current buffer then advance to the next buffer.
    buffer->DidConsume(last_returned_size_);
    ++current_buffer_;
    position_ += last_returned_size_;
    return true;
  }

  // We've reached the end of the stream. So reset |last_returned_size_|
  // to zero to prevent any backup request.
  // This is the same as in ArrayInputStream.
  // See google/protobuf/io/zero_copy_stream_impl_lite.cc.
  last_returned_size_ = 0;
  return false;
}

void MultipleArrayInputStream::BackUp(int count) {
  DCHECK_LE(count, last_returned_size_);
  DCHECK_GT(current_buffer_, 0u);

  // Rewind one buffer and rewind data offset by |count| bytes.
  --current_buffer_;
  scoped_refptr<net::DrainableIOBuffer> buffer = buffers_[current_buffer_];
  buffer->SetOffset(buffer->size() - count);
  position_ -= count;
  DCHECK_GE(position_, 0);
}

bool MultipleArrayInputStream::Skip(int count) {
  DCHECK_GE(count, 0);
  last_returned_size_ = 0;

  while (count && current_buffer_ < buffers_.size()) {
    scoped_refptr<net::DrainableIOBuffer> buffer = buffers_[current_buffer_];
    int read = std::min(count, buffer->BytesRemaining());

    // Advance the current buffer offset and position.
    buffer->DidConsume(read);
    position_ += read;
    count -= read;

    // If the current buffer is fully read, then advance to the next buffer.
    if (!buffer->BytesRemaining())
      ++current_buffer_;
  }
  return count == 0;
}

int64 MultipleArrayInputStream::ByteCount() const {
  return position_;
}

}  // namespace remoting
