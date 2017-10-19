// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/peerconnection/RTCRtpContributingSource.h"

#include "modules/peerconnection/RTCRtpReceiver.h"
#include "public/platform/WebRTCRtpContributingSource.h"

namespace blink {

RTCRtpContributingSource::RTCRtpContributingSource(
    RTCRtpReceiver* receiver,
    const WebRTCRtpContributingSource& webContributingSource)
    : receiver_(receiver),
      timestamp_ms_(webContributingSource.TimestampMs()),
      source_(webContributingSource.Source()) {
  DCHECK(receiver_);
}

void RTCRtpContributingSource::UpdateMembers(
    const WebRTCRtpContributingSource& webContributingSource) {
  timestamp_ms_ = webContributingSource.TimestampMs();
  DCHECK_EQ(webContributingSource.Source(), source_);
}

double RTCRtpContributingSource::timestamp() {
  receiver_->UpdateSourcesIfNeeded();
  return timestamp_ms_;
}

uint32_t RTCRtpContributingSource::source() const {
  // Skip |receiver_->UpdateSourcesIfNeeded()|, |source_| is a constant.
  return source_;
}

void RTCRtpContributingSource::Trace(blink::Visitor* visitor) {
  visitor->Trace(receiver_);
}

}  // namespace blink
