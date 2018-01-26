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

double RTCRtpContributingSource::timestamp() const {
  return timestamp_ms_;
}

uint32_t RTCRtpContributingSource::source() const {
  return source_;
}

void RTCRtpContributingSource::Trace(blink::Visitor* visitor) {
  visitor->Trace(receiver_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
