// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_stream.h"

#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_transport.h"

namespace blink {

RTCQuicStream::RTCQuicStream(RTCQuicTransport* transport)
    : transport_(transport) {
  DCHECK(transport_);
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

void RTCQuicStream::Stop() {
  state_ = RTCQuicStreamState::kClosed;
}

void RTCQuicStream::Trace(blink::Visitor* visitor) {
  visitor->Trace(transport_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
