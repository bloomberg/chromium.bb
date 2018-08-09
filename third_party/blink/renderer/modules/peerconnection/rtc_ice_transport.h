// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ICE_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ICE_TRANSPORT_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_ice_candidate_pair.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate_pair.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_parameters.h"

namespace blink {

class RTCIceCandidate;

// Blink bindings for the RTCIceTransport JavaScript object.
class MODULES_EXPORT RTCIceTransport final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static RTCIceTransport* Create();

  // rtc_ice_transport.idl
  String role() const;
  String state() const;
  String gatheringState() const;
  const HeapVector<Member<RTCIceCandidate>>& getLocalCandidates() const;
  const HeapVector<Member<RTCIceCandidate>>& getRemoteCandidates() const;
  void getSelectedCandidatePair(
      base::Optional<RTCIceCandidatePair>& result) const;
  void getLocalParameters(base::Optional<RTCIceParameters>& result) const;
  void getRemoteParameters(base::Optional<RTCIceParameters>& result) const;

  // For garbage collection.
  void Trace(blink::Visitor* visitor) override;

 private:
  RTCIceTransport();

  HeapVector<Member<RTCIceCandidate>> local_candidates_;
  HeapVector<Member<RTCIceCandidate>> remote_candidates_;
  base::Optional<RTCIceCandidatePair> selected_candidate_pair_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ICE_TRANSPORT_H_
