// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RTCPeerConnectionErrorCallback_h
#define RTCPeerConnectionErrorCallback_h

#include "platform/heap/Handle.h"

namespace blink {

class DOMException;

class RTCPeerConnectionErrorCallback
    : public GarbageCollectedFinalized<RTCPeerConnectionErrorCallback> {
 public:
  virtual ~RTCPeerConnectionErrorCallback() {}
  virtual void handleEvent(DOMException*) = 0;
  virtual void Trace(blink::Visitor* visitor) {}
};

}  // namespace blink

#endif  // RTCPeerConnectionErrorCallback_h
