// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_STREAM_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class RTCQuicTransport;

enum class RTCQuicStreamState { kNew, kOpening, kOpen, kClosing, kClosed };

class MODULES_EXPORT RTCQuicStream final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  RTCQuicStream(RTCQuicTransport* transport);
  ~RTCQuicStream() override;

  void Stop();

  // rtc_quic_stream.idl
  RTCQuicTransport* transport() const;
  String state() const;
  uint32_t readBufferedAmount() const;
  uint32_t writeBufferedAmount() const;

  // For garbage collection.
  void Trace(blink::Visitor* visitor) override;

 private:
  Member<RTCQuicTransport> transport_;
  RTCQuicStreamState state_ = RTCQuicStreamState::kNew;
  uint32_t read_buffered_amount_ = 0;
  uint32_t write_buffered_amount_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_STREAM_H_
