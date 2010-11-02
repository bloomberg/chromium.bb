// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <limits>

#include "net/websockets/websocket.h"

#include "base/message_loop.h"
#include "net/base/host_resolver.h"
#include "net/websockets/websocket_handshake.h"
#include "net/websockets/websocket_handshake_draft75.h"

namespace net {

static const char kClosingFrame[2] = {'\xff', '\x00'};
static int64 kClosingHandshakeTimeout = 1000;  // msec.

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
      current_write_buf_(NULL),
      server_closing_handshake_(false),
      client_closing_handshake_(false),
      closing_handshake_started_(false),
      force_close_task_(NULL),
      closing_handshake_timeout_(kClosingHandshakeTimeout) {
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
  if (ready_state_ == CLOSING || ready_state_ == CLOSED) {
    return;
  }
  if (client_closing_handshake_) {
    // We must not send any data after we start the WebSocket closing handshake.
    return;
  }
  DCHECK(ready_state_ == OPEN);
  DCHECK(MessageLoop::current() == origin_loop_);

  IOBufferWithSize* buf = new IOBufferWithSize(msg.size() + 2);
  char* p = buf->data();
  *p = '\0';
  memcpy(p + 1, msg.data(), msg.size());
  *(p + 1 + msg.size()) = '\xff';
  pending_write_bufs_.push_back(make_scoped_refptr(buf));
  SendPending();
}

void WebSocket::Close() {
  DCHECK(MessageLoop::current() == origin_loop_);

  // If connection has not yet started, do nothing.
  if (ready_state_ == INITIALIZED) {
    DCHECK(!socket_stream_);
    ready_state_ = CLOSED;
    return;
  }

  // If the readyState attribute is in the CLOSING or CLOSED state, do nothing
  if (ready_state_ == CLOSING || ready_state_ == CLOSED)
    return;

  if (request_->version() == DRAFT75) {
    DCHECK(socket_stream_);
    socket_stream_->Close();
    return;
  }

  // If the WebSocket connection is not yet established, fail the WebSocket
  // connection and set the readyState attribute's value to CLOSING.
  if (ready_state_ == CONNECTING) {
    ready_state_ = CLOSING;
    origin_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &WebSocket::FailConnection));
  }

  // If the WebSocket closing handshake has not yet been started, start
  // the WebSocket closing handshake and set the readyState attribute's value
  // to CLOSING.
  if (!closing_handshake_started_) {
    ready_state_ = CLOSING;
    origin_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &WebSocket::StartClosingHandshake));
  }

  // Otherwise, set the readyState attribute's value to CLOSING.
  ready_state_ = CLOSING;
}

void WebSocket::DetachDelegate() {
  if (!delegate_)
    return;
  delegate_ = NULL;
  if (ready_state_ == INITIALIZED) {
    DCHECK(!socket_stream_);
    ready_state_ = CLOSED;
    return;
  }
  if (ready_state_ != CLOSED) {
    DCHECK(socket_stream_);
    socket_stream_->Close();
  }
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
  switch (request_->version()) {
    case DEFAULT_VERSION:
      handshake_.reset(new WebSocketHandshake(
          request_->url(), request_->origin(), request_->location(),
          request_->protocol()));
      break;
    case DRAFT75:
      handshake_.reset(new WebSocketHandshakeDraft75(
          request_->url(), request_->origin(), request_->location(),
          request_->protocol()));
      break;
    default:
      NOTREACHED() << "Unexpected protocol version:" << request_->version();
  }

  const std::string msg = handshake_->CreateClientHandshakeMessage();
  IOBufferWithSize* buf = new IOBufferWithSize(msg.size());
  memcpy(buf->data(), msg.data(), msg.size());
  pending_write_bufs_.push_back(make_scoped_refptr(buf));
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
  origin_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &WebSocket::DoSocketError, error));
}

void WebSocket::SendPending() {
  DCHECK(MessageLoop::current() == origin_loop_);
  if (!socket_stream_) {
    DCHECK_EQ(CLOSED, ready_state_);
    return;
  }
  if (!current_write_buf_) {
    if (pending_write_bufs_.empty()) {
      if (client_closing_handshake_) {
        // Already sent 0xFF and 0x00 bytes.
        // *The WebSocket closing handshake has started.*
        closing_handshake_started_ = true;
        if (server_closing_handshake_) {
          // 4.2 3-8-3 If the WebSocket connection is not already closed,
          // then close the WebSocket connection.
          // *The WebSocket closing handshake has finished*
          socket_stream_->Close();
        } else {
          // 5. Wait a user-agent-determined length of time, or until the
          // WebSocket connection is closed.
          force_close_task_ =
              NewRunnableMethod(this, &WebSocket::DoForceCloseConnection);
          origin_loop_->PostDelayedTask(
              FROM_HERE, force_close_task_, closing_handshake_timeout_);
        }
      }
      return;
    }
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
  scoped_refptr<WebSocket> protect(this);
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
    case CLOSING:  // need to process closing-frame from server.
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
  if (server_closing_handshake_) {
    // Any data on the connection after the 0xFF frame is discarded.
    return;
  }
  scoped_refptr<WebSocket> protect(this);
  const char* start_frame =
      current_read_buf_->StartOfBuffer() + read_consumed_len_;
  const char* next_frame = start_frame;
  const char* p = next_frame;
  const char* end =
      current_read_buf_->StartOfBuffer() + current_read_buf_->offset();
  while (p < end) {
    // Let /error/ be false.
    bool error = false;

    // Handle the /frame type/ byte as follows.
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
        if (request_->version() != DRAFT75 &&
            frame_byte == 0xFF && length == 0) {
          // 4.2 Data framing 3. Handle the /frame type/ byte.
          // 8. If the /frame type/ is 0xFF and the /length/ was 0, then
          // run the following substeps:
          // 1. If the WebSocket closing handshake has not yet started, then
          // start the WebSocket closing handshake.
          server_closing_handshake_ = true;
          if (!closing_handshake_started_) {
            origin_loop_->PostTask(
                FROM_HERE,
                NewRunnableMethod(this, &WebSocket::StartClosingHandshake));
          } else {
            // If the WebSocket closing handshake has been started and
            // the WebSocket connection is not already closed, then close
            // the WebSocket connection.
            socket_stream_->Close();
          }
          return;
        }
        // 4.2 3-8 Otherwise, let /error/ be true.
        error = true;
      } else {
        // Not enough data in buffer.
        break;
      }
    } else {
      const char* msg_start = p;
      while (p < end && *p != '\xff')
        ++p;
      if (p < end && *p == '\xff') {
        if (frame_byte == 0x00) {
          if (delegate_) {
            delegate_->OnMessage(this, std::string(msg_start, p - msg_start));
          }
        } else {
          // Otherwise, discard the data and let /error/ to be true.
          error = true;
        }
        ++p;
        next_frame = p;
      }
    }
    // If /error/ is true, then *a WebSocket error has been detected.*
    if (error && delegate_)
      delegate_->OnError(this);
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

void WebSocket::StartClosingHandshake() {
  // 4.2 *start the WebSocket closing handshake*.
  if (closing_handshake_started_ || client_closing_handshake_) {
    // 1. If the WebSocket closing handshake has started, then abort these
    // steps.
    return;
  }
  // 2.,3. Send a 0xFF and 0x00 byte to the server.
  client_closing_handshake_ = true;
  IOBufferWithSize* buf = new IOBufferWithSize(2);
  memcpy(buf->data(), kClosingFrame, 2);
  pending_write_bufs_.push_back(make_scoped_refptr(buf));
  SendPending();
}

void WebSocket::DoForceCloseConnection() {
  // 4.2 *start the WebSocket closing handshake*
  // 6. If the WebSocket connection is not already closed, then close the
  // WebSocket connection.  (If this happens, then the closing handshake
  // doesn't finish.)
  DCHECK(MessageLoop::current() == origin_loop_);
  force_close_task_ = NULL;
  FailConnection();
}

void WebSocket::FailConnection() {
  DCHECK(MessageLoop::current() == origin_loop_);
  // 6.1 Client-initiated closure.
  // *fail the WebSocket connection*.
  // the user agent must close the WebSocket connection, and may report the
  // problem to the user.
  if (!socket_stream_)
    return;
  socket_stream_->Close();
}

void WebSocket::DoClose() {
  DCHECK(MessageLoop::current() == origin_loop_);
  if (force_close_task_) {
    // WebSocket connection is closed while waiting a user-agent-determined
    // length of time after *The WebSocket closing handshake has started*.
    force_close_task_->Cancel();
    force_close_task_ = NULL;
  }
  WebSocketDelegate* delegate = delegate_;
  delegate_ = NULL;
  ready_state_ = CLOSED;
  if (!socket_stream_)
    return;
  socket_stream_ = NULL;
  if (delegate)
    delegate->OnClose(this,
                      server_closing_handshake_ && closing_handshake_started_);
  Release();
}

void WebSocket::DoSocketError(int error) {
  DCHECK(MessageLoop::current() == origin_loop_);
  if (delegate_)
    delegate_->OnSocketError(this, error);
}

}  // namespace net
