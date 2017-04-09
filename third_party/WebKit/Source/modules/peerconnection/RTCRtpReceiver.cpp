// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/peerconnection/RTCRtpReceiver.h"

#include "wtf/text/WTFString.h"

namespace blink {

RTCRtpReceiver::RTCRtpReceiver(std::unique_ptr<WebRTCRtpReceiver> receiver,
                               MediaStreamTrack* track)
    : receiver_(std::move(receiver)), track_(track) {
  DCHECK(receiver_);
  DCHECK(track_);
  DCHECK_EQ(static_cast<String>(receiver_->Track().Id()), track_->id());
}

MediaStreamTrack* RTCRtpReceiver::track() const {
  return track_;
}

DEFINE_TRACE(RTCRtpReceiver) {
  visitor->Trace(track_);
}

}  // namespace blink
