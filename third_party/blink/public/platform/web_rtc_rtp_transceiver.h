// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_RTP_TRANSCEIVER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_RTP_TRANSCEIVER_H_

#include <vector>

#include "base/optional.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_rtc_rtp_receiver.h"
#include "third_party/blink/public/platform/web_rtc_rtp_sender.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/webrtc/api/rtptransceiverinterface.h"

namespace blink {

// Interface for content to implement as to allow the surfacing of transceivers.
// TODO(hbos): [Onion Soup] Remove the content layer versions of this class and
// rely on webrtc directly from blink. Requires coordination with senders and
// receivers. https://crbug.com/787254
class BLINK_PLATFORM_EXPORT WebRTCRtpTransceiver {
 public:
  virtual ~WebRTCRtpTransceiver();

  // Identifies the webrtc-layer transceiver. Multiple WebRTCRtpTransceiver can
  // exist for the same webrtc-layer transceiver.
  virtual uintptr_t Id() const = 0;
  virtual WebString Mid() const = 0;
  virtual std::unique_ptr<WebRTCRtpSender> Sender() const = 0;
  virtual std::unique_ptr<WebRTCRtpReceiver> Receiver() const = 0;
  virtual bool Stopped() const = 0;
  virtual webrtc::RtpTransceiverDirection Direction() const = 0;
  virtual base::Optional<webrtc::RtpTransceiverDirection> CurrentDirection()
      const = 0;
  virtual void Stop() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_RTP_TRANSCEIVER_H_
