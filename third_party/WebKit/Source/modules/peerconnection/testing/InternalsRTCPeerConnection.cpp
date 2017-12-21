// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/peerconnection/testing/InternalsRTCPeerConnection.h"

namespace blink {

int InternalsRTCPeerConnection::peerConnectionCount(Internals& internals) {
  return RTCPeerConnection::PeerConnectionCount();
}

int InternalsRTCPeerConnection::peerConnectionCountLimit(Internals& internals) {
  return RTCPeerConnection::PeerConnectionCountLimit();
}

}  // namespace blink
