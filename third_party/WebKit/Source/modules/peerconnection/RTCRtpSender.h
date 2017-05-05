// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RTCRtpSender_h
#define RTCRtpSender_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Member.h"
#include "platform/heap/Visitor.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebRTCRtpSender.h"

namespace blink {

class MediaStreamTrack;

// https://w3c.github.io/webrtc-pc/#rtcrtpsender-interface
class RTCRtpSender final : public GarbageCollectedFinalized<RTCRtpSender>,
                           public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  RTCRtpSender(std::unique_ptr<WebRTCRtpSender>, MediaStreamTrack*);

  MediaStreamTrack* track();

  DECLARE_VIRTUAL_TRACE();

 private:
  std::unique_ptr<WebRTCRtpSender> sender_;
  Member<MediaStreamTrack> track_;
};

}  // namespace blink

#endif  // RTCRtpSender_h
