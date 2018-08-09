// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_transport.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_parameters.h"

namespace blink {

RTCIceTransport* RTCIceTransport::Create() {
  return new RTCIceTransport();
}

RTCIceTransport::RTCIceTransport() = default;

String RTCIceTransport::role() const {
  return String();
}

String RTCIceTransport::state() const {
  return "new";
}

String RTCIceTransport::gatheringState() const {
  return "new";
}

const HeapVector<Member<RTCIceCandidate>>& RTCIceTransport::getLocalCandidates()
    const {
  return local_candidates_;
}

const HeapVector<Member<RTCIceCandidate>>&
RTCIceTransport::getRemoteCandidates() const {
  return remote_candidates_;
}

void RTCIceTransport::getSelectedCandidatePair(
    base::Optional<RTCIceCandidatePair>& result) const {
  result = selected_candidate_pair_;
}

void RTCIceTransport::getLocalParameters(
    base::Optional<RTCIceParameters>& result) const {
  result = base::nullopt;
}

void RTCIceTransport::getRemoteParameters(
    base::Optional<RTCIceParameters>& result) const {
  result = base::nullopt;
}

void RTCIceTransport::Trace(blink::Visitor* visitor) {
  visitor->Trace(local_candidates_);
  visitor->Trace(remote_candidates_);
  visitor->Trace(selected_candidate_pair_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
