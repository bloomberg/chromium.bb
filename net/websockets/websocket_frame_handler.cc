// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <limits>

#include "net/websockets/websocket_frame_handler.h"

#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

WebSocketFrameHandler::WebSocketFrameHandler()
    : current_buffer_size_(0),
      original_current_buffer_size_(0) {
}

WebSocketFrameHandler::~WebSocketFrameHandler() {
}

void WebSocketFrameHandler::AppendData(const char* data, int length) {
  scoped_refptr<IOBufferWithSize> buffer = new IOBufferWithSize(length);
  memcpy(buffer->data(), data, length);
  pending_buffers_.push_back(buffer);
}

int WebSocketFrameHandler::UpdateCurrentBuffer(bool buffered) {
  if (current_buffer_)
    return 0;
  DCHECK(!current_buffer_size_);
  DCHECK(!original_current_buffer_size_);

  if (pending_buffers_.empty())
    return 0;
  scoped_refptr<IOBufferWithSize> buffer = pending_buffers_.front();

  int buffer_size = 0;
  if (buffered) {
    std::vector<FrameInfo> frame_info;
    buffer_size =
        ParseWebSocketFrame(buffer->data(), buffer->size(), &frame_info);
    if (buffer_size <= 0)
      return buffer_size;

    original_current_buffer_size_ = buffer_size;

    // TODO(ukai): filter(e.g. compress or decompress) frame messages.
  } else {
    original_current_buffer_size_ = buffer->size();
    buffer_size = buffer->size();
  }

  current_buffer_ = buffer;
  current_buffer_size_ = buffer_size;
  return buffer_size;
}

void WebSocketFrameHandler::ReleaseCurrentBuffer() {
  DCHECK(!pending_buffers_.empty());
  scoped_refptr<IOBufferWithSize> front_buffer = pending_buffers_.front();
  pending_buffers_.pop_front();
  int remaining_size = front_buffer->size() - original_current_buffer_size_;
  if (remaining_size > 0) {
    scoped_refptr<IOBufferWithSize> next_buffer = NULL;
    int buffer_size = remaining_size;
    if (!pending_buffers_.empty()) {
      next_buffer = pending_buffers_.front();
      buffer_size += next_buffer->size();
      pending_buffers_.pop_front();
    }
    // TODO(ukai): don't copy data.
    scoped_refptr<IOBufferWithSize> buffer = new IOBufferWithSize(buffer_size);
    memcpy(buffer->data(), front_buffer->data() + original_current_buffer_size_,
           remaining_size);
    if (next_buffer)
      memcpy(buffer->data() + remaining_size,
             next_buffer->data(), next_buffer->size());
    pending_buffers_.push_front(buffer);
  }
  current_buffer_ = NULL;
  current_buffer_size_ = 0;
  original_current_buffer_size_ = 0;
}

/* static */
int WebSocketFrameHandler::ParseWebSocketFrame(
    const char* buffer, int size, std::vector<FrameInfo>* frame_info) {
  const char* end = buffer + size;
  const char* p = buffer;
  int buffer_size = 0;
  while (p < end) {
    FrameInfo frame;
    frame.frame_start = p;
    frame.message_length = -1;
    unsigned char frame_byte = static_cast<unsigned char>(*p++);
    if ((frame_byte & 0x80) == 0x80) {
      int length = 0;
      while (p < end) {
        // Note: might overflow later if numeric_limits<int>::max() is not
        // n*128-1.
        if (length > std::numeric_limits<int>::max() / 128) {
          // frame length overflow.
          return ERR_INSUFFICIENT_RESOURCES;
        }
        unsigned char c = static_cast<unsigned char>(*p);
        length = length * 128 + (c & 0x7f);
        ++p;
        if ((c & 0x80) != 0x80)
          break;
      }
      if (end - p >= length) {
        frame.message_start = p;
        frame.message_length = length;
        p += length;
      } else {
        break;
      }
    } else {
      frame.message_start = p;
      while (p < end && *p != '\xff')
        ++p;
      if (p < end && *p == '\xff') {
        frame.message_length = p - frame.message_start;
        ++p;
      } else {
        break;
      }
    }
    if (frame.message_length >= 0 && p <= end) {
      frame.frame_length = p - frame.frame_start;
      buffer_size += frame.frame_length;
      frame_info->push_back(frame);
    }
  }
  return buffer_size;
}

}  // namespace net
