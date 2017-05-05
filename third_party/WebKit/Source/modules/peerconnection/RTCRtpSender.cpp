// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/peerconnection/RTCRtpSender.h"

#include "modules/mediastream/MediaStreamTrack.h"

namespace blink {

RTCRtpSender::RTCRtpSender(std::unique_ptr<WebRTCRtpSender> sender,
                           MediaStreamTrack* track)
    : sender_(std::move(sender)), track_(track) {
  DCHECK(sender_);
  DCHECK(track_);
}

MediaStreamTrack* RTCRtpSender::track() {
  DCHECK((track_ && sender_->Track() &&
          sender_->Track()->Id() == static_cast<WebString>(track_->id())) ||
         (!track_ && !sender_->Track()));
  return track_;
}

DEFINE_TRACE(RTCRtpSender) {
  visitor->Trace(track_);
}

}  // namespace blink
