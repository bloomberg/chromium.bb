// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/peerconnection/RTCRtpReceiver.h"

#include "wtf/text/WTFString.h"

namespace blink {

RTCRtpReceiver::RTCRtpReceiver(std::unique_ptr<WebRTCRtpReceiver> receiver,
                               MediaStreamTrack* track)
    : m_receiver(std::move(receiver)), m_track(track) {
  DCHECK(m_receiver);
  DCHECK(m_track);
  DCHECK_EQ(static_cast<String>(m_receiver->track().id()), m_track->id());
}

MediaStreamTrack* RTCRtpReceiver::track() const {
  return m_track;
}

DEFINE_TRACE(RTCRtpReceiver) {
  visitor->trace(m_track);
}

}  // namespace blink
