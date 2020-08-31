// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_RTP_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_RTP_SOURCE_H_

#include "base/macros.h"
#include "base/optional.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/webrtc/api/rtp_receiver_interface.h"

namespace base {
class TimeTicks;
}

namespace webrtc {
class RtpSource;
}

namespace blink {

class PLATFORM_EXPORT RTCRtpSource {
 public:
  enum class Type {
    kSSRC,
    kCSRC,
  };

  explicit RTCRtpSource(const webrtc::RtpSource& source);
  ~RTCRtpSource();

  Type SourceType() const;
  base::TimeTicks Timestamp() const;
  uint32_t Source() const;
  base::Optional<double> AudioLevel() const;
  uint32_t RtpTimestamp() const;
  base::Optional<int64_t> CaptureTimestamp() const;

 private:
  const webrtc::RtpSource source_;

  DISALLOW_COPY_AND_ASSIGN(RTCRtpSource);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_RTP_SOURCE_H_
