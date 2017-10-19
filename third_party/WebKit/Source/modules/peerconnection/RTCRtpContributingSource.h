// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RTCRtpContributingSource_h
#define RTCRtpContributingSource_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Member.h"
#include "platform/heap/Visitor.h"

namespace blink {

class RTCRtpReceiver;
class WebRTCRtpContributingSource;

// https://w3c.github.io/webrtc-pc/#dom-rtcrtpcontributingsource
class RTCRtpContributingSource final
    : public GarbageCollectedFinalized<RTCRtpContributingSource>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  RTCRtpContributingSource(RTCRtpReceiver*, const WebRTCRtpContributingSource&);
  // The source's source ID must match |source|.
  void UpdateMembers(const WebRTCRtpContributingSource&);

  double timestamp();
  uint32_t source() const;

  virtual void Trace(blink::Visitor*);

 private:
  Member<RTCRtpReceiver> receiver_;
  double timestamp_ms_;
  const uint32_t source_;
};

}  // namespace blink

#endif  // RTCRtpContributingSource_h
