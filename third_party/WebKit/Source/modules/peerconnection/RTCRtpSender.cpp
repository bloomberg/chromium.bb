// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/peerconnection/RTCRtpSender.h"

#include "modules/mediastream/MediaStreamTrack.h"

namespace blink {

namespace {

bool TrackEquals(const MediaStreamTrack* track,
                 const WebMediaStreamTrack* web_track) {
  if (track)
    return web_track && web_track->Id() == WebString(track->id());
  return !web_track;
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

WebRTCRtpSender* RTCRtpSender::web_rtp_sender() {
  return sender_.get();
}

void RTCRtpSender::SetTrack(MediaStreamTrack* track) {
  DCHECK(TrackEquals(track, sender_->Track()));
  track_ = track;
}

DEFINE_TRACE(RTCRtpSender) {
  visitor->Trace(track_);
}

}  // namespace blink
