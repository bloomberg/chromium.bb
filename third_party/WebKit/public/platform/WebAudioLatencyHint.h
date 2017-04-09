// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebAudioLatencyHint_h
#define WebAudioLatencyHint_h

#include "public/platform/WebString.h"

namespace blink {

class WebAudioLatencyHint {
 public:
  enum AudioContextLatencyCategory {
    kCategoryInteractive,
    kCategoryBalanced,
    kCategoryPlayback,
    kCategoryExact
  };

  explicit WebAudioLatencyHint(const WebString& category) {
    if (category == "interactive") {
      category_ = kCategoryInteractive;
    } else if (category == "balanced") {
      category_ = kCategoryBalanced;
    } else if (category == "playback") {
      category_ = kCategoryPlayback;
    } else {
      NOTREACHED();
      category_ = kCategoryInteractive;
    }
  }

  explicit WebAudioLatencyHint(AudioContextLatencyCategory category)
      : category_(category), seconds_(0) {}
  explicit WebAudioLatencyHint(double seconds)
      : category_(kCategoryExact), seconds_(seconds) {}

  AudioContextLatencyCategory Category() const { return category_; }
  double Seconds() const {
    DCHECK_EQ(category_, kCategoryExact);
    return seconds_;
  }

 private:
  AudioContextLatencyCategory category_;
  double seconds_;
};

}  // namespace blink

#endif
