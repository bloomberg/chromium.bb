// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <limits>

#include "net/websockets/websocket.h"

#include "base/message_loop.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace net {

static const int kWebSocketPort = 80;
static const int kSecureWebSocketPort = 443;

static const char kServerHandshakeHeader[] =
    "HTTP/1.1 101 Web Socket Protocol Handshake\r\n";
static const size_t kServerHandshakeHeaderLength =
    sizeof(kServerHandshakeHeader) - 1;

static const char kUpgradeHeader[] = "Upgrade: WebSocket\r\n";
static const size_t kUpgradeHeaderLength = sizeof(kUpgradeHeader) - 1;

static const char kConnectionHeader[] = "Connection: Upgrade\r\n";
static const size_t kConnectionHeaderLength = sizeof(kConnectionHeader) - 1;

bool WebSocket::Request::is_secure() const {
  return url_.SchemeIs("wss");
}

WebSocket::WebSocket(Request* request, WebSocketDelegate* delegate)
    : ready_state_(INITIALIZED),
      mode_(MODE_INCOMPLETE),
      request_(request),
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
  pending_write_bufs_.push_back(CreateClientHandshakeMessage());
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

IOBufferWithSize* WebSocket::CreateClientHandshakeMessage() const {
  std::string msg;
  msg = "GET ";
  msg += request_->url().path();
  if (request_->url().has_query()) {
    msg += "?";
    msg += request_->url().query();
  }
  msg += " HTTP/1.1\r\n";
  msg += kUpgradeHeader;
  msg += kConnectionHeader;
  msg += "Host: ";
  msg += StringToLowerASCII(request_->url().host());
  if (request_->url().has_port()) {
    bool secure = request_->is_secure();
    int port = request_->url().EffectiveIntPort();
    if ((!secure &&
         port != kWebSocketPort && port != url_parse::PORT_UNSPECIFIED) ||
        (secure &&
         port != kSecureWebSocketPort && port != url_parse::PORT_UNSPECIFIED)) {
      msg += ":";
      msg += IntToString(port);
    }
  }
  msg += "\r\n";
  msg += "Origin: ";
  msg += StringToLowerASCII(request_->origin());
  msg += "\r\n";
  if (!request_->protocol().empty()) {
    msg += "WebSocket-Protocol: ";
    msg += request_->protocol();
    msg += "\r\n";
  }
  // TODO(ukai): Add cookie if necessary.
  msg += "\r\n";
  IOBufferWithSize* buf = new IOBufferWithSize(msg.size());
  memcpy(buf->data(), msg.data(), msg.size());
  return buf;
}

int WebSocket::CheckHandshake() {
  DCHECK(current_read_buf_);
  DCHECK(ready_state_ == CONNECTING);
  mode_ = MODE_INCOMPLETE;
  const char *start = current_read_buf_->StartOfBuffer() + read_consumed_len_;
  const char *p = start;
  size_t len = current_read_buf_->offset() - read_consumed_len_;
  if (len < kServerHandshakeHeaderLength) {
    return -1;
  }
  if (!memcmp(p, kServerHandshakeHeader, kServerHandshakeHeaderLength)) {
    mode_ = MODE_NORMAL;
  } else {
    int eoh = HttpUtil::LocateEndOfHeaders(p, len);
    if (eoh < 0)
      return -1;
    scoped_refptr<HttpResponseHeaders> headers(
        new HttpResponseHeaders(HttpUtil::AssembleRawHeaders(p, eoh)));
    if (headers->response_code() == 407) {
      mode_ = MODE_AUTHENTICATE;
      // TODO(ukai): Implement authentication handlers.
    }
    DLOG(INFO) << "non-normal websocket connection. "
               << "response_code=" << headers->response_code()
               << " mode=" << mode_;
    // Invalid response code.
    ready_state_ = CLOSED;
    return eoh;
  }
  const char* end = p + len + 1;
  p += kServerHandshakeHeaderLength;

  if (mode_ == MODE_NORMAL) {
    size_t header_size = end - p;
    if (header_size < kUpgradeHeaderLength)
      return -1;
    if (memcmp(p, kUpgradeHeader, kUpgradeHeaderLength)) {
      DLOG(INFO) << "Bad Upgrade Header "
                 << std::string(p, kUpgradeHeaderLength);
      ready_state_ = CLOSED;
      return p - start;
    }
    p += kUpgradeHeaderLength;

    header_size = end - p;
    if (header_size < kConnectionHeaderLength)
      return -1;
    if (memcmp(p, kConnectionHeader, kConnectionHeaderLength)) {
      DLOG(INFO) << "Bad Connection Header "
                 << std::string(p, kConnectionHeaderLength);
      ready_state_ = CLOSED;
      return p - start;
    }
    p += kConnectionHeaderLength;
  }
  int eoh = HttpUtil::LocateEndOfHeaders(start, len);
  if (eoh == -1)
    return eoh;
  scoped_refptr<HttpResponseHeaders> headers(
      new HttpResponseHeaders(HttpUtil::AssembleRawHeaders(start, eoh)));
  if (!ProcessHeaders(*headers)) {
    DLOG(INFO) << "Process Headers failed: "
               << std::string(start, eoh);
    ready_state_ = CLOSED;
    return eoh;
  }
  switch (mode_) {
    case MODE_NORMAL:
      if (CheckResponseHeaders()) {
        ready_state_ = OPEN;
      } else {
        ready_state_ = CLOSED;
      }
      break;
    default:
      ready_state_ = CLOSED;
      break;
  }
  if (ready_state_ == CLOSED)
    DLOG(INFO) << "CheckHandshake mode=" << mode_
               << " " << std::string(start, eoh);
  return eoh;
}

// Gets the value of the specified header.
// It assures only one header of |name| in |headers|.
// Returns true iff single header of |name| is found in |headers|
// and |value| is filled with the value.
// Returns false otherwise.
static bool GetSingleHeader(const HttpResponseHeaders& headers,
                            const std::string& name,
                            std::string* value) {
  std::string first_value;
  void* iter = NULL;
  if (!headers.EnumerateHeader(&iter, name, &first_value))
    return false;

  // Checks no more |name| found in |headers|.
  // Second call of EnumerateHeader() must return false.
  std::string second_value;
  if (headers.EnumerateHeader(&iter, name, &second_value))
    return false;
  *value = first_value;
  return true;
}

bool WebSocket::ProcessHeaders(const HttpResponseHeaders& headers) {
  if (!GetSingleHeader(headers, "websocket-origin", &ws_origin_))
    return false;

  if (!GetSingleHeader(headers, "websocket-location", &ws_location_))
    return false;

  if (!request_->protocol().empty()
      && !GetSingleHeader(headers, "websocket-protocol", &ws_protocol_))
    return false;
  return true;
}

bool WebSocket::CheckResponseHeaders() const {
  DCHECK(mode_ == MODE_NORMAL);
  if (!LowerCaseEqualsASCII(request_->origin(), ws_origin_.c_str()))
    return false;
  if (request_->location() != ws_location_)
    return false;
  if (request_->protocol() != ws_protocol_)
    return false;
  return true;
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
        int eoh = CheckHandshake();
        if (eoh < 0) {
          // Not enough data,  Retry when more data is available.
          return;
        }
        SkipReadBuffer(eoh);
      }
      if (ready_state_ != OPEN) {
        // Handshake failed.
        socket_stream_->Close();
        return;
      }
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
      while (p < end && (*p & 0x80) == 0x80) {
        if (length > std::numeric_limits<int>::max() / 128) {
          // frame length overflow.
          socket_stream_->Close();
          return;
        }
        length = length * 128 + (*p & 0x7f);
        ++p;
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
