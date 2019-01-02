// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_TRANSPORT_H_

#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_parameters.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class DOMArrayBuffer;
class ExceptionState;
class RTCCertificate;
class RTCIceTransport;
class RTCQuicParameters;
class RTCQuicStream;

enum class RTCQuicTransportState {
  kNew,
  kConnecting,
  kConnected,
  kClosed,
  kFailed
};

class MODULES_EXPORT RTCQuicTransport final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static RTCQuicTransport* Create(
      RTCIceTransport* transport,
      const HeapVector<Member<RTCCertificate>>& certificates,
      ExceptionState& exception_state);

  ~RTCQuicTransport() override;

  // https://w3c.github.io/webrtc-quic/#quic-transport*
  RTCIceTransport* transport() const;
  String state() const;
  void getLocalParameters(RTCQuicParameters& result) const;
  void getRemoteParameters(base::Optional<RTCQuicParameters>& result) const;
  const HeapVector<Member<RTCCertificate>>& getCertificates() const;
  const HeapVector<Member<DOMArrayBuffer>>& getRemoteCertificates() const;
  void stop();
  RTCQuicStream* createStream(ExceptionState& exception_state);

  // For garbage collection.
  void Trace(blink::Visitor* visitor) override;

 private:
  RTCQuicTransport(RTCIceTransport* transport,
                   const HeapVector<Member<RTCCertificate>>& certificates,
                   ExceptionState& exception_state);

  bool IsClosed() const { return state_ == RTCQuicTransportState::kClosed; }
  bool RaiseExceptionIfClosed(ExceptionState& exception_state) const;

  Member<RTCIceTransport> transport_;
  RTCQuicTransportState state_ = RTCQuicTransportState::kNew;
  HeapVector<Member<RTCCertificate>> certificates_;
  HeapVector<Member<DOMArrayBuffer>> remote_certificates_;
  HeapHashSet<Member<RTCQuicStream>> streams_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_TRANSPORT_H_
