// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/protocol_decoder.h"

#include "base/logging.h"
#include "remoting/base/multiple_array_input_stream.h"
#include "talk/base/byteorder.h"

namespace remoting {

ProtocolDecoder::ProtocolDecoder()
    : last_read_position_(0),
      available_bytes_(0),
      next_payload_(0),
      next_payload_known_(false) {
}

ProtocolDecoder::~ProtocolDecoder() {}

void ProtocolDecoder::ParseClientMessages(scoped_refptr<media::DataBuffer> data,
                                          ClientMessageList* messages) {
  ParseMessages<ChromotingClientMessage>(data, messages);
}

void ProtocolDecoder::ParseHostMessages(scoped_refptr<media::DataBuffer> data,
                                        HostMessageList* messages) {
  ParseMessages<ChromotingHostMessage>(data, messages);
}

template <typename T>
void ProtocolDecoder::ParseMessages(scoped_refptr<media::DataBuffer> data,
                                    std::vector<T*>* messages) {
  // If this is the first data in the processing queue, then set the
  // last read position to 0.
  if (data_list_.empty())
    last_read_position_ = 0;

  // First enqueue the data received.
  data_list_.push_back(data);
  available_bytes_ += data->GetDataSize();

  // Then try to parse one message until we can't parse anymore.
  T* message;
  while (ParseOneMessage<T>(&message)) {
    messages->push_back(message);
  }
}

template <typename T>
bool ProtocolDecoder::ParseOneMessage(T** message) {
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

  // Extract data from |data_list_| used to form a full protocol buffer.
  DataList buffers;
  std::deque<const uint8*> buffer_pointers;
  std::deque<int> buffer_sizes;
  while (next_payload_ > 0 && !data_list_.empty()) {
    scoped_refptr<media::DataBuffer> buffer = data_list_.front();
    size_t read_bytes = std::min(buffer->GetDataSize() - last_read_position_,
                                 next_payload_);

    buffers.push_back(buffer);
    buffer_pointers.push_back(buffer->GetData() + last_read_position_);
    buffer_sizes.push_back(read_bytes);

    // Adjust counters.
    last_read_position_ += read_bytes;
    next_payload_ -= read_bytes;
    available_bytes_ -= read_bytes;

    // If the front buffer is fully read, remove it from the queue.
    if (buffer->GetDataSize() == last_read_position_) {
      data_list_.pop_front();
      last_read_position_ = 0;
    }
  }
  DCHECK_EQ(0UL, next_payload_);
  DCHECK_EQ(buffers.size(), buffer_pointers.size());
  DCHECK_EQ(buffers.size(), buffer_sizes.size());

  // Create a MultipleArrayInputStream for parsing.
  MultipleArrayInputStream stream(buffers.size());
  for (size_t i = 0; i < buffers.size(); ++i) {
    stream.SetBuffer(i, buffer_pointers[i], buffer_sizes[i]);
  }

  // And finally it is parsing.
  *message = new T();
  bool ret = (*message)->ParseFromZeroCopyStream(&stream);
  if (!ret)
    delete *message;
  return ret;
}

bool ProtocolDecoder::GetPayloadSize(int* size) {
  // The header has a size of 4 bytes.
  const size_t kHeaderSize = sizeof(int32);

  if (available_bytes_ < kHeaderSize)
    return false;

  std::string header;
  while (header.length() < kHeaderSize && !data_list_.empty()) {
    scoped_refptr<media::DataBuffer> buffer = data_list_.front();

    // Find out how many bytes we need and how many bytes are available in this
    // buffer.
    int needed_bytes = kHeaderSize - header.length();
    int available_bytes = buffer->GetDataSize() - last_read_position_;

    // Then append the required bytes into the header and advance the last
    // read position.
    int read_bytes = std::min(needed_bytes, available_bytes);
    header.append(
        reinterpret_cast<const char*>(buffer->GetData()) + last_read_position_,
        read_bytes);
    last_read_position_ += read_bytes;
    available_bytes_ -= read_bytes;

    // If the buffer is depleted then remove it from the queue.
    if (last_read_position_ == buffer->GetDataSize()) {
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
