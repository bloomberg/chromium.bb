// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RTCTrackEvent_h
#define RTCTrackEvent_h

#include "modules/EventModules.h"
#include "platform/heap/Member.h"
#include "platform/heap/Visitor.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class MediaStream;
class MediaStreamTrack;
class RTCRtpReceiver;
class RTCTrackEventInit;

// https://w3c.github.io/webrtc-pc/#rtctrackevent
class RTCTrackEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static RTCTrackEvent* Create(const AtomicString& type,
                               const RTCTrackEventInit& eventInitDict);
  RTCTrackEvent(RTCRtpReceiver*,
                MediaStreamTrack*,
                const HeapVector<Member<MediaStream>>&);

  RTCRtpReceiver* receiver() const;
  MediaStreamTrack* track() const;
  HeapVector<Member<MediaStream>> streams() const;

  DECLARE_VIRTUAL_TRACE();

 private:
  RTCTrackEvent(const AtomicString& type,
                const RTCTrackEventInit& eventInitDict);

  Member<RTCRtpReceiver> receiver_;
  Member<MediaStreamTrack> track_;
  HeapVector<Member<MediaStream>> streams_;
};

}  // namespace blink

#endif  // RTCTrackEvent_h
