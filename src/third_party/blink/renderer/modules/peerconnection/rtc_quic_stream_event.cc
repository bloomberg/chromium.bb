// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_stream_event.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_quic_stream_event_init.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_stream.h"

namespace blink {

RTCQuicStreamEvent* RTCQuicStreamEvent::Create(RTCQuicStream* stream) {
  return MakeGarbageCollected<RTCQuicStreamEvent>(stream);
}

RTCQuicStreamEvent* RTCQuicStreamEvent::Create(
    const AtomicString& type,
    const RTCQuicStreamEventInit* initializer) {
  return MakeGarbageCollected<RTCQuicStreamEvent>(type, initializer);
}

RTCQuicStreamEvent::RTCQuicStreamEvent(RTCQuicStream* stream)
    : Event(event_type_names::kQuicstream, Bubbles::kNo, Cancelable::kNo),
      stream_(stream) {}

RTCQuicStreamEvent::RTCQuicStreamEvent(
    const AtomicString& type,
    const RTCQuicStreamEventInit* initializer)
    : Event(type, initializer), stream_(initializer->stream()) {}

RTCQuicStreamEvent::~RTCQuicStreamEvent() = default;

RTCQuicStream* RTCQuicStreamEvent::stream() const {
  return stream_.Get();
}

const AtomicString& RTCQuicStreamEvent::InterfaceName() const {
  return event_interface_names::kRTCQuicStreamEvent;
}

void RTCQuicStreamEvent::Trace(Visitor* visitor) {
  visitor->Trace(stream_);
  Event::Trace(visitor);
}

}  // namespace blink
