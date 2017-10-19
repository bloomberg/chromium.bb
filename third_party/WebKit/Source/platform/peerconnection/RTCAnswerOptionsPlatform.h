// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RTCAnswerOptionsPlatform_h
#define RTCAnswerOptionsPlatform_h

#include "platform/heap/Handle.h"

namespace blink {

class RTCAnswerOptionsPlatform final
    : public GarbageCollected<RTCAnswerOptionsPlatform> {
 public:
  static RTCAnswerOptionsPlatform* Create(bool voice_activity_detection) {
    return new RTCAnswerOptionsPlatform(voice_activity_detection);
  }

  bool VoiceActivityDetection() const { return voice_activity_detection_; }

  void Trace(blink::Visitor* visitor) {}

 private:
  explicit RTCAnswerOptionsPlatform(bool voice_activity_detection)
      : voice_activity_detection_(voice_activity_detection) {}

  bool voice_activity_detection_;
};

}  // namespace blink

#endif  // RTCAnswerOptionsPlatform_h
