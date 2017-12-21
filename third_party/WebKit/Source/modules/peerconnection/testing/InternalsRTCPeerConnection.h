// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InternalsRTCPeerConnection_h
#define InternalsRTCPeerConnection_h

#include "modules/peerconnection/RTCPeerConnection.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class Internals;

class InternalsRTCPeerConnection {
  STATIC_ONLY(InternalsRTCPeerConnection);

 public:
  static int peerConnectionCount(Internals&);
  static int peerConnectionCountLimit(Internals&);
};

}  // namespace blink

#endif  // InternalsRTCPeerConnection_h
