// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRTCRtpSender_h
#define WebRTCRtpSender_h

#include "WebCommon.h"
#include "WebRTCRtpParameters.h"
#include "WebRTCStats.h"
#include "WebRTCVoidRequest.h"
#include "WebString.h"

namespace blink {

class WebMediaStreamTrack;
class WebRTCDTMFSenderHandler;

// Implementations of this interface keep the corresponding WebRTC-layer sender
// alive through reference counting. Multiple |WebRTCRtpSender|s could reference
// the same sender; check for equality with |id|.
// https://w3c.github.io/webrtc-pc/#rtcrtpsender-interface
class BLINK_PLATFORM_EXPORT WebRTCRtpSender {
 public:
  virtual ~WebRTCRtpSender();

  // Two |WebRTCRtpSender|s referencing the same WebRTC-layer sender have the
  // same |id|. IDs are guaranteed to be unique amongst senders but they are
  // allowed to be reused after a sender is destroyed.
  virtual uintptr_t Id() const = 0;
  virtual WebMediaStreamTrack Track() const = 0;
  // TODO(hbos): Replace WebRTCVoidRequest by something resolving promises based
  // on RTCError, as to surface both exception type and error message.
  // https://crbug.com/790007
  virtual void ReplaceTrack(WebMediaStreamTrack, WebRTCVoidRequest) = 0;
  virtual std::unique_ptr<WebRTCDTMFSenderHandler> GetDtmfSender() const = 0;
  virtual std::unique_ptr<WebRTCRtpParameters> GetParameters() const = 0;
  virtual void GetStats(std::unique_ptr<blink::WebRTCStatsReportCallback>) = 0;
};

}  // namespace blink

#endif  // WebRTCRtpSender_h
