// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRTCOfferOptions_h
#define WebRTCOfferOptions_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"

namespace blink {

class RTCOfferOptionsPlatform;

class BLINK_PLATFORM_EXPORT WebRTCOfferOptions {
 public:
  WebRTCOfferOptions(int32_t offer_to_receive_audio,
                     int32_t offer_to_receive_video,
                     bool voice_activity_detection,
                     bool ice_restart);
  WebRTCOfferOptions(const WebRTCOfferOptions& other) { Assign(other); }
  ~WebRTCOfferOptions() { Reset(); }

  WebRTCOfferOptions& operator=(const WebRTCOfferOptions& other) {
    Assign(other);
    return *this;
  }

  void Assign(const WebRTCOfferOptions&);

  void Reset();
  bool IsNull() const { return private_.IsNull(); }

  int32_t OfferToReceiveVideo() const;
  int32_t OfferToReceiveAudio() const;
  bool VoiceActivityDetection() const;
  bool IceRestart() const;

#if INSIDE_BLINK
  WebRTCOfferOptions(RTCOfferOptionsPlatform*);
#endif

 private:
  WebPrivatePtr<RTCOfferOptionsPlatform> private_;
};

}  // namespace blink

#endif  // WebRTCOfferOptions_h
