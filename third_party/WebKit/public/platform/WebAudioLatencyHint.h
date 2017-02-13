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
      m_category = kCategoryInteractive;
    } else if (category == "balanced") {
      m_category = kCategoryBalanced;
    } else if (category == "playback") {
      m_category = kCategoryPlayback;
    } else {
      NOTREACHED();
      m_category = kCategoryInteractive;
    }
  }

  explicit WebAudioLatencyHint(AudioContextLatencyCategory category)
      : m_category(category), m_seconds(0) {}
  explicit WebAudioLatencyHint(double seconds)
      : m_category(kCategoryExact), m_seconds(seconds) {}

  AudioContextLatencyCategory category() const { return m_category; }
  double seconds() const {
    DCHECK_EQ(m_category, kCategoryExact);
    return m_seconds;
  }

 private:
  AudioContextLatencyCategory m_category;
  double m_seconds;
};

}  // namespace blink

#endif
