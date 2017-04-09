// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RTCRtpReceiver_h
#define RTCRtpReceiver_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/mediastream/MediaStreamTrack.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Member.h"
#include "platform/heap/Visitor.h"
#include "public/platform/WebRTCRtpReceiver.h"

namespace blink {

// https://w3c.github.io/webrtc-pc/#rtcrtpreceiver-interface
class RTCRtpReceiver final : public GarbageCollectedFinalized<RTCRtpReceiver>,
                             public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Takes ownership of the receiver.
  RTCRtpReceiver(std::unique_ptr<WebRTCRtpReceiver>, MediaStreamTrack*);

  MediaStreamTrack* track() const;

  DECLARE_VIRTUAL_TRACE();

 private:
  std::unique_ptr<WebRTCRtpReceiver> receiver_;
  Member<MediaStreamTrack> track_;
};

}  // namespace blink

#endif  // RTCRtpReceiver_h
