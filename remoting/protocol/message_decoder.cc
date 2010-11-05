// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/message_decoder.h"

#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "remoting/base/multiple_array_input_stream.h"
#include "remoting/proto/internal.pb.h"
#include "third_party/libjingle/source/talk/base/byteorder.h"

namespace remoting {

MessageDecoder::MessageDecoder()
    : available_bytes_(0),
      next_payload_(0),
      next_payload_known_(false) {
}

MessageDecoder::~MessageDecoder() {}

void MessageDecoder::AddBuffer(scoped_refptr<net::IOBuffer> data,
                               int data_size) {
  buffer_list_.push_back(make_scoped_refptr(
      new net::DrainableIOBuffer(data, data_size)));
  available_bytes_ += data_size;
}

MultipleArrayInputStream* MessageDecoder::CreateInputStreamFromData() {
  // Determine the payload size. If we already know it then skip this part.
  // We may not have enough data to determine the payload size so use a
  // utility function to find out.
  int next_payload = -1;
  if (!next_payload_known_ && GetPayloadSize(&next_payload)) {
    DCHECK_NE(-1, next_payload);
    next_payload_ = next_payload;
    next_payload_known_ = true;
  }

  // If the next payload size is still not known or we don't have enough
  // data for parsing then exit.
  if (!next_payload_known_ || available_bytes_ < next_payload_)
    return NULL;
  next_payload_known_ = false;

  // The following loop gather buffers in |buffer_list_| that sum up to
  // |next_payload_| bytes. These buffers are added to |stream|.

  // Create a MultipleArrayInputStream for parsing.
  // TODO(hclam): Avoid creating this object everytime.
  MultipleArrayInputStream* stream = new MultipleArrayInputStream();
  while (next_payload_ > 0 && !buffer_list_.empty()) {
    scoped_refptr<net::DrainableIOBuffer> buffer = buffer_list_.front();
    int read_bytes = std::min(buffer->BytesRemaining(), next_payload_);

    // This call creates a new instance of DrainableIOBuffer internally.
    // This will reference the same base pointer but maintain it's own
    // version of data pointer.
    stream->AddBuffer(buffer, read_bytes);

    // Adjust counters.
    buffer->DidConsume(read_bytes);
    next_payload_ -= read_bytes;
    available_bytes_ -= read_bytes;

    // If the front buffer is fully read then remove it from the queue.
    if (!buffer->BytesRemaining())
      buffer_list_.pop_front();
  }
  DCHECK_EQ(0, next_payload_);
  DCHECK_LE(0, available_bytes_);
  return stream;
}

static int GetHeaderSize(const std::string& header) {
  return header.length();
}

bool MessageDecoder::GetPayloadSize(int* size) {
  // The header has a size of 4 bytes.
  const int kHeaderSize = sizeof(int32);

  if (available_bytes_ < kHeaderSize)
    return false;

  std::string header;
  while (GetHeaderSize(header) < kHeaderSize && !buffer_list_.empty()) {
    scoped_refptr<net::DrainableIOBuffer> buffer = buffer_list_.front();

    // Find out how many bytes we need and how many bytes are available in this
    // buffer.
    int needed_bytes = kHeaderSize - GetHeaderSize(header);
    int available_bytes = buffer->BytesRemaining();

    // Then append the required bytes into the header and advance the last
    // read position.
    int read_bytes = std::min(needed_bytes, available_bytes);
    header.append(buffer->data(), read_bytes);
    buffer->DidConsume(read_bytes);
    available_bytes_ -= read_bytes;

    // If the buffer is depleted then remove it from the queue.
    if (!buffer->BytesRemaining()) {
      buffer_list_.pop_front();
    }
  }

  *size = talk_base::GetBE32(header.c_str());
  return true;
}

}  // namespace remoting
