// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRTCRtpSender_h
#define WebRTCRtpSender_h

#include "WebCommon.h"
#include "WebString.h"

namespace blink {

class WebMediaStreamTrack;

// Implementations of this interface keep the corresponding WebRTC-layer sender
// alive through reference counting. Multiple |WebRTCRtpSender|s could reference
// the same sender; check for equality with |id|.
// https://w3c.github.io/webrtc-pc/#rtcrtpsender-interface
class BLINK_PLATFORM_EXPORT WebRTCRtpSender {
 public:
  virtual ~WebRTCRtpSender();

  // Two |WebRTCRtpSender|s referencing the same WebRTC-layer sender have the
  // same |id|. IDs are guaranteed to be unique amongst senders but they are
  // allowed to be reused after a sender is destroyed.
  virtual uintptr_t Id() const = 0;
  virtual const WebMediaStreamTrack* Track() const = 0;
};

}  // namespace blink

#endif  // WebRTCRtpSender_h
