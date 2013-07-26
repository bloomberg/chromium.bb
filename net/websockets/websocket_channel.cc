// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_channel.h"

#include <algorithm>

#include "base/basictypes.h"  // for size_t
#include "base/bind.h"
#include "base/safe_numerics.h"
#include "base/strings/string_util.h"
#include "net/base/big_endian.h"
#include "net/base/io_buffer.h"
#include "net/base/net_log.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_event_interface.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_mux.h"
#include "net/websockets/websocket_stream.h"

namespace net {

namespace {

const int kDefaultSendQuotaLowWaterMark = 1 << 16;
const int kDefaultSendQuotaHighWaterMark = 1 << 17;
const size_t kWebSocketCloseCodeLength = 2;

// This uses type uint64 to match the definition of
// WebSocketFrameHeader::payload_length in websocket_frame.h.
const uint64 kMaxControlFramePayload = 125;

// Concatenate the data from two IOBufferWithSize objects into a single one.
IOBufferWithSize* ConcatenateIOBuffers(
    const scoped_refptr<IOBufferWithSize>& part1,
    const scoped_refptr<IOBufferWithSize>& part2) {
  int newsize = part1->size() + part2->size();
  IOBufferWithSize* newbuffer = new IOBufferWithSize(newsize);
  std::copy(part1->data(), part1->data() + part1->size(), newbuffer->data());
  std::copy(part2->data(),
            part2->data() + part2->size(),
            newbuffer->data() + part1->size());
  return newbuffer;
}

}  // namespace

// A class to encapsulate a set of frames and information about the size of
// those frames.
class WebSocketChannel::SendBuffer {
 public:
  SendBuffer() : total_bytes_(0) {}

  // Add a WebSocketFrameChunk to the buffer and increase total_bytes_.
  void AddFrame(scoped_ptr<WebSocketFrameChunk> chunk);

  // Return a pointer to the frames_ for write purposes.
  ScopedVector<WebSocketFrameChunk>* frames() { return &frames_; }

 private:
  // The frames_ that will be sent in the next call to WriteFrames().
  ScopedVector<WebSocketFrameChunk> frames_;

  // The total size of the buffers in |frames_|. This will be used to measure
  // the throughput of the link.
  // TODO(ricea): Measure the throughput of the link.
  size_t total_bytes_;
};

void WebSocketChannel::SendBuffer::AddFrame(
    scoped_ptr<WebSocketFrameChunk> chunk) {
  total_bytes_ += chunk->data->size();
  frames_.push_back(chunk.release());
}

// Implementation of WebSocketStream::ConnectDelegate that simply forwards the
// calls on to the WebSocketChannel that created it.
class WebSocketChannel::ConnectDelegate
    : public WebSocketStream::ConnectDelegate {
 public:
  explicit ConnectDelegate(WebSocketChannel* creator) : creator_(creator) {}

  virtual void OnSuccess(scoped_ptr<WebSocketStream> stream) OVERRIDE {
    creator_->OnConnectSuccess(stream.Pass());
  }

  virtual void OnFailure(uint16 websocket_error) OVERRIDE {
    creator_->OnConnectFailure(websocket_error);
  }

 private:
  // A pointer to the WebSocketChannel that created us. We do not need to worry
  // about this pointer being stale, because deleting WebSocketChannel cancels
  // the connect process, deleting this object and preventing its callbacks from
  // being called.
  WebSocketChannel* const creator_;

  DISALLOW_COPY_AND_ASSIGN(ConnectDelegate);
};

WebSocketChannel::WebSocketChannel(
    const GURL& socket_url,
    scoped_ptr<WebSocketEventInterface> event_interface)
    : socket_url_(socket_url),
      event_interface_(event_interface.Pass()),
      send_quota_low_water_mark_(kDefaultSendQuotaLowWaterMark),
      send_quota_high_water_mark_(kDefaultSendQuotaHighWaterMark),
      current_send_quota_(0),
      closing_code_(0),
      state_(FRESHLY_CONSTRUCTED) {}

WebSocketChannel::~WebSocketChannel() {
  // The stream may hold a pointer to read_frame_chunks_, and so it needs to be
  // destroyed first.
  stream_.reset();
}

void WebSocketChannel::SendAddChannelRequest(
    const std::vector<std::string>& requested_subprotocols,
    const GURL& origin,
    URLRequestContext* url_request_context) {
  // Delegate to the tested version.
  SendAddChannelRequestWithFactory(
      requested_subprotocols,
      origin,
      url_request_context,
      base::Bind(&WebSocketStream::CreateAndConnectStream));
}

bool WebSocketChannel::InClosingState() const {
  // We intentionally do not support state RECV_CLOSED here, because it is only
  // used in one code path and should not leak into the code in general.
  DCHECK_NE(RECV_CLOSED, state_)
      << "InClosingState called with state_ == RECV_CLOSED";
  return state_ == SEND_CLOSED || state_ == CLOSE_WAIT || state_ == CLOSED;
}

void WebSocketChannel::SendFrame(bool fin,
                                 WebSocketFrameHeader::OpCode op_code,
                                 const std::vector<char>& data) {
  if (data.size() > INT_MAX) {
    NOTREACHED() << "Frame size sanity check failed";
    return;
  }
  if (stream_ == NULL) {
    LOG(DFATAL) << "Got SendFrame without a connection established; "
                << "misbehaving renderer? fin=" << fin << " op_code=" << op_code
                << " data.size()=" << data.size();
    return;
  }
  if (InClosingState()) {
    VLOG(1) << "SendFrame called in state " << state_
            << ". This may be a bug, or a harmless race.";
    return;
  }
  if (state_ != CONNECTED) {
    NOTREACHED() << "SendFrame() called in state " << state_;
    return;
  }
  if (data.size() > base::checked_numeric_cast<size_t>(current_send_quota_)) {
    FailChannel(SEND_GOING_AWAY,
                kWebSocketMuxErrorSendQuotaViolation,
                "Send quota exceeded");
    return;
  }
  if (!WebSocketFrameHeader::IsKnownDataOpCode(op_code)) {
    LOG(DFATAL) << "Got SendFrame with bogus op_code " << op_code
                << "; misbehaving renderer? fin=" << fin
                << " data.size()=" << data.size();
    return;
  }
  current_send_quota_ -= data.size();
  // TODO(ricea): If current_send_quota_ has dropped below
  // send_quota_low_water_mark_, we may want to consider increasing the "low
  // water mark" and "high water mark", but only if we think we are not
  // saturating the link to the WebSocket server.
  // TODO(ricea): For kOpCodeText, do UTF-8 validation?
  scoped_refptr<IOBufferWithSize> buffer(new IOBufferWithSize(data.size()));
  std::copy(data.begin(), data.end(), buffer->data());
  SendIOBufferWithSize(fin, op_code, buffer);
}

void WebSocketChannel::SendFlowControl(int64 quota) {
  DCHECK_EQ(CONNECTED, state_);
  // TODO(ricea): Add interface to WebSocketStream and implement.
  // stream_->SendFlowControl(quota);
}

void WebSocketChannel::StartClosingHandshake(uint16 code,
                                             const std::string& reason) {
  if (InClosingState()) {
    VLOG(1) << "StartClosingHandshake called in state " << state_
            << ". This may be a bug, or a harmless race.";
    return;
  }
  if (state_ != CONNECTED) {
    NOTREACHED() << "StartClosingHandshake() called in state " << state_;
    return;
  }
  // TODO(ricea): Validate |code|? Check that |reason| is valid UTF-8?
  // TODO(ricea): There should be a timeout for the closing handshake.
  SendClose(code, reason);  // Sets state_ to SEND_CLOSED
}

void WebSocketChannel::SendAddChannelRequestForTesting(
    const std::vector<std::string>& requested_subprotocols,
    const GURL& origin,
    URLRequestContext* url_request_context,
    const WebSocketStreamFactory& factory) {
  SendAddChannelRequestWithFactory(
      requested_subprotocols, origin, url_request_context, factory);
}

void WebSocketChannel::SendAddChannelRequestWithFactory(
    const std::vector<std::string>& requested_subprotocols,
    const GURL& origin,
    URLRequestContext* url_request_context,
    const WebSocketStreamFactory& factory) {
  DCHECK_EQ(FRESHLY_CONSTRUCTED, state_);
  scoped_ptr<WebSocketStream::ConnectDelegate> connect_delegate(
      new ConnectDelegate(this));
  stream_request_ = factory.Run(socket_url_,
                                requested_subprotocols,
                                origin,
                                url_request_context,
                                BoundNetLog(),
                                connect_delegate.Pass());
  state_ = CONNECTING;
}

void WebSocketChannel::OnConnectSuccess(scoped_ptr<WebSocketStream> stream) {
  DCHECK(stream);
  DCHECK_EQ(CONNECTING, state_);
  stream_ = stream.Pass();
  state_ = CONNECTED;
  event_interface_->OnAddChannelResponse(false, stream_->GetSubProtocol());

  // TODO(ricea): Get flow control information from the WebSocketStream once we
  // have a multiplexing WebSocketStream.
  current_send_quota_ = send_quota_high_water_mark_;
  event_interface_->OnFlowControl(send_quota_high_water_mark_);

  // We don't need this any more.
  stream_request_.reset();
  ReadFrames();
}

void WebSocketChannel::OnConnectFailure(uint16 websocket_error) {
  DCHECK_EQ(CONNECTING, state_);
  state_ = CLOSED;
  stream_request_.reset();
  event_interface_->OnAddChannelResponse(true, "");
}

void WebSocketChannel::WriteFrames() {
  int result = OK;
  do {
    // This use of base::Unretained is safe because we own the WebSocketStream
    // and destroying it cancels all callbacks.
    result = stream_->WriteFrames(
        data_being_sent_->frames(),
        base::Bind(
            &WebSocketChannel::OnWriteDone, base::Unretained(this), false));
    if (result != ERR_IO_PENDING) {
      OnWriteDone(true, result);
    }
  } while (result == OK && data_being_sent_);
}

void WebSocketChannel::OnWriteDone(bool synchronous, int result) {
  DCHECK_NE(FRESHLY_CONSTRUCTED, state_);
  DCHECK_NE(CONNECTING, state_);
  DCHECK_NE(ERR_IO_PENDING, result);
  DCHECK(data_being_sent_);
  switch (result) {
    case OK:
      if (data_to_send_next_) {
        data_being_sent_ = data_to_send_next_.Pass();
        if (!synchronous) {
          WriteFrames();
        }
      } else {
        data_being_sent_.reset();
        if (current_send_quota_ < send_quota_low_water_mark_) {
          // TODO(ricea): Increase low_water_mark and high_water_mark if
          // throughput is high, reduce them if throughput is low.  Low water
          // mark needs to be >= the bandwidth delay product *of the IPC
          // channel*. Because factors like context-switch time, thread wake-up
          // time, and bus speed come into play it is complex and probably needs
          // to be determined empirically.
          DCHECK_LE(send_quota_low_water_mark_, send_quota_high_water_mark_);
          // TODO(ricea): Truncate quota by the quota specified by the remote
          // server, if the protocol in use supports quota.
          int fresh_quota = send_quota_high_water_mark_ - current_send_quota_;
          current_send_quota_ += fresh_quota;
          event_interface_->OnFlowControl(fresh_quota);
        }
      }
      return;

    // If a recoverable error condition existed, it would go here.

    default:
      DCHECK_LT(result, 0)
          << "WriteFrames() should only return OK or ERR_ codes";
      stream_->Close();
      if (state_ != CLOSED) {
        state_ = CLOSED;
        event_interface_->OnDropChannel(kWebSocketErrorAbnormalClosure,
                                        "Abnormal Closure");
      }
      return;
  }
}

void WebSocketChannel::ReadFrames() {
  int result = OK;
  do {
    // This use of base::Unretained is safe because we own the WebSocketStream,
    // and any pending reads will be cancelled when it is destroyed.
    result = stream_->ReadFrames(
        &read_frame_chunks_,
        base::Bind(
            &WebSocketChannel::OnReadDone, base::Unretained(this), false));
    if (result != ERR_IO_PENDING) {
      OnReadDone(true, result);
    }
  } while (result == OK && state_ != CLOSED);
}

void WebSocketChannel::OnReadDone(bool synchronous, int result) {
  DCHECK_NE(FRESHLY_CONSTRUCTED, state_);
  DCHECK_NE(CONNECTING, state_);
  DCHECK_NE(ERR_IO_PENDING, result);
  switch (result) {
    case OK:
      // ReadFrames() must use ERR_CONNECTION_CLOSED for a closed connection
      // with no data read, not an empty response.
      DCHECK(!read_frame_chunks_.empty())
          << "ReadFrames() returned OK, but nothing was read.";
      for (size_t i = 0; i < read_frame_chunks_.size(); ++i) {
        scoped_ptr<WebSocketFrameChunk> chunk(read_frame_chunks_[i]);
        read_frame_chunks_[i] = NULL;
        ProcessFrameChunk(chunk.Pass());
      }
      read_frame_chunks_.clear();
      // We need to always keep a call to ReadFrames pending.
      if (!synchronous && state_ != CLOSED) {
        ReadFrames();
      }
      return;

    default:
      DCHECK_LT(result, 0)
          << "ReadFrames() should only return OK or ERR_ codes";
      stream_->Close();
      if (state_ != CLOSED) {
        state_ = CLOSED;
        uint16 code = kWebSocketErrorAbnormalClosure;
        std::string reason = "Abnormal Closure";
        if (closing_code_ != 0) {
          code = closing_code_;
          reason = closing_reason_;
        }
        event_interface_->OnDropChannel(code, reason);
      }
      return;
  }
}

void WebSocketChannel::ProcessFrameChunk(
    scoped_ptr<WebSocketFrameChunk> chunk) {
  bool is_first_chunk = false;
  if (chunk->header) {
    DCHECK(current_frame_header_ == NULL)
        << "Received the header for a new frame without notification that "
        << "the previous frame was complete.";
    is_first_chunk = true;
    current_frame_header_.swap(chunk->header);
    if (current_frame_header_->masked) {
      // RFC6455 Section 5.1 "A client MUST close a connection if it detects a
      // masked frame."
      FailChannel(SEND_REAL_ERROR,
                  kWebSocketErrorProtocolError,
                  "Masked frame from server");
      return;
    }
  }
  if (!current_frame_header_) {
    // If we rejected the previous chunk as invalid, then we will have reset
    // current_frame_header_ to avoid using it. More chunks of the invalid frame
    // may still arrive, so this is not necessarily a bug on our side. However,
    // if this happens when state_ is CONNECTED, it is definitely a bug.
    DCHECK(state_ != CONNECTED) << "Unexpected header-less frame received "
                                << "(final_chunk = " << chunk->final_chunk
                                << ", data size = " << chunk->data->size()
                                << ")";
    return;
  }
  scoped_refptr<IOBufferWithSize> data_buffer;
  data_buffer.swap(chunk->data);
  const bool is_final_chunk = chunk->final_chunk;
  chunk.reset();
  WebSocketFrameHeader::OpCode opcode = current_frame_header_->opcode;
  if (WebSocketFrameHeader::IsKnownControlOpCode(opcode)) {
    if (!current_frame_header_->final) {
      FailChannel(SEND_REAL_ERROR,
                  kWebSocketErrorProtocolError,
                  "Control message with FIN bit unset received");
      return;
    }
    if (current_frame_header_->payload_length > kMaxControlFramePayload) {
      FailChannel(SEND_REAL_ERROR,
                  kWebSocketErrorProtocolError,
                  "Control message has payload over 125 bytes");
      return;
    }
    if (!is_final_chunk) {
      VLOG(2) << "Encountered a split control frame, opcode " << opcode;
      if (incomplete_control_frame_body_) {
        // The really horrid case. We need to create a new IOBufferWithSize
        // combining the new one and the old one. This should virtually never
        // happen.
        // TODO(ricea): This algorithm is O(N^2). Use a fixed 127-byte buffer
        // instead.
        VLOG(3) << "Hit the really horrid case";
        incomplete_control_frame_body_ =
            ConcatenateIOBuffers(incomplete_control_frame_body_, data_buffer);
      } else {
        // The merely horrid case. Store the IOBufferWithSize to use when the
        // rest of the control frame arrives.
        incomplete_control_frame_body_.swap(data_buffer);
      }
      return;  // Handle when complete.
    }
    if (incomplete_control_frame_body_) {
      VLOG(2) << "Rejoining a split control frame, opcode " << opcode;
      data_buffer =
          ConcatenateIOBuffers(incomplete_control_frame_body_, data_buffer);
      incomplete_control_frame_body_ = NULL;  // Frame now complete.
    }
  }

  // Apply basic sanity checks to the |payload_length| field from the frame
  // header. We can only apply a strict check when we know we have the whole
  // frame in one chunk.
  DCHECK_GE(current_frame_header_->payload_length,
            base::checked_numeric_cast<uint64>(data_buffer->size()));
  DCHECK(!is_first_chunk || !is_final_chunk ||
         current_frame_header_->payload_length ==
             base::checked_numeric_cast<uint64>(data_buffer->size()));

  // Respond to the frame appropriately to its type.
  HandleFrame(opcode, is_first_chunk, is_final_chunk, data_buffer);

  if (is_final_chunk) {
    // Make sure we do not apply this frame header to any future chunks.
    current_frame_header_.reset();
  }
}

void WebSocketChannel::HandleFrame(
    const WebSocketFrameHeader::OpCode opcode,
    bool is_first_chunk,
    bool is_final_chunk,
    const scoped_refptr<IOBufferWithSize>& data_buffer) {
  DCHECK_NE(RECV_CLOSED, state_)
      << "HandleFrame() does not support being called re-entrantly from within "
         "SendClose()";
  if (state_ == CLOSED || state_ == CLOSE_WAIT) {
    DVLOG_IF(1, state_ == CLOSED) << "A frame was received while in the CLOSED "
                                     "state. This is possible after a channel "
                                     "failed, but should be very rare.";
    std::string frame_name;
    switch (opcode) {
      case WebSocketFrameHeader::kOpCodeText:    // fall-thru
      case WebSocketFrameHeader::kOpCodeBinary:  // fall-thru
      case WebSocketFrameHeader::kOpCodeContinuation:
        frame_name = "Data frame";
        break;

      case WebSocketFrameHeader::kOpCodePing:
        frame_name = "Ping";
        break;

      case WebSocketFrameHeader::kOpCodePong:
        frame_name = "Pong";
        break;

      case WebSocketFrameHeader::kOpCodeClose:
        frame_name = "Close";
        break;

      default:
        frame_name = "Unknown frame type";
        break;
    }
    // SEND_REAL_ERROR makes no difference here, as we won't send another Close
    // frame.
    FailChannel(SEND_REAL_ERROR,
                kWebSocketErrorProtocolError,
                frame_name + " received after close");
    return;
  }
  switch (opcode) {
    case WebSocketFrameHeader::kOpCodeText:    // fall-thru
    case WebSocketFrameHeader::kOpCodeBinary:  // fall-thru
    case WebSocketFrameHeader::kOpCodeContinuation:
      if (state_ == CONNECTED) {
        const bool final = is_final_chunk && current_frame_header_->final;
        // TODO(ricea): Need to fail the connection if UTF-8 is invalid
        // post-reassembly. Requires a streaming UTF-8 validator.
        // TODO(ricea): Can this copy be eliminated?
        const char* const data_begin = data_buffer->data();
        const char* const data_end = data_begin + data_buffer->size();
        const std::vector<char> data(data_begin, data_end);
        // TODO(ricea): Handle the (improbable) case when ReadFrames returns far
        // more data at once than we want to send in a single IPC (in which case
        // we need to buffer the data and return to the event loop with a
        // callback to send the rest in 32K chunks).

        // Send the received frame to the renderer process.
        event_interface_->OnDataFrame(
            final,
            is_first_chunk ? opcode : WebSocketFrameHeader::kOpCodeContinuation,
            data);
      } else {
        VLOG(3) << "Ignored data packet received in state " << state_;
      }
      return;

    case WebSocketFrameHeader::kOpCodePing:
      VLOG(1) << "Got Ping of size " << data_buffer->size();
      if (state_ == CONNECTED) {
        SendIOBufferWithSize(
            true, WebSocketFrameHeader::kOpCodePong, data_buffer);
      } else {
        VLOG(3) << "Ignored ping in state " << state_;
      }
      return;

    case WebSocketFrameHeader::kOpCodePong:
      VLOG(1) << "Got Pong of size " << data_buffer->size();
      // We do not need to do anything with pong messages.
      return;

    case WebSocketFrameHeader::kOpCodeClose: {
      uint16 code = kWebSocketNormalClosure;
      std::string reason;
      ParseClose(data_buffer, &code, &reason);
      // TODO(ricea): Find a way to safely log the message from the close
      // message (escape control codes and so on).
      VLOG(1) << "Got Close with code " << code;
      switch (state_) {
        case CONNECTED:
          state_ = RECV_CLOSED;
          SendClose(code, reason);  // Sets state_ to CLOSE_WAIT
          event_interface_->OnClosingHandshake();
          closing_code_ = code;
          closing_reason_ = reason;
          break;

        case SEND_CLOSED:
          state_ = CLOSE_WAIT;
          // From RFC6455 section 7.1.5: "Each endpoint
          // will see the status code sent by the other end as _The WebSocket
          // Connection Close Code_."
          closing_code_ = code;
          closing_reason_ = reason;
          break;

        default:
          LOG(DFATAL) << "Got Close in unexpected state " << state_;
          break;
      }
      return;
    }

    default:
      FailChannel(
          SEND_REAL_ERROR, kWebSocketErrorProtocolError, "Unknown opcode");
      return;
  }
}

void WebSocketChannel::SendIOBufferWithSize(
    bool fin,
    WebSocketFrameHeader::OpCode op_code,
    const scoped_refptr<IOBufferWithSize>& buffer) {
  DCHECK(state_ == CONNECTED || state_ == RECV_CLOSED);
  DCHECK(stream_);
  scoped_ptr<WebSocketFrameHeader> header(new WebSocketFrameHeader(op_code));
  header->final = fin;
  header->masked = true;
  header->payload_length = buffer->size();
  scoped_ptr<WebSocketFrameChunk> chunk(new WebSocketFrameChunk());
  chunk->header = header.Pass();
  chunk->final_chunk = true;
  chunk->data = buffer;
  if (data_being_sent_) {
    // Either the link to the WebSocket server is saturated, or we are simply
    // processing a batch of messages.
    // TODO(ricea): We need to keep some statistics to work out which situation
    // we are in and adjust quota appropriately.
    if (!data_to_send_next_)
      data_to_send_next_.reset(new SendBuffer);
    data_to_send_next_->AddFrame(chunk.Pass());
  } else {
    data_being_sent_.reset(new SendBuffer);
    data_being_sent_->AddFrame(chunk.Pass());
    WriteFrames();
  }
}

void WebSocketChannel::FailChannel(ExposeError expose,
                                   uint16 code,
                                   const std::string& reason) {
  DCHECK_NE(FRESHLY_CONSTRUCTED, state_);
  DCHECK_NE(CONNECTING, state_);
  // TODO(ricea): Logging.
  State old_state = state_;
  if (state_ == CONNECTED) {
    uint16 send_code = kWebSocketErrorGoingAway;
    std::string send_reason = "Internal Error";
    if (expose == SEND_REAL_ERROR) {
      send_code = code;
      send_reason = reason;
    }
    SendClose(send_code, send_reason);  // Sets state_ to SEND_CLOSED
  }
  // Careful study of RFC6455 section 7.1.7 and 7.1.1 indicates we should close
  // the connection ourselves without waiting for the closing handshake.
  stream_->Close();
  state_ = CLOSED;

  // We may be in the middle of processing several chunks. We should not re-use
  // the frame header.
  current_frame_header_.reset();
  if (old_state != CLOSED) {
    event_interface_->OnDropChannel(code, reason);
  }
}

void WebSocketChannel::SendClose(uint16 code, const std::string& reason) {
  DCHECK(state_ == CONNECTED || state_ == RECV_CLOSED);
  // TODO(ricea): Ensure reason.length() <= 123
  scoped_refptr<IOBufferWithSize> body;
  if (code == kWebSocketErrorNoStatusReceived) {
    // Special case: translate kWebSocketErrorNoStatusReceived into a Close
    // frame with no payload.
    body = new IOBufferWithSize(0);
  } else {
    const size_t payload_length = kWebSocketCloseCodeLength + reason.length();
    body = new IOBufferWithSize(payload_length);
    WriteBigEndian(body->data(), code);
    COMPILE_ASSERT(sizeof(code) == kWebSocketCloseCodeLength,
                   they_should_both_be_two);
    std::copy(
        reason.begin(), reason.end(), body->data() + kWebSocketCloseCodeLength);
  }
  SendIOBufferWithSize(true, WebSocketFrameHeader::kOpCodeClose, body);
  state_ = (state_ == CONNECTED) ? SEND_CLOSED : CLOSE_WAIT;
}

void WebSocketChannel::ParseClose(const scoped_refptr<IOBufferWithSize>& buffer,
                                  uint16* code,
                                  std::string* reason) {
  const char* data = buffer->data();
  size_t size = base::checked_numeric_cast<size_t>(buffer->size());
  reason->clear();
  if (size < kWebSocketCloseCodeLength) {
    *code = kWebSocketErrorNoStatusReceived;
    if (size != 0) {
      VLOG(1) << "Close frame with payload size " << size << " received "
              << "(the first byte is " << std::hex << static_cast<int>(data[0])
              << ")";
      return;
    }
    return;
  }
  uint16 unchecked_code = 0;
  ReadBigEndian(data, &unchecked_code);
  COMPILE_ASSERT(sizeof(unchecked_code) == kWebSocketCloseCodeLength,
                 they_should_both_be_two_bytes);
  if (unchecked_code >= static_cast<uint16>(kWebSocketNormalClosure) &&
      unchecked_code <=
          static_cast<uint16>(kWebSocketErrorPrivateReservedMax)) {
    *code = unchecked_code;
  } else {
    VLOG(1) << "Close frame contained code outside of the valid range: "
            << unchecked_code;
    *code = kWebSocketErrorAbnormalClosure;
  }
  std::string text(data + kWebSocketCloseCodeLength, data + size);
  // TODO(ricea): Is this check strict enough? In particular, check the
  // "Security Considerations" from RFC3629.
  if (IsStringUTF8(text)) {
    reason->swap(text);
  }
}

}  // namespace net
