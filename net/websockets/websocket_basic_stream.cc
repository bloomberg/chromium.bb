// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_basic_stream.h"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/client_socket_handle.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_frame_parser.h"

namespace net {

namespace {

// The number of bytes to attempt to read at a time.
// TODO(ricea): See if there is a better number or algorithm to fulfill our
// requirements:
//  1. We would like to use minimal memory on low-bandwidth or idle connections
//  2. We would like to read as close to line speed as possible on
//     high-bandwidth connections
//  3. We can't afford to cause jank on the IO thread by copying large buffers
//     around
//  4. We would like to hit any sweet-spots that might exist in terms of network
//     packet sizes / encryption block sizes / IPC alignment issues, etc.
const int kReadBufferSize = 32 * 1024;

}  // namespace

WebSocketBasicStream::WebSocketBasicStream(
    scoped_ptr<ClientSocketHandle> connection)
    : read_buffer_(new IOBufferWithSize(kReadBufferSize)),
      connection_(connection.Pass()),
      generate_websocket_masking_key_(&GenerateWebSocketMaskingKey) {
  DCHECK(connection_->is_initialized());
}

WebSocketBasicStream::~WebSocketBasicStream() { Close(); }

int WebSocketBasicStream::ReadFrames(
    ScopedVector<WebSocketFrameChunk>* frame_chunks,
    const CompletionCallback& callback) {
  DCHECK(frame_chunks->empty());
  // If there is data left over after parsing the HTTP headers, attempt to parse
  // it as WebSocket frames.
  if (http_read_buffer_) {
    DCHECK_GE(http_read_buffer_->offset(), 0);
    // We cannot simply copy the data into read_buffer_, as it might be too
    // large.
    scoped_refptr<GrowableIOBuffer> buffered_data;
    buffered_data.swap(http_read_buffer_);
    DCHECK(http_read_buffer_.get() == NULL);
    if (!parser_.Decode(buffered_data->StartOfBuffer(),
                        buffered_data->offset(),
                        frame_chunks))
      return WebSocketErrorToNetError(parser_.websocket_error());
    if (!frame_chunks->empty())
      return OK;
  }

  // Run until socket stops giving us data or we get some chunks.
  while (true) {
    // base::Unretained(this) here is safe because net::Socket guarantees not to
    // call any callbacks after Disconnect(), which we call from the
    // destructor. The caller of ReadFrames() is required to keep |frame_chunks|
    // valid.
    int result = connection_->socket()
                     ->Read(read_buffer_.get(),
                            read_buffer_->size(),
                            base::Bind(&WebSocketBasicStream::OnReadComplete,
                                       base::Unretained(this),
                                       base::Unretained(frame_chunks),
                                       callback));
    if (result == ERR_IO_PENDING)
      return result;
    result = HandleReadResult(result, frame_chunks);
    if (result != ERR_IO_PENDING)
      return result;
  }
}

int WebSocketBasicStream::WriteFrames(
    ScopedVector<WebSocketFrameChunk>* frame_chunks,
    const CompletionCallback& callback) {
  // This function always concatenates all frames into a single buffer.
  // TODO(ricea): Investigate whether it would be better in some cases to
  // perform multiple writes with smaller buffers.
  //
  // First calculate the size of the buffer we need to allocate.
  typedef ScopedVector<WebSocketFrameChunk>::const_iterator Iterator;
  const int kMaximumTotalSize = std::numeric_limits<int>::max();
  int total_size = 0;
  for (Iterator it = frame_chunks->begin(); it != frame_chunks->end(); ++it) {
    WebSocketFrameChunk* chunk = *it;
    DCHECK(chunk->header)
        << "Only complete frames are supported by WebSocketBasicStream";
    DCHECK(chunk->final_chunk)
        << "Only complete frames are supported by WebSocketBasicStream";
    // Force the masked bit on.
    chunk->header->masked = true;
    // We enforce flow control so the renderer should never be able to force us
    // to cache anywhere near 2GB of frames.
    int chunk_size =
        chunk->data->size() + GetWebSocketFrameHeaderSize(*(chunk->header));
    CHECK_GE(kMaximumTotalSize - total_size, chunk_size)
        << "Aborting to prevent overflow";
    total_size += chunk_size;
  }
  scoped_refptr<IOBufferWithSize> combined_buffer(
      new IOBufferWithSize(total_size));
  char* dest = combined_buffer->data();
  int remaining_size = total_size;
  for (Iterator it = frame_chunks->begin(); it != frame_chunks->end(); ++it) {
    WebSocketFrameChunk* chunk = *it;
    WebSocketMaskingKey mask = generate_websocket_masking_key_();
    int result = WriteWebSocketFrameHeader(
        *(chunk->header), &mask, dest, remaining_size);
    DCHECK(result != ERR_INVALID_ARGUMENT)
        << "WriteWebSocketFrameHeader() says that " << remaining_size
        << " is not enough to write the header in. This should not happen.";
    CHECK_GE(result, 0) << "Potentially security-critical check failed";
    dest += result;
    remaining_size -= result;

    const char* const frame_data = chunk->data->data();
    const int frame_size = chunk->data->size();
    CHECK_GE(remaining_size, frame_size);
    std::copy(frame_data, frame_data + frame_size, dest);
    MaskWebSocketFramePayload(mask, 0, dest, frame_size);
    dest += frame_size;
    remaining_size -= frame_size;
  }
  DCHECK_EQ(0, remaining_size) << "Buffer size calculation was wrong; "
                               << remaining_size << " bytes left over.";
  scoped_refptr<DrainableIOBuffer> drainable_buffer(
      new DrainableIOBuffer(combined_buffer, total_size));
  return WriteEverything(drainable_buffer, callback);
}

void WebSocketBasicStream::Close() { connection_->socket()->Disconnect(); }

std::string WebSocketBasicStream::GetSubProtocol() const {
  return sub_protocol_;
}

std::string WebSocketBasicStream::GetExtensions() const { return extensions_; }

int WebSocketBasicStream::SendHandshakeRequest(
    const GURL& url,
    const HttpRequestHeaders& headers,
    HttpResponseInfo* response_info,
    const CompletionCallback& callback) {
  // TODO(ricea): Implement handshake-related functionality.
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

int WebSocketBasicStream::ReadHandshakeResponse(
    const CompletionCallback& callback) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

/*static*/
scoped_ptr<WebSocketBasicStream>
WebSocketBasicStream::CreateWebSocketBasicStreamForTesting(
    scoped_ptr<ClientSocketHandle> connection,
    const scoped_refptr<GrowableIOBuffer>& http_read_buffer,
    const std::string& sub_protocol,
    const std::string& extensions,
    WebSocketMaskingKeyGeneratorFunction key_generator_function) {
  scoped_ptr<WebSocketBasicStream> stream(
      new WebSocketBasicStream(connection.Pass()));
  if (http_read_buffer) {
    stream->http_read_buffer_ = http_read_buffer;
  }
  stream->sub_protocol_ = sub_protocol;
  stream->extensions_ = extensions;
  stream->generate_websocket_masking_key_ = key_generator_function;
  return stream.Pass();
}

int WebSocketBasicStream::WriteEverything(
    const scoped_refptr<DrainableIOBuffer>& buffer,
    const CompletionCallback& callback) {
  while (buffer->BytesRemaining() > 0) {
    // The use of base::Unretained() here is safe because on destruction we
    // disconnect the socket, preventing any further callbacks.
    int result = connection_->socket()
                     ->Write(buffer.get(),
                             buffer->BytesRemaining(),
                             base::Bind(&WebSocketBasicStream::OnWriteComplete,
                                        base::Unretained(this),
                                        buffer,
                                        callback));
    if (result > 0) {
      buffer->DidConsume(result);
    } else {
      return result;
    }
  }
  return OK;
}

void WebSocketBasicStream::OnWriteComplete(
    const scoped_refptr<DrainableIOBuffer>& buffer,
    const CompletionCallback& callback,
    int result) {
  if (result < 0) {
    DCHECK(result != ERR_IO_PENDING);
    callback.Run(result);
    return;
  }

  DCHECK(result != 0);
  buffer->DidConsume(result);
  result = WriteEverything(buffer, callback);
  if (result != ERR_IO_PENDING)
    callback.Run(result);
}

int WebSocketBasicStream::HandleReadResult(
    int result,
    ScopedVector<WebSocketFrameChunk>* frame_chunks) {
  DCHECK_NE(ERR_IO_PENDING, result);
  DCHECK(frame_chunks->empty());
  if (result < 0)
    return result;
  if (result == 0)
    return ERR_CONNECTION_CLOSED;
  if (!parser_.Decode(read_buffer_->data(), result, frame_chunks))
    return WebSocketErrorToNetError(parser_.websocket_error());
  if (!frame_chunks->empty())
    return OK;
  return ERR_IO_PENDING;
}

void WebSocketBasicStream::OnReadComplete(
    ScopedVector<WebSocketFrameChunk>* frame_chunks,
    const CompletionCallback& callback,
    int result) {
  result = HandleReadResult(result, frame_chunks);
  if (result == ERR_IO_PENDING)
    result = ReadFrames(frame_chunks, callback);
  if (result != ERR_IO_PENDING)
    callback.Run(result);
}

}  // namespace net
