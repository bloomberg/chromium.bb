// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_FRAME_HANDLER_H_
#define NET_WEBSOCKETS_WEBSOCKET_FRAME_HANDLER_H_
#pragma once

#include <deque>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

namespace net {

class IOBuffer;
class IOBufferWithSize;

// Handles WebSocket frame messages.
class WebSocketFrameHandler {
 public:
  struct FrameInfo {
    const char* frame_start;
    int frame_length;
    const char* message_start;
    int message_length;
  };

  WebSocketFrameHandler();
  ~WebSocketFrameHandler();

  // Appends WebSocket raw data on connection.
  // For sending, this is data from WebKit.
  // For receiving, this is data from network.
  void AppendData(const char* data, int len);

  // Updates current IOBuffer.
  // If |buffered| is true, it tries to find WebSocket frames.
  // Otherwise, it just picks the first buffer in |pending_buffers_|.
  // Returns available size of data, 0 if no more data or current buffer was
  // not released, and negative if some error occurred.
  int UpdateCurrentBuffer(bool buffered);

  // Gets current IOBuffer.
  // For sending, this is data to network.
  // For receiving, this is data to WebKit.
  // Returns NULL just after ReleaseCurrentBuffer() was called.
  IOBuffer* GetCurrentBuffer() { return current_buffer_.get(); }
  int GetCurrentBufferSize() const { return current_buffer_size_; }

  // Returns original buffer size of current IOBuffer.
  // This might differ from GetCurrentBufferSize() if frame message is
  // compressed or decompressed.
  int GetOriginalBufferSize() const { return original_current_buffer_size_; }

  // Releases current IOBuffer.
  void ReleaseCurrentBuffer();

  // Parses WebSocket frame in [|buffer|, |buffer|+|size|), fills frame
  // information in |frame_info|, and returns number of bytes for
  // complete WebSocket frames.
  static int ParseWebSocketFrame(const char* buffer, int size,
                                 std::vector<FrameInfo>* frame_info);

 private:
  typedef std::deque< scoped_refptr<IOBufferWithSize> > PendingDataQueue;

  scoped_refptr<IOBuffer> current_buffer_;
  int current_buffer_size_;

  int original_current_buffer_size_;

  // Deque of IOBuffers in pending.
  PendingDataQueue pending_buffers_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketFrameHandler);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_FRAME_HANDLER_H_
