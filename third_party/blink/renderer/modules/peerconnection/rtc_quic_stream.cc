// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_stream.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

const uint32_t RTCQuicStream::kWriteBufferSize = 4 * 1024;

RTCQuicStream::RTCQuicStream(ExecutionContext* context,
                             RTCQuicTransport* transport,
                             QuicStreamProxy* stream_proxy)
    : ContextClient(context), transport_(transport), proxy_(stream_proxy) {
  DCHECK(transport_);
  DCHECK(proxy_);
}

RTCQuicStream::~RTCQuicStream() = default;

RTCQuicTransport* RTCQuicStream::transport() const {
  return transport_;
}

String RTCQuicStream::state() const {
  switch (state_) {
    case RTCQuicStreamState::kNew:
      return "new";
    case RTCQuicStreamState::kOpening:
      return "opening";
    case RTCQuicStreamState::kOpen:
      return "open";
    case RTCQuicStreamState::kClosing:
      return "closing";
    case RTCQuicStreamState::kClosed:
      return "closed";
  }
  return String();
}

uint32_t RTCQuicStream::readBufferedAmount() const {
  return read_buffered_amount_;
}

uint32_t RTCQuicStream::writeBufferedAmount() const {
  return write_buffered_amount_;
}

uint32_t RTCQuicStream::maxWriteBufferedAmount() const {
  return kWriteBufferSize;
}

void RTCQuicStream::write(NotShared<DOMUint8Array> data,
                          ExceptionState& exception_state) {
  if (IsClosed() || wrote_fin_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The stream is not writable.");
    return;
  }
  if (data.View()->length() == 0) {
    return;
  }
  uint32_t remaining_write_buffer_size =
      kWriteBufferSize - writeBufferedAmount();
  if (data.View()->length() > remaining_write_buffer_size) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "The write data size of " + String::Number(data.View()->length()) +
            " bytes would exceed the remaining write buffer size of " +
            String::Number(remaining_write_buffer_size) + " bytes.");
    return;
  }
  Vector<uint8_t> data_vector(data.View()->length());
  memcpy(data_vector.data(), data.View()->Data(), data.View()->length());
  proxy_->WriteData(std::move(data_vector), /*fin=*/false);
  write_buffered_amount_ += data.View()->length();
}

void RTCQuicStream::finish() {
  if (IsClosed()) {
    return;
  }
  if (wrote_fin_) {
    return;
  }
  proxy_->WriteData({}, /*fin=*/true);
  wrote_fin_ = true;
  if (readable_) {
    DCHECK_EQ(state_, RTCQuicStreamState::kOpen);
    state_ = RTCQuicStreamState::kClosing;
  } else {
    DCHECK_EQ(state_, RTCQuicStreamState::kClosing);
    Close(CloseReason::kReadWriteFinished);
  }
}

void RTCQuicStream::reset() {
  if (IsClosed()) {
    return;
  }
  Close(CloseReason::kLocalReset);
}

void RTCQuicStream::OnRemoteReset() {
  Close(CloseReason::kRemoteReset);
}

void RTCQuicStream::OnDataReceived(Vector<uint8_t> data, bool fin) {
  if (!fin) {
    return;
  }
  DCHECK_NE(state_, RTCQuicStreamState::kClosed);
  DCHECK(readable_);
  readable_ = false;
  if (!wrote_fin_) {
    DCHECK_EQ(state_, RTCQuicStreamState::kOpen);
    state_ = RTCQuicStreamState::kClosing;
  } else {
    DCHECK_EQ(state_, RTCQuicStreamState::kClosing);
    Close(CloseReason::kReadWriteFinished);
  }
  DispatchEvent(*Event::Create(event_type_names::kStatechange));
}

void RTCQuicStream::OnWriteDataConsumed(uint32_t amount) {
  DCHECK_GE(write_buffered_amount_, amount);
  write_buffered_amount_ -= amount;
}

void RTCQuicStream::OnQuicTransportClosed(
    RTCQuicTransport::CloseReason reason) {
  switch (reason) {
    case RTCQuicTransport::CloseReason::kContextDestroyed:
      Close(CloseReason::kContextDestroyed);
      break;
    default:
      Close(CloseReason::kQuicTransportClosed);
      break;
  }
}

void RTCQuicStream::Close(CloseReason reason) {
  DCHECK_NE(state_, RTCQuicStreamState::kClosed);

  // Tear down the QuicStreamProxy.
  // If the Close is caused by a remote event or regular use of WriteData, the
  // QuicStreamProxy will have already been deleted.
  // If the Close is caused by the transport then the transport is responsible
  // for deleting the QuicStreamProxy.
  if (reason == CloseReason::kLocalReset) {
    // This deletes the QuicStreamProxy.
    proxy_->Reset();
  }
  proxy_ = nullptr;

  // Remove this stream from the RTCQuicTransport unless closing from a
  // transport-level event.
  switch (reason) {
    case CloseReason::kReadWriteFinished:
    case CloseReason::kLocalReset:
    case CloseReason::kRemoteReset:
      transport_->RemoveStream(this);
      break;
    case CloseReason::kQuicTransportClosed:
    case CloseReason::kContextDestroyed:
      // The RTCQuicTransport will handle clearing its list of streams.
      break;
  }

  // Clear observable state.
  readable_ = false;
  write_buffered_amount_ = 0;

  // Change the state. Fire the statechange event only if the close is caused by
  // a remote stream event.
  state_ = RTCQuicStreamState::kClosed;
  if (reason == CloseReason::kRemoteReset) {
    DispatchEvent(*Event::Create(event_type_names::kStatechange));
  }
}

const AtomicString& RTCQuicStream::InterfaceName() const {
  return event_target_names::kRTCQuicStream;
}

ExecutionContext* RTCQuicStream::GetExecutionContext() const {
  return ContextClient::GetExecutionContext();
}

void RTCQuicStream::Trace(blink::Visitor* visitor) {
  visitor->Trace(transport_);
  EventTargetWithInlineData::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
