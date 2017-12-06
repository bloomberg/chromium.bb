// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/peerconnection/RTCRtpSender.h"

#include "modules/mediastream/MediaStreamTrack.h"

namespace blink {

namespace {

bool TrackEquals(MediaStreamTrack* track,
                 const WebMediaStreamTrack& web_track) {
  if (track)
    return track->Component() == static_cast<MediaStreamComponent*>(web_track);
  return web_track.IsNull();
}

}  // namespace

RTCRtpSender::RTCRtpSender(std::unique_ptr<WebRTCRtpSender> sender,
                           MediaStreamTrack* track)
    : sender_(std::move(sender)), track_(track) {
  DCHECK(sender_);
  DCHECK(track_);
}

MediaStreamTrack* RTCRtpSender::track() {
  DCHECK(TrackEquals(track_, sender_->Track()));
  return track_;
}

WebRTCRtpSender* RTCRtpSender::web_sender() {
  return sender_.get();
}

void RTCRtpSender::SetTrack(MediaStreamTrack* track) {
  DCHECK(TrackEquals(track, sender_->Track()));
  track_ = track;
}

void RTCRtpSender::Trace(blink::Visitor* visitor) {
  visitor->Trace(track_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
