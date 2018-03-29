// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RTCRtpReceiver_h
#define RTCRtpReceiver_h

#include <map>

#include "modules/mediastream/MediaStream.h"
#include "modules/mediastream/MediaStreamTrack.h"
#include "modules/peerconnection/RTCRtpContributingSource.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Member.h"
#include "platform/heap/Visitor.h"
#include "public/platform/WebRTCRtpReceiver.h"

namespace blink {

// https://w3c.github.io/webrtc-pc/#rtcrtpreceiver-interface
class RTCRtpReceiver final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Takes ownership of the receiver.
  RTCRtpReceiver(std::unique_ptr<WebRTCRtpReceiver>,
                 MediaStreamTrack*,
                 MediaStreamVector);

  MediaStreamTrack* track() const;
  const HeapVector<Member<RTCRtpContributingSource>>& getContributingSources();
  ScriptPromise getStats(ScriptState*);

  const WebRTCRtpReceiver& web_receiver() const;
  MediaStreamVector streams() const;
  void UpdateSourcesIfNeeded();

  virtual void Trace(blink::Visitor*);

 private:
#if DCHECK_IS_ON()
  bool StateMatchesWebReceiver() const;
#endif  // DCHECK_IS_ON()
  void SetContributingSourcesNeedsUpdating();

  std::unique_ptr<WebRTCRtpReceiver> receiver_;
  Member<MediaStreamTrack> track_;
  MediaStreamVector streams_;

  // The current contributing sources (|getContributingSources|).
  HeapVector<Member<RTCRtpContributingSource>> contributing_sources_;
  bool contributing_sources_needs_updating_ = true;
};

}  // namespace blink

#endif  // RTCRtpReceiver_h
