// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/messages_decoder.h"

#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "remoting/base/multiple_array_input_stream.h"
#include "talk/base/byteorder.h"

namespace remoting {

MessagesDecoder::MessagesDecoder()
    : last_read_position_(0),
      available_bytes_(0),
      next_payload_(0),
      next_payload_known_(false) {
}

MessagesDecoder::~MessagesDecoder() {}

void MessagesDecoder::ParseClientMessages(scoped_refptr<net::IOBuffer> data,
                                          int data_size,
                                          ClientMessageList* messages) {
  ParseMessages<ChromotingClientMessage>(data, data_size, messages);
}

void MessagesDecoder::ParseHostMessages(scoped_refptr<net::IOBuffer> data,
                                        int data_size,
                                        HostMessageList* messages) {
  ParseMessages<ChromotingHostMessage>(data, data_size, messages);
}

MessagesDecoder::DataChunk::DataChunk(net::IOBuffer* data, size_t data_size)
    : data(data),
      data_size(data_size) {
}

MessagesDecoder::DataChunk::~DataChunk() {}

template <typename T>
void MessagesDecoder::ParseMessages(scoped_refptr<net::IOBuffer> data,
                                    int data_size,
                                    std::list<T*>* messages) {
  // If this is the first data in the processing queue, then set the
  // last read position to 0.
  if (data_list_.empty())
    last_read_position_ = 0;

  // First enqueue the data received.
  data_list_.push_back(DataChunk(data, data_size));
  available_bytes_ += data_size;

  // Then try to parse one message until we can't parse anymore.
  T* message;
  while (ParseOneMessage<T>(&message)) {
    messages->push_back(message);
  }
}

template <typename T>
bool MessagesDecoder::ParseOneMessage(T** message) {
  // Determine the payload size. If we already know it, then skip this
  // part.
  // We have the value set to -1 for checking later.
  int next_payload = -1;
  if (!next_payload_known_ && GetPayloadSize(&next_payload)) {
    DCHECK_NE(-1, next_payload);
    next_payload_ = next_payload;
    next_payload_known_ = true;
  }

  // If the next payload size is still not known or we don't have enough
  // data for parsing then exit.
  if (!next_payload_known_ || available_bytes_ < next_payload_)
    return false;
  next_payload_known_ = false;

  // Create a MultipleArrayInputStream for parsing.
  MultipleArrayInputStream stream;
  std::vector<scoped_refptr<net::IOBuffer> > buffers;
  while (next_payload_ > 0 && !data_list_.empty()) {
    DataChunk* buffer = &(data_list_.front());
    size_t read_bytes = std::min(buffer->data_size - last_read_position_,
                                 next_payload_);

    buffers.push_back(buffer->data);
    stream.AddBuffer(buffer->data->data() + last_read_position_, read_bytes);

    // Adjust counters.
    last_read_position_ += read_bytes;
    next_payload_ -= read_bytes;
    available_bytes_ -= read_bytes;

    // If the front buffer is fully read, remove it from the queue.
    if (buffer->data_size == last_read_position_) {
      data_list_.pop_front();
      last_read_position_ = 0;
    }
  }
  DCHECK_EQ(0UL, next_payload_);

  // And finally it is parsing.
  *message = new T();
  bool ret = (*message)->ParseFromZeroCopyStream(&stream);
  if (!ret) {
    LOG(ERROR) << "Received invalid message.";
    delete *message;
  }
  return ret;
}

bool MessagesDecoder::GetPayloadSize(int* size) {
  // The header has a size of 4 bytes.
  const size_t kHeaderSize = sizeof(int32);

  if (available_bytes_ < kHeaderSize)
    return false;

  std::string header;
  while (header.length() < kHeaderSize && !data_list_.empty()) {
    DataChunk* buffer = &(data_list_.front());

    // Find out how many bytes we need and how many bytes are available in this
    // buffer.
    int needed_bytes = kHeaderSize - header.length();
    int available_bytes = buffer->data_size - last_read_position_;

    // Then append the required bytes into the header and advance the last
    // read position.
    int read_bytes = std::min(needed_bytes, available_bytes);
    header.append(
        reinterpret_cast<char*>(buffer->data->data()) + last_read_position_,
        read_bytes);
    last_read_position_ += read_bytes;
    available_bytes_ -= read_bytes;

    // If the buffer is depleted then remove it from the queue.
    if (last_read_position_ == buffer->data_size) {
      last_read_position_ = 0;
      data_list_.pop_front();
    }
  }

  if (header.length() == kHeaderSize) {
    *size = talk_base::GetBE32(header.c_str());
    return true;
  }
  NOTREACHED() << "Unable to extract payload size";
  return false;
}

}  // namespace remoting
