// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_channel.h"

#include <algorithm>

#include "base/basictypes.h"  // for size_t
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/safe_numerics.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/base/big_endian.h"
#include "net/base/io_buffer.h"
#include "net/base/net_log.h"
#include "net/http/http_util.h"
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
// This timeout is based on TCPMaximumSegmentLifetime * 2 from
// MainThreadWebSocketChannel.cpp in Blink.
const int kClosingHandshakeTimeoutSeconds = 2 * 2 * 60;

typedef WebSocketEventInterface::ChannelState ChannelState;
const ChannelState CHANNEL_ALIVE = WebSocketEventInterface::CHANNEL_ALIVE;
const ChannelState CHANNEL_DELETED = WebSocketEventInterface::CHANNEL_DELETED;

// Maximum close reason length = max control frame payload -
//                               status code length
//                             = 125 - 2
const size_t kMaximumCloseReasonLength = 125 - kWebSocketCloseCodeLength;

// Check a close status code for strict compliance with RFC6455. This is only
// used for close codes received from a renderer that we are intending to send
// out over the network. See ParseClose() for the restrictions on incoming close
// codes. The |code| parameter is type int for convenience of implementation;
// the real type is uint16.
bool IsStrictlyValidCloseStatusCode(int code) {
  static const int kInvalidRanges[] = {
      // [BAD, OK)
      0,    1000,   // 1000 is the first valid code
      1005, 1007,   // 1005 and 1006 MUST NOT be set.
      1014, 3000,   // 1014 unassigned; 1015 up to 2999 are reserved.
      5000, 65536,  // Codes above 5000 are invalid.
  };
  const int* const kInvalidRangesEnd =
      kInvalidRanges + arraysize(kInvalidRanges);

  DCHECK_GE(code, 0);
  DCHECK_LT(code, 65536);
  const int* upper = std::upper_bound(kInvalidRanges, kInvalidRangesEnd, code);
  DCHECK_NE(kInvalidRangesEnd, upper);
  DCHECK_GT(upper, kInvalidRanges);
  DCHECK_GT(*upper, code);
  DCHECK_LE(*(upper - 1), code);
  return ((upper - kInvalidRanges) % 2) == 0;
}

// This function avoids a bunch of boilerplate code.
void AllowUnused(ChannelState ALLOW_UNUSED unused) {}

}  // namespace

// A class to encapsulate a set of frames and information about the size of
// those frames.
class WebSocketChannel::SendBuffer {
 public:
  SendBuffer() : total_bytes_(0) {}

  // Add a WebSocketFrame to the buffer and increase total_bytes_.
  void AddFrame(scoped_ptr<WebSocketFrame> chunk);

  // Return a pointer to the frames_ for write purposes.
  ScopedVector<WebSocketFrame>* frames() { return &frames_; }

 private:
  // The frames_ that will be sent in the next call to WriteFrames().
  ScopedVector<WebSocketFrame> frames_;

  // The total size of the payload data in |frames_|. This will be used to
  // measure the throughput of the link.
  // TODO(ricea): Measure the throughput of the link.
  size_t total_bytes_;
};

void WebSocketChannel::SendBuffer::AddFrame(scoped_ptr<WebSocketFrame> frame) {
  total_bytes_ += frame->header.payload_length;
  frames_.push_back(frame.release());
}

// Implementation of WebSocketStream::ConnectDelegate that simply forwards the
// calls on to the WebSocketChannel that created it.
class WebSocketChannel::ConnectDelegate
    : public WebSocketStream::ConnectDelegate {
 public:
  explicit ConnectDelegate(WebSocketChannel* creator) : creator_(creator) {}

  virtual void OnSuccess(scoped_ptr<WebSocketStream> stream) OVERRIDE {
    creator_->OnConnectSuccess(stream.Pass());
    // |this| may have been deleted.
  }

  virtual void OnFailure(uint16 websocket_error) OVERRIDE {
    creator_->OnConnectFailure(websocket_error);
    // |this| has been deleted.
  }

 private:
  // A pointer to the WebSocketChannel that created this object. There is no
  // danger of this pointer being stale, because deleting the WebSocketChannel
  // cancels the connect process, deleting this object and preventing its
  // callbacks from being called.
  WebSocketChannel* const creator_;

  DISALLOW_COPY_AND_ASSIGN(ConnectDelegate);
};

WebSocketChannel::WebSocketChannel(
    scoped_ptr<WebSocketEventInterface> event_interface,
    URLRequestContext* url_request_context)
    : event_interface_(event_interface.Pass()),
      url_request_context_(url_request_context),
      send_quota_low_water_mark_(kDefaultSendQuotaLowWaterMark),
      send_quota_high_water_mark_(kDefaultSendQuotaHighWaterMark),
      current_send_quota_(0),
      timeout_(base::TimeDelta::FromSeconds(kClosingHandshakeTimeoutSeconds)),
      closing_code_(0),
      state_(FRESHLY_CONSTRUCTED) {}

WebSocketChannel::~WebSocketChannel() {
  // The stream may hold a pointer to read_frames_, and so it needs to be
  // destroyed first.
  stream_.reset();
  // The timer may have a callback pointing back to us, so stop it just in case
  // someone decides to run the event loop from their destructor.
  timer_.Stop();
}

void WebSocketChannel::SendAddChannelRequest(
    const GURL& socket_url,
    const std::vector<std::string>& requested_subprotocols,
    const GURL& origin) {
  // Delegate to the tested version.
  SendAddChannelRequestWithSuppliedCreator(
      socket_url,
      requested_subprotocols,
      origin,
      base::Bind(&WebSocketStream::CreateAndConnectStream));
}

bool WebSocketChannel::InClosingState() const {
  // The state RECV_CLOSED is not supported here, because it is only used in one
  // code path and should not leak into the code in general.
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
    AllowUnused(FailChannel(SEND_GOING_AWAY,
                            kWebSocketMuxErrorSendQuotaViolation,
                            "Send quota exceeded"));
    // |this| has been deleted.
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
  // send_quota_low_water_mark_, it might be good to increase the "low
  // water mark" and "high water mark", but only if the link to the WebSocket
  // server is not saturated.
  // TODO(ricea): For kOpCodeText, do UTF-8 validation?
  scoped_refptr<IOBuffer> buffer(new IOBuffer(data.size()));
  std::copy(data.begin(), data.end(), buffer->data());
  AllowUnused(SendIOBuffer(fin, op_code, buffer, data.size()));
  // |this| may have been deleted.
}

void WebSocketChannel::SendFlowControl(int64 quota) {
  DCHECK(state_ == CONNECTING || state_ == CONNECTED || state_ == SEND_CLOSED ||
         state_ == CLOSE_WAIT);
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
  // Javascript actually only permits 1000 and 3000-4999, but the implementation
  // itself may produce different codes. The length of |reason| is also checked
  // by Javascript.
  if (!IsStrictlyValidCloseStatusCode(code) ||
      reason.size() > kMaximumCloseReasonLength) {
    // "InternalServerError" is actually used for errors from any endpoint, per
    // errata 3227 to RFC6455. If the renderer is sending us an invalid code or
    // reason it must be malfunctioning in some way, and based on that we
    // interpret this as an internal error.
    AllowUnused(
        SendClose(kWebSocketErrorInternalServerError, "Internal Error"));
    // |this| may have been deleted.
    return;
  }
  AllowUnused(SendClose(code, IsStringUTF8(reason) ? reason : std::string()));
  // |this| may have been deleted.
}

void WebSocketChannel::SendAddChannelRequestForTesting(
    const GURL& socket_url,
    const std::vector<std::string>& requested_subprotocols,
    const GURL& origin,
    const WebSocketStreamCreator& creator) {
  SendAddChannelRequestWithSuppliedCreator(
      socket_url, requested_subprotocols, origin, creator);
}

void WebSocketChannel::SetClosingHandshakeTimeoutForTesting(
    base::TimeDelta delay) {
  timeout_ = delay;
}

void WebSocketChannel::SendAddChannelRequestWithSuppliedCreator(
    const GURL& socket_url,
    const std::vector<std::string>& requested_subprotocols,
    const GURL& origin,
    const WebSocketStreamCreator& creator) {
  DCHECK_EQ(FRESHLY_CONSTRUCTED, state_);
  if (!socket_url.SchemeIsWSOrWSS()) {
    // TODO(ricea): Kill the renderer (this error should have been caught by
    // Javascript).
    AllowUnused(event_interface_->OnAddChannelResponse(true, ""));
    // |this| is deleted here.
    return;
  }
  socket_url_ = socket_url;
  scoped_ptr<WebSocketStream::ConnectDelegate> connect_delegate(
      new ConnectDelegate(this));
  stream_request_ = creator.Run(socket_url_,
                                requested_subprotocols,
                                origin,
                                url_request_context_,
                                BoundNetLog(),
                                connect_delegate.Pass());
  state_ = CONNECTING;
}

void WebSocketChannel::OnConnectSuccess(scoped_ptr<WebSocketStream> stream) {
  DCHECK(stream);
  DCHECK_EQ(CONNECTING, state_);
  stream_ = stream.Pass();
  state_ = CONNECTED;
  if (event_interface_->OnAddChannelResponse(
          false, stream_->GetSubProtocol()) == CHANNEL_DELETED)
    return;

  // TODO(ricea): Get flow control information from the WebSocketStream once we
  // have a multiplexing WebSocketStream.
  current_send_quota_ = send_quota_high_water_mark_;
  if (event_interface_->OnFlowControl(send_quota_high_water_mark_) ==
      CHANNEL_DELETED)
    return;

  // |stream_request_| is not used once the connection has succeeded.
  stream_request_.reset();
  AllowUnused(ReadFrames());
  // |this| may have been deleted.
}

void WebSocketChannel::OnConnectFailure(uint16 websocket_error) {
  DCHECK_EQ(CONNECTING, state_);
  state_ = CLOSED;
  stream_request_.reset();
  AllowUnused(event_interface_->OnAddChannelResponse(true, ""));
  // |this| has been deleted.
}

ChannelState WebSocketChannel::WriteFrames() {
  int result = OK;
  do {
    // This use of base::Unretained is safe because this object owns the
    // WebSocketStream and destroying it cancels all callbacks.
    result = stream_->WriteFrames(
        data_being_sent_->frames(),
        base::Bind(base::IgnoreResult(&WebSocketChannel::OnWriteDone),
                   base::Unretained(this),
                   false));
    if (result != ERR_IO_PENDING) {
      if (OnWriteDone(true, result) == CHANNEL_DELETED)
        return CHANNEL_DELETED;
    }
  } while (result == OK && data_being_sent_);
  return CHANNEL_ALIVE;
}

ChannelState WebSocketChannel::OnWriteDone(bool synchronous, int result) {
  DCHECK_NE(FRESHLY_CONSTRUCTED, state_);
  DCHECK_NE(CONNECTING, state_);
  DCHECK_NE(ERR_IO_PENDING, result);
  DCHECK(data_being_sent_);
  switch (result) {
    case OK:
      if (data_to_send_next_) {
        data_being_sent_ = data_to_send_next_.Pass();
        if (!synchronous)
          return WriteFrames();
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
          return event_interface_->OnFlowControl(fresh_quota);
        }
      }
      return CHANNEL_ALIVE;

    // If a recoverable error condition existed, it would go here.

    default:
      DCHECK_LT(result, 0)
          << "WriteFrames() should only return OK or ERR_ codes";
      stream_->Close();
      DCHECK_NE(CLOSED, state_);
      state_ = CLOSED;
      return event_interface_->OnDropChannel(kWebSocketErrorAbnormalClosure,
                                             "Abnormal Closure");
  }
}

ChannelState WebSocketChannel::ReadFrames() {
  int result = OK;
  do {
    // This use of base::Unretained is safe because this object owns the
    // WebSocketStream, and any pending reads will be cancelled when it is
    // destroyed.
    result = stream_->ReadFrames(
        &read_frames_,
        base::Bind(base::IgnoreResult(&WebSocketChannel::OnReadDone),
                   base::Unretained(this),
                   false));
    if (result != ERR_IO_PENDING) {
      if (OnReadDone(true, result) == CHANNEL_DELETED)
        return CHANNEL_DELETED;
    }
    DCHECK_NE(CLOSED, state_);
  } while (result == OK);
  return CHANNEL_ALIVE;
}

ChannelState WebSocketChannel::OnReadDone(bool synchronous, int result) {
  DCHECK_NE(FRESHLY_CONSTRUCTED, state_);
  DCHECK_NE(CONNECTING, state_);
  DCHECK_NE(ERR_IO_PENDING, result);
  switch (result) {
    case OK:
      // ReadFrames() must use ERR_CONNECTION_CLOSED for a closed connection
      // with no data read, not an empty response.
      DCHECK(!read_frames_.empty())
          << "ReadFrames() returned OK, but nothing was read.";
      for (size_t i = 0; i < read_frames_.size(); ++i) {
        scoped_ptr<WebSocketFrame> frame(read_frames_[i]);
        read_frames_[i] = NULL;
        if (ProcessFrame(frame.Pass()) == CHANNEL_DELETED)
          return CHANNEL_DELETED;
      }
      read_frames_.clear();
      // There should always be a call to ReadFrames pending.
      // TODO(ricea): Unless we are out of quota.
      DCHECK_NE(CLOSED, state_);
      if (!synchronous)
        return ReadFrames();
      return CHANNEL_ALIVE;

    case ERR_WS_PROTOCOL_ERROR:
      return FailChannel(SEND_REAL_ERROR,
                         kWebSocketErrorProtocolError,
                         "WebSocket Protocol Error");

    default:
      DCHECK_LT(result, 0)
          << "ReadFrames() should only return OK or ERR_ codes";
      stream_->Close();
      DCHECK_NE(CLOSED, state_);
      state_ = CLOSED;
      uint16 code = kWebSocketErrorAbnormalClosure;
      std::string reason = "Abnormal Closure";
      if (closing_code_ != 0) {
        code = closing_code_;
        reason = closing_reason_;
      }
      return event_interface_->OnDropChannel(code, reason);
  }
}

ChannelState WebSocketChannel::ProcessFrame(scoped_ptr<WebSocketFrame> frame) {
  if (frame->header.masked) {
    // RFC6455 Section 5.1 "A client MUST close a connection if it detects a
    // masked frame."
    return FailChannel(SEND_REAL_ERROR,
                       kWebSocketErrorProtocolError,
                       "Masked frame from server");
  }
  const WebSocketFrameHeader::OpCode opcode = frame->header.opcode;
  if (WebSocketFrameHeader::IsKnownControlOpCode(opcode) &&
      !frame->header.final) {
    return FailChannel(SEND_REAL_ERROR,
                       kWebSocketErrorProtocolError,
                       "Control message with FIN bit unset received");
  }

  // Respond to the frame appropriately to its type.
  return HandleFrame(
      opcode, frame->header.final, frame->data, frame->header.payload_length);
}

ChannelState WebSocketChannel::HandleFrame(
    const WebSocketFrameHeader::OpCode opcode,
    bool final,
    const scoped_refptr<IOBuffer>& data_buffer,
    size_t size) {
  DCHECK_NE(RECV_CLOSED, state_)
      << "HandleFrame() does not support being called re-entrantly from within "
         "SendClose()";
  DCHECK_NE(CLOSED, state_);
  if (state_ == CLOSE_WAIT) {
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
    // SEND_REAL_ERROR makes no difference here, as FailChannel() won't send
    // another Close frame.
    return FailChannel(SEND_REAL_ERROR,
                       kWebSocketErrorProtocolError,
                       frame_name + " received after close");
  }
  switch (opcode) {
    case WebSocketFrameHeader::kOpCodeText:    // fall-thru
    case WebSocketFrameHeader::kOpCodeBinary:  // fall-thru
    case WebSocketFrameHeader::kOpCodeContinuation:
      if (state_ == CONNECTED) {
        // TODO(ricea): Need to fail the connection if UTF-8 is invalid
        // post-reassembly. Requires a streaming UTF-8 validator.
        // TODO(ricea): Can this copy be eliminated?
        const char* const data_begin = size ? data_buffer->data() : NULL;
        const char* const data_end = data_begin + size;
        const std::vector<char> data(data_begin, data_end);
        // TODO(ricea): Handle the case when ReadFrames returns far
        // more data at once than should be sent in a single IPC. This needs to
        // be handled carefully, as an overloaded IO thread is one possible
        // cause of receiving very large chunks.

        // Sends the received frame to the renderer process.
        return event_interface_->OnDataFrame(final, opcode, data);
      }
      VLOG(3) << "Ignored data packet received in state " << state_;
      return CHANNEL_ALIVE;

    case WebSocketFrameHeader::kOpCodePing:
      VLOG(1) << "Got Ping of size " << size;
      if (state_ == CONNECTED)
        return SendIOBuffer(
            true, WebSocketFrameHeader::kOpCodePong, data_buffer, size);
      VLOG(3) << "Ignored ping in state " << state_;
      return CHANNEL_ALIVE;

    case WebSocketFrameHeader::kOpCodePong:
      VLOG(1) << "Got Pong of size " << size;
      // There is no need to do anything with pong messages.
      return CHANNEL_ALIVE;

    case WebSocketFrameHeader::kOpCodeClose: {
      uint16 code = kWebSocketNormalClosure;
      std::string reason;
      ParseClose(data_buffer, size, &code, &reason);
      // TODO(ricea): Find a way to safely log the message from the close
      // message (escape control codes and so on).
      VLOG(1) << "Got Close with code " << code;
      switch (state_) {
        case CONNECTED:
          state_ = RECV_CLOSED;
          if (SendClose(code, reason) ==  // Sets state_ to CLOSE_WAIT
              CHANNEL_DELETED)
            return CHANNEL_DELETED;
          if (event_interface_->OnClosingHandshake() == CHANNEL_DELETED)
            return CHANNEL_DELETED;
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
      return CHANNEL_ALIVE;
    }

    default:
      return FailChannel(
          SEND_REAL_ERROR, kWebSocketErrorProtocolError, "Unknown opcode");
  }
}

ChannelState WebSocketChannel::SendIOBuffer(
    bool fin,
    WebSocketFrameHeader::OpCode op_code,
    const scoped_refptr<IOBuffer>& buffer,
    size_t size) {
  DCHECK(state_ == CONNECTED || state_ == RECV_CLOSED);
  DCHECK(stream_);
  scoped_ptr<WebSocketFrame> frame(new WebSocketFrame(op_code));
  WebSocketFrameHeader& header = frame->header;
  header.final = fin;
  header.masked = true;
  header.payload_length = size;
  frame->data = buffer;
  if (data_being_sent_) {
    // Either the link to the WebSocket server is saturated, or several messages
    // are being sent in a batch.
    // TODO(ricea): Keep some statistics to work out the situation and adjust
    // quota appropriately.
    if (!data_to_send_next_)
      data_to_send_next_.reset(new SendBuffer);
    data_to_send_next_->AddFrame(frame.Pass());
    return CHANNEL_ALIVE;
  }
  data_being_sent_.reset(new SendBuffer);
  data_being_sent_->AddFrame(frame.Pass());
  return WriteFrames();
}

ChannelState WebSocketChannel::FailChannel(ExposeError expose,
                                           uint16 code,
                                           const std::string& reason) {
  DCHECK_NE(FRESHLY_CONSTRUCTED, state_);
  DCHECK_NE(CONNECTING, state_);
  DCHECK_NE(CLOSED, state_);
  // TODO(ricea): Logging.
  if (state_ == CONNECTED) {
    uint16 send_code = kWebSocketErrorGoingAway;
    std::string send_reason = "Internal Error";
    if (expose == SEND_REAL_ERROR) {
      send_code = code;
      send_reason = reason;
    }
    if (SendClose(send_code, send_reason) ==  // Sets state_ to SEND_CLOSED
        CHANNEL_DELETED)
      return CHANNEL_DELETED;
  }
  // Careful study of RFC6455 section 7.1.7 and 7.1.1 indicates the browser
  // should close the connection itself without waiting for the closing
  // handshake.
  stream_->Close();
  state_ = CLOSED;

  return event_interface_->OnDropChannel(code, reason);
}

ChannelState WebSocketChannel::SendClose(uint16 code,
                                         const std::string& reason) {
  DCHECK(state_ == CONNECTED || state_ == RECV_CLOSED);
  DCHECK_LE(reason.size(), kMaximumCloseReasonLength);
  scoped_refptr<IOBuffer> body;
  size_t size = 0;
  if (code == kWebSocketErrorNoStatusReceived) {
    // Special case: translate kWebSocketErrorNoStatusReceived into a Close
    // frame with no payload.
    body = new IOBuffer(0);
  } else {
    const size_t payload_length = kWebSocketCloseCodeLength + reason.length();
    body = new IOBuffer(payload_length);
    size = payload_length;
    WriteBigEndian(body->data(), code);
    COMPILE_ASSERT(sizeof(code) == kWebSocketCloseCodeLength,
                   they_should_both_be_two);
    std::copy(
        reason.begin(), reason.end(), body->data() + kWebSocketCloseCodeLength);
  }
  // This use of base::Unretained() is safe because we stop the timer in the
  // destructor.
  timer_.Start(
      FROM_HERE,
      timeout_,
      base::Bind(&WebSocketChannel::CloseTimeout, base::Unretained(this)));
  if (SendIOBuffer(true, WebSocketFrameHeader::kOpCodeClose, body, size) ==
      CHANNEL_DELETED)
    return CHANNEL_DELETED;
  // SendIOBuffer() checks |state_|, so it is best not to change it until after
  // SendIOBuffer() returns.
  state_ = (state_ == CONNECTED) ? SEND_CLOSED : CLOSE_WAIT;
  return CHANNEL_ALIVE;
}

void WebSocketChannel::ParseClose(const scoped_refptr<IOBuffer>& buffer,
                                  size_t size,
                                  uint16* code,
                                  std::string* reason) {
  reason->clear();
  if (size < kWebSocketCloseCodeLength) {
    *code = kWebSocketErrorNoStatusReceived;
    if (size != 0) {
      VLOG(1) << "Close frame with payload size " << size << " received "
              << "(the first byte is " << std::hex
              << static_cast<int>(buffer->data()[0]) << ")";
    }
    return;
  }
  const char* data = buffer->data();
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
  // IsStringUTF8() blocks surrogate pairs and non-characters, so it is strictly
  // stronger than required by RFC3629.
  if (IsStringUTF8(text)) {
    reason->swap(text);
  }
}

void WebSocketChannel::CloseTimeout() {
  stream_->Close();
  DCHECK_NE(CLOSED, state_);
  state_ = CLOSED;
  AllowUnused(event_interface_->OnDropChannel(kWebSocketErrorAbnormalClosure,
                                              "Abnormal Closure"));
  // |this| has been deleted.
}

}  // namespace net
