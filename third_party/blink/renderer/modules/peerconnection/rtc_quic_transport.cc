// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_transport.h"

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_certificate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_parameters.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_stream.h"

namespace blink {

RTCQuicTransport* RTCQuicTransport::Create(
    RTCIceTransport* transport,
    const HeapVector<Member<RTCCertificate>>& certificates,
    ExceptionState& exception_state) {
  if (transport->IsClosed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot construct an RTCQuicTransport with a closed RTCIceTransport.");
    return nullptr;
  }
  for (const auto& certificate : certificates) {
    if (certificate->expires() < ConvertSecondsToDOMTimeStamp(CurrentTime())) {
      exception_state.ThrowTypeError(
          "Cannot construct an RTCQuicTransport with an expired certificate.");
      return nullptr;
    }
  }
  return new RTCQuicTransport(transport, certificates, exception_state);
}

RTCQuicTransport::RTCQuicTransport(
    RTCIceTransport* transport,
    const HeapVector<Member<RTCCertificate>>& certificates,
    ExceptionState& exception_state)
    : transport_(transport), certificates_(certificates) {}

RTCQuicTransport::~RTCQuicTransport() = default;

RTCIceTransport* RTCQuicTransport::transport() const {
  return transport_;
}

String RTCQuicTransport::state() const {
  switch (state_) {
    case RTCQuicTransportState::kNew:
      return "new";
    case RTCQuicTransportState::kConnecting:
      return "connecting";
    case RTCQuicTransportState::kConnected:
      return "connected";
    case RTCQuicTransportState::kClosed:
      return "closed";
    case RTCQuicTransportState::kFailed:
      return "failed";
  }
  return String();
}

void RTCQuicTransport::getLocalParameters(RTCQuicParameters& result) const {
  HeapVector<RTCDtlsFingerprint> fingerprints;
  for (const auto& certificate : certificates_) {
    // TODO(github.com/w3c/webrtc-quic/issues/33): The specification says that
    // getLocalParameters should return one fingerprint per certificate but is
    // not clear which one to pick if an RTCCertificate has multiple
    // fingerprints.
    for (const auto& certificate_fingerprint : certificate->getFingerprints()) {
      fingerprints.push_back(certificate_fingerprint);
    }
  }
  result.setFingerprints(fingerprints);
}

void RTCQuicTransport::getRemoteParameters(
    base::Optional<RTCQuicParameters>& result) const {
  result = base::nullopt;
}

const HeapVector<Member<RTCCertificate>>& RTCQuicTransport::getCertificates()
    const {
  return certificates_;
}

const HeapVector<Member<DOMArrayBuffer>>&
RTCQuicTransport::getRemoteCertificates() const {
  return remote_certificates_;
}

void RTCQuicTransport::stop() {
  for (RTCQuicStream* stream : streams_) {
    stream->Stop();
  }
  streams_.clear();
  state_ = RTCQuicTransportState::kClosed;
}

RTCQuicStream* RTCQuicTransport::createStream(ExceptionState& exception_state) {
  if (RaiseExceptionIfClosed(exception_state)) {
    return nullptr;
  }
  RTCQuicStream* stream = new RTCQuicStream(this);
  streams_.insert(stream);
  return stream;
}

bool RTCQuicTransport::RaiseExceptionIfClosed(
    ExceptionState& exception_state) const {
  if (IsClosed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The RTCQuicTransport's state is 'closed'.");
    return true;
  }
  return false;
}

void RTCQuicTransport::Trace(blink::Visitor* visitor) {
  visitor->Trace(transport_);
  visitor->Trace(certificates_);
  visitor->Trace(remote_certificates_);
  visitor->Trace(streams_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
