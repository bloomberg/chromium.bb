// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/websocket_connection.h"

#include <map>
#include <vector>

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/sha1.h"
#include "base/single_thread_task_runner.h"
#include "base/string_split.h"
#include "base/sys_byteorder.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

namespace remoting {

namespace {

const int kReadBufferSize = 1024;
const char kLineSeparator[] = "\r\n";
const char kHeaderEndMarker[] = "\r\n\r\n";
const char kHeaderKeyValueSeparator[] = ": ";
const int kMaskLength = 4;

// Maximum frame length that can be encoded without extended length filed.
const uint32 kMaxNotExtendedLength = 125;

// Maximum frame length that can be encoded in 16 bits.
const uint32 kMax16BitLength = 65535;

// Special values of the length field used to extend frame length to 16 or 64
// bits.
const uint32 kLength16BitMarker = 126;
const uint32 kLength64BitMarker = 127;

// Fixed value specified in RFC6455. It's used to compute accept token sent to
// the client in Sec-WebSocket-Accept key.
const char kWebsocketKeySalt[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

}  // namespace

WebSocketConnection::WebSocketConnection()
    : delegate_(NULL),
      maximum_message_size_(0),
      state_(READING_HEADERS),
      receiving_message_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

WebSocketConnection::~WebSocketConnection() {
  Close();
}

void WebSocketConnection::Start(
    scoped_ptr<net::StreamSocket> socket,
    ConnectedCallback connected_callback) {
  socket_ = socket.Pass();
  connected_callback_ = connected_callback;
  reader_.Init(socket_.get(), base::Bind(
      &WebSocketConnection::OnSocketReadResult, base::Unretained(this)));
  writer_.Init(socket_.get(), base::Bind(
      &WebSocketConnection::OnSocketWriteError, base::Unretained(this)));
}

void WebSocketConnection::Accept(Delegate* delegate) {
  DCHECK_EQ(state_, HEADERS_READ);

  state_ = ACCEPTED;
  delegate_ = delegate;

  std::string accept_key =
      base::SHA1HashString(websocket_key_ + kWebsocketKeySalt);
  std::string accept_key_base64;
  bool result = base::Base64Encode(accept_key, &accept_key_base64);
  DCHECK(result);

  std::string handshake;
  handshake += "HTTP/1.1 101 Switching Protocol";
  handshake += kLineSeparator;
  handshake += "Upgrade: websocket";
  handshake += kLineSeparator;
  handshake += "Connection: Upgrade";
  handshake += kLineSeparator;
  handshake += "Sec-WebSocket-Accept: " + accept_key_base64;
  handshake += kHeaderEndMarker;

  scoped_refptr<net::IOBufferWithSize> buffer =
      new net::IOBufferWithSize(handshake.size());
  memcpy(buffer->data(), handshake.data(), handshake.size());
  writer_.Write(buffer, base::Closure());
}

void WebSocketConnection::Reject() {
  DCHECK_EQ(state_, HEADERS_READ);

  state_ = CLOSED;
  std::string response = "HTTP/1.1 401 Unauthorized";
  response += kHeaderEndMarker;
  scoped_refptr<net::IOBufferWithSize> buffer =
      new net::IOBufferWithSize(response.size());
  memcpy(buffer->data(), response.data(), response.size());
  writer_.Write(buffer, base::Closure());
}

void WebSocketConnection::set_maximum_message_size(uint64 size) {
  maximum_message_size_ = size;
}

void WebSocketConnection::SendText(const std::string& text) {
  SendFragment(OPCODE_TEXT_FRAME, text);
}

void WebSocketConnection::Close() {
  switch (state_) {
    case READING_HEADERS:
      break;

    case HEADERS_READ:
      Reject();
      break;

    case ACCEPTED:
      SendFragment(OPCODE_CLOSE, std::string());
      break;

    case CLOSED:
      break;
  }
  state_ = CLOSED;
}

void WebSocketConnection::CloseOnError() {
  State old_state_ = state_;
  Close();
  if (old_state_ == ACCEPTED) {
    DCHECK(delegate_);
    delegate_->OnWebSocketClosed();
  }
}

void WebSocketConnection::OnSocketReadResult(scoped_refptr<net::IOBuffer> data,
                                             int result) {
  if (result <= 0) {
    if (result != 0) {
      LOG(ERROR) << "Error when trying to read from WebSocket connection: "
                << result;
    }
    CloseOnError();
    return;
  }

  switch (state_) {
    case READING_HEADERS: {
      headers_.append(data->data(), data->data() + result);
      size_t header_end_pos = headers_.find(kHeaderEndMarker);
      if (header_end_pos != std::string::npos) {
        bool result;
        if (header_end_pos != headers_.size() - strlen(kHeaderEndMarker)) {
          LOG(ERROR) << "WebSocket client tried writing data before handshake "
            "has finished.";
          DCHECK(!connected_callback_.is_null());
          state_ = CLOSED;
          result = false;
        } else {
          // Crop newline symbols from the end.
          headers_.resize(header_end_pos);

          result = ParseHeaders();
          if (!result) {
            state_ = CLOSED;
          } else {
            state_ = HEADERS_READ;
          }
        }
        ConnectedCallback cb(connected_callback_);
        connected_callback_.Reset();
        cb.Run(result);
      }
      break;
    }

    case HEADERS_READ:
      LOG(ERROR) << "Received unexpected data before websocket "
          "connection is accepted.";
      CloseOnError();
      break;

    case ACCEPTED:
      DCHECK(delegate_);
      received_data_.append(data->data(), data->data() + result);
      ProcessData();

    case CLOSED:
      // Ignore anything received after connection is rejected or closed.
      break;
  }
}

void WebSocketConnection::ProcessData() {
  DCHECK_EQ(state_, ACCEPTED);

  if (received_data_.size() < 2) {
    // Header hasn't been received yet.
    return;
  }

  bool fin_bit = (received_data_.data()[0] & 0x80) != 0;

  // 3 bits after FIN are reserved for WebSocket extensions. RFC6455 requires
  // that endpoint fails connection if any of these bits is set while no
  // extension that uses these bits was negotiated.
  int rsv_bits = received_data_.data()[0] & 0x70;
  if (rsv_bits != 0) {
    LOG(ERROR) << "Incoming has unsupported RSV bits set.";
    CloseOnError();
    return;
  }

  int opcode = received_data_.data()[0] & 0x0f;

  int mask_bit = received_data_.data()[1] & 0x80;
  if (mask_bit == 0) {
    LOG(ERROR) << "Incoming frame is not masked.";
    CloseOnError();
    return;
  }

  // Length field has variable size in each WebSocket frame - it's either 1, 3
  // or 9 bytes with the first bit always reserved for MASK flag. The first byte
  // is set to 126 or 127 for 16 and 64 bit extensions respectively. Code below
  // extracts |length| value and sets |length_field_size| accordingly.
  int length_field_size = 1;
  uint64 length = received_data_.data()[1] & 0x7F;
  if (length == kLength16BitMarker) {
    if (received_data_.size() < 4) {
      // Haven't received the whole frame header yet.
      return;
    }
    length_field_size = 3;
    length = base::NetToHost16(
        *reinterpret_cast<const uint16*>(received_data_.data() + 2));
  } else if (length == kLength64BitMarker) {
    if (received_data_.size() < 10) {
      // Haven't received the whole frame header yet.
      return;
    }
    length_field_size = 9;
    length = base::NetToHost64(
        *reinterpret_cast<const uint64*>(received_data_.data() + 2));
  }

  int payload_position = 1 + length_field_size + kMaskLength;

  // Check that the size of the frame is below the limit. It needs to be done
  // before we read the payload to avoid allocating buffer for a bogus frame
  // that is too big.
  if (maximum_message_size_ > 0 && length > maximum_message_size_) {
    LOG(ERROR) << "Client tried to send a fragment that is bigger than "
        "the maximum message size of " << maximum_message_size_;
    CloseOnError();
    return;
  }

  if (received_data_.size() < payload_position + length) {
    // Haven't received the whole frame yet.
    return;
  }

  // Unmask the payload.
  if (mask_bit) {
    const char* mask = received_data_.data() + length_field_size + 1;
    UnmaskPayload(
      mask,
      const_cast<char*>(received_data_.data()) + payload_position, length);
  }

  const char* payload = received_data_.data() + payload_position;

  if (opcode < 0x8) {
    if (maximum_message_size_ > 0 &&
        current_message_.size() + length > maximum_message_size_) {
      LOG(ERROR) << "Client tried to send a message that is bigger than "
          "the maximum message size of " << maximum_message_size_;
      CloseOnError();
      return;
    }

    // Non-control message.
    current_message_.append(payload, payload + length);
  } else {
    // Control message.
    if (!fin_bit) {
      LOG(ERROR) << "Received fragmented control message.";
      CloseOnError();
      return;
    }
    if (length > kMaxNotExtendedLength) {
      LOG(ERROR) << "Received control message that is larger than 125 bytes.";
      CloseOnError();
      return;
    }
  }

  switch (opcode) {
    case OPCODE_CONTINUATION:
      if (!receiving_message_) {
        LOG(ERROR) << "Received unexpected continuation frame.";
        CloseOnError();
        return;
      }
      break;

    case OPCODE_TEXT_FRAME:
    case OPCODE_BINARY_FRAME:
      if (receiving_message_) {
        LOG(ERROR) << "Received unexpected new start frame in a middle of "
            "a message.";
        CloseOnError();
        return;
      }
      break;

    case OPCODE_CLOSE:
      Close();
      delegate_->OnWebSocketClosed();
      return;

    case OPCODE_PING:
      SendFragment(OPCODE_PONG, std::string(payload, payload + length));
      break;

    case OPCODE_PONG:
      break;

    default:
      LOG(ERROR) << "Received invalid opcode: " << opcode;
      CloseOnError();
      return;
  }

  // Remove the frame from |received_data_|.
  received_data_.erase(0, payload_position + length);

  // Post a task to process the data left in the buffer, if any.
  if (!received_data_.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&WebSocketConnection::ProcessData,
                              weak_factory_.GetWeakPtr()));
  }

  // Handle payload in non-control messages. Delegate can be called only at the
  // end of this function
  if (opcode < 0x8) {
    if (!fin_bit) {
      receiving_message_ = true;
    } else {
      receiving_message_ = false;
      std::string msg;
      msg.swap(current_message_);
      delegate_->OnWebSocketMessage(msg);
    }
  }
}

void WebSocketConnection::SendFragment(WebsocketOpcode opcode,
                                      const std::string& payload) {
  DCHECK_EQ(state_, ACCEPTED);

  int length_field_size = 1;
  if (payload.size() > kMax16BitLength) {
    length_field_size = 9;
  } else if (payload.size() > kMaxNotExtendedLength) {
    length_field_size = 3;
  }

  scoped_refptr<net::IOBufferWithSize> buffer =
      new net::IOBufferWithSize(1 + length_field_size + payload.size());

  // Always set FIN flag because we never fragment outgoing messages.
  buffer->data()[0] = opcode | 0x80;

  if (payload.size() > kMax16BitLength) {
    uint64 size = base::HostToNet64(payload.size());
    buffer->data()[1] = kLength64BitMarker;
    memcpy(buffer->data() + 2, reinterpret_cast<char*>(&size), sizeof(size));
  } else if (payload.size() > kMaxNotExtendedLength) {
    uint16 size = base::HostToNet16(payload.size());
    buffer->data()[1] = kLength16BitMarker;
    memcpy(buffer->data() + 2, reinterpret_cast<char*>(&size), sizeof(size));
  } else {
    buffer->data()[1] = payload.size();
  }
  memcpy(buffer->data() + 1 + length_field_size,
         payload.data(), payload.size());

  writer_.Write(buffer, base::Closure());
}

bool WebSocketConnection::ParseHeaders() {
  std::vector<std::string> lines;
  base::SplitStringUsingSubstr(headers_, kLineSeparator, &lines);

  // Parse request line.
  std::vector<std::string> request_parts;
  base::SplitString(lines[0], ' ', &request_parts);
  if (request_parts.size() != 3 ||
      request_parts[0] != "GET" ||
      request_parts[2] != "HTTP/1.1") {
    LOG(ERROR) << "Invalid Request-Line: " << headers_[0];
    return false;
  }
  request_path_ = request_parts[1];

  std::map<std::string, std::string> headers;

  for (size_t i = 1; i < lines.size(); ++i) {
    std::string separator(kHeaderKeyValueSeparator);
    size_t pos = lines[i].find(separator);
    if (pos == std::string::npos || pos == 0) {
      LOG(ERROR) << "Invalid header line: " << lines[i];
      return false;
    }
    std::string key = lines[i].substr(0, pos);
    if (headers.find(key) != headers.end()) {
      LOG(ERROR) << "Duplicate header value: " << key;
      return false;
    }
    headers[key] = lines[i].substr(pos + separator.size());
  }

  std::map<std::string, std::string>::iterator it = headers.find("Connection");
  if (it == headers.end() || it->second != "Upgrade") {
    LOG(ERROR) << "Connection header is missing or invalid.";
    return false;
  }

  it = headers.find("Upgrade");
  if (it == headers.end() || it->second != "websocket") {
    LOG(ERROR) << "Upgrade header is missing or invalid.";
    return false;
  }

  it = headers.find("Host");
  if (it == headers.end()) {
    LOG(ERROR) << "Host header is missing.";
    return false;
  }
  request_host_ = it->second;

  it = headers.find("Sec-WebSocket-Version");
  if (it == headers.end()) {
    LOG(ERROR) << "Sec-WebSocket-Version header is missing.";
    return false;
  }
  if (it->second != "13") {
    LOG(ERROR) << "Unsupported WebSocket protocol version: " << it->second;
    return false;
  }

  it = headers.find("Origin");
  if (it == headers.end()) {
    LOG(ERROR) << "Origin header is missing.";
    return false;
  }
  origin_ = it->second;

  it = headers.find("Sec-WebSocket-Key");
  if (it == headers.end()) {
    LOG(ERROR) << "Sec-WebSocket-Key header is missing.";
    return false;
  }
  websocket_key_ = it->second;

  return true;
}

void WebSocketConnection::UnmaskPayload(const char* mask,
                                        char* payload, int payload_length) {
  for (int i = 0; i < payload_length; ++i) {
    payload[i] = payload[i] ^ mask[i % kMaskLength];
  }
}

void WebSocketConnection::OnSocketWriteError(int error) {
  LOG(ERROR) << "Failed to write to a WebSocket. Error: " << error;
  CloseOnError();
}

}  // namespace remoting
