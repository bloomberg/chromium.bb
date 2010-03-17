// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <limits>

#include "net/websockets/websocket.h"

#include "base/message_loop.h"
#include "net/websockets/websocket_handshake.h"

namespace net {

WebSocket::WebSocket(Request* request, WebSocketDelegate* delegate)
    : ready_state_(INITIALIZED),
      request_(request),
      handshake_(NULL),
      delegate_(delegate),
      origin_loop_(MessageLoop::current()),
      socket_stream_(NULL),
      max_pending_send_allowed_(0),
      current_read_buf_(NULL),
      read_consumed_len_(0),
      current_write_buf_(NULL) {
  DCHECK(request_.get());
  DCHECK(delegate_);
  DCHECK(origin_loop_);
}

WebSocket::~WebSocket() {
  DCHECK(ready_state_ == INITIALIZED || !delegate_);
  DCHECK(!socket_stream_);
  DCHECK(!delegate_);
}

void WebSocket::Connect() {
  DCHECK(ready_state_ == INITIALIZED);
  DCHECK(request_.get());
  DCHECK(delegate_);
  DCHECK(!socket_stream_);
  DCHECK(MessageLoop::current() == origin_loop_);

  socket_stream_ = new SocketStream(request_->url(), this);
  socket_stream_->set_context(request_->context());

  if (request_->host_resolver())
    socket_stream_->SetHostResolver(request_->host_resolver());
  if (request_->client_socket_factory())
    socket_stream_->SetClientSocketFactory(request_->client_socket_factory());

  AddRef();  // Release in DoClose().
  ready_state_ = CONNECTING;
  socket_stream_->Connect();
}

void WebSocket::Send(const std::string& msg) {
  DCHECK(ready_state_ == OPEN);
  DCHECK(MessageLoop::current() == origin_loop_);

  IOBufferWithSize* buf = new IOBufferWithSize(msg.size() + 2);
  char* p = buf->data();
  *p = '\0';
  memcpy(p + 1, msg.data(), msg.size());
  *(p + 1 + msg.size()) = '\xff';
  pending_write_bufs_.push_back(buf);
  SendPending();
}

void WebSocket::Close() {
  DCHECK(MessageLoop::current() == origin_loop_);

  if (ready_state_ == INITIALIZED) {
    DCHECK(!socket_stream_);
    ready_state_ = CLOSED;
    return;
  }
  if (ready_state_ != CLOSED) {
    DCHECK(socket_stream_);
    socket_stream_->Close();
    return;
  }
}

void WebSocket::DetachDelegate() {
  if (!delegate_)
    return;
  delegate_ = NULL;
  Close();
}

void WebSocket::OnConnected(SocketStream* socket_stream,
                            int max_pending_send_allowed) {
  DCHECK(socket_stream == socket_stream_);
  max_pending_send_allowed_ = max_pending_send_allowed;

  // Use |max_pending_send_allowed| as hint for initial size of read buffer.
  current_read_buf_ = new GrowableIOBuffer();
  current_read_buf_->SetCapacity(max_pending_send_allowed_);
  read_consumed_len_ = 0;

  DCHECK(!current_write_buf_);
  DCHECK(!handshake_.get());
  handshake_.reset(new WebSocketHandshake(
      request_->url(), request_->origin(), request_->location(),
      request_->protocol()));

  const std::string msg = handshake_->CreateClientHandshakeMessage();
  IOBufferWithSize* buf = new IOBufferWithSize(msg.size());
  memcpy(buf->data(), msg.data(), msg.size());
  pending_write_bufs_.push_back(buf);
  origin_loop_->PostTask(FROM_HERE,
                         NewRunnableMethod(this, &WebSocket::SendPending));
}

void WebSocket::OnSentData(SocketStream* socket_stream, int amount_sent) {
  DCHECK(socket_stream == socket_stream_);
  DCHECK(current_write_buf_);
  current_write_buf_->DidConsume(amount_sent);
  DCHECK_GE(current_write_buf_->BytesRemaining(), 0);
  if (current_write_buf_->BytesRemaining() == 0) {
    current_write_buf_ = NULL;
    pending_write_bufs_.pop_front();
  }
  origin_loop_->PostTask(FROM_HERE,
                         NewRunnableMethod(this, &WebSocket::SendPending));
}

void WebSocket::OnReceivedData(SocketStream* socket_stream,
                               const char* data, int len) {
  DCHECK(socket_stream == socket_stream_);
  AddToReadBuffer(data, len);
  origin_loop_->PostTask(FROM_HERE,
                         NewRunnableMethod(this, &WebSocket::DoReceivedData));
}

void WebSocket::OnClose(SocketStream* socket_stream) {
  origin_loop_->PostTask(FROM_HERE,
                         NewRunnableMethod(this, &WebSocket::DoClose));
}

void WebSocket::OnError(const SocketStream* socket_stream, int error) {
  origin_loop_->PostTask(FROM_HERE,
                         NewRunnableMethod(this, &WebSocket::DoError, error));
}

void WebSocket::SendPending() {
  DCHECK(MessageLoop::current() == origin_loop_);
  DCHECK(socket_stream_);
  if (!current_write_buf_) {
    if (pending_write_bufs_.empty())
      return;
    current_write_buf_ = new DrainableIOBuffer(
        pending_write_bufs_.front(), pending_write_bufs_.front()->size());
  }
  DCHECK_GT(current_write_buf_->BytesRemaining(), 0);
  bool sent = socket_stream_->SendData(
      current_write_buf_->data(),
      std::min(current_write_buf_->BytesRemaining(),
               max_pending_send_allowed_));
  DCHECK(sent);
}

void WebSocket::DoReceivedData() {
  DCHECK(MessageLoop::current() == origin_loop_);
  switch (ready_state_) {
    case CONNECTING:
      {
        DCHECK(handshake_.get());
        DCHECK(current_read_buf_);
        const char* data =
            current_read_buf_->StartOfBuffer() + read_consumed_len_;
        size_t len = current_read_buf_->offset() - read_consumed_len_;
        int eoh = handshake_->ReadServerHandshake(data, len);
        if (eoh < 0) {
          // Not enough data,  Retry when more data is available.
          return;
        }
        SkipReadBuffer(eoh);
      }
      if (handshake_->mode() != WebSocketHandshake::MODE_CONNECTED) {
        // Handshake failed.
        socket_stream_->Close();
        return;
      }
      ready_state_ = OPEN;
      if (delegate_)
        delegate_->OnOpen(this);
      if (current_read_buf_->offset() == read_consumed_len_) {
        // No remaining data after handshake message.
        break;
      }
      // FALL THROUGH
    case OPEN:
      ProcessFrameData();
      break;

    case CLOSED:
      // Closed just after DoReceivedData is queued on |origin_loop_|.
      break;
    default:
      NOTREACHED();
      break;
  }
}

void WebSocket::ProcessFrameData() {
  DCHECK(current_read_buf_);
  const char* start_frame =
      current_read_buf_->StartOfBuffer() + read_consumed_len_;
  const char* next_frame = start_frame;
  const char* p = next_frame;
  const char* end =
      current_read_buf_->StartOfBuffer() + current_read_buf_->offset();
  while (p < end) {
    unsigned char frame_byte = static_cast<unsigned char>(*p++);
    if ((frame_byte & 0x80) == 0x80) {
      int length = 0;
      while (p < end) {
        if (length > std::numeric_limits<int>::max() / 128) {
          // frame length overflow.
          socket_stream_->Close();
          return;
        }
        unsigned char c = static_cast<unsigned char>(*p);
        length = length * 128 + (c & 0x7f);
        ++p;
        if ((c & 0x80) != 0x80)
          break;
      }
      // Checks if the frame body hasn't been completely received yet.
      // It also checks the case the frame length bytes haven't been completely
      // received yet, because p == end and length > 0 in such case.
      if (p + length < end) {
        p += length;
        next_frame = p;
      } else {
        break;
      }
    } else {
      const char* msg_start = p;
      while (p < end && *p != '\xff')
        ++p;
      if (p < end && *p == '\xff') {
        if (frame_byte == 0x00 && delegate_)
          delegate_->OnMessage(this, std::string(msg_start, p - msg_start));
        ++p;
        next_frame = p;
      }
    }
  }
  SkipReadBuffer(next_frame - start_frame);
}

void WebSocket::AddToReadBuffer(const char* data, int len) {
  DCHECK(current_read_buf_);
  // Check if |current_read_buf_| has enough space to store |len| of |data|.
  if (len >= current_read_buf_->RemainingCapacity()) {
    current_read_buf_->SetCapacity(
        current_read_buf_->offset() + len);
  }

  DCHECK(current_read_buf_->RemainingCapacity() >= len);
  memcpy(current_read_buf_->data(), data, len);
  current_read_buf_->set_offset(current_read_buf_->offset() + len);
}

void WebSocket::SkipReadBuffer(int len) {
  if (len == 0)
    return;
  DCHECK_GT(len, 0);
  read_consumed_len_ += len;
  int remaining = current_read_buf_->offset() - read_consumed_len_;
  DCHECK_GE(remaining, 0);
  if (remaining < read_consumed_len_ &&
      current_read_buf_->RemainingCapacity() < read_consumed_len_) {
    // Pre compaction:
    // 0             v-read_consumed_len_  v-offset               v- capacity
    // |..processed..| .. remaining ..     | .. RemainingCapacity |
    //
    memmove(current_read_buf_->StartOfBuffer(),
            current_read_buf_->StartOfBuffer() + read_consumed_len_,
            remaining);
    read_consumed_len_ = 0;
    current_read_buf_->set_offset(remaining);
    // Post compaction:
    // 0read_consumed_len_  v- offset                             v- capacity
    // |.. remaining ..     | ..  RemainingCapacity  ...          |
    //
  }
}

void WebSocket::DoClose() {
  DCHECK(MessageLoop::current() == origin_loop_);
  WebSocketDelegate* delegate = delegate_;
  delegate_ = NULL;
  ready_state_ = CLOSED;
  if (!socket_stream_)
    return;
  socket_stream_ = NULL;
  if (delegate)
    delegate->OnClose(this);
  Release();
}

void WebSocket::DoError(int error) {
  DCHECK(MessageLoop::current() == origin_loop_);
  if (delegate_)
    delegate_->OnError(this, error);
}

}  // namespace net
