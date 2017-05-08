// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundBleedAvoidance_h
#define BackgroundBleedAvoidance_h

namespace blink {

enum BackgroundBleedAvoidance {
  kBackgroundBleedNone,
  kBackgroundBleedShrinkBackground,
  kBackgroundBleedClipOnly,
  kBackgroundBleedClipLayer,
};

inline bool BleedAvoidanceIsClipping(BackgroundBleedAvoidance bleed_avoidance) {
  return bleed_avoidance == kBackgroundBleedClipOnly ||
         bleed_avoidance == kBackgroundBleedClipLayer;
}

}  // namespace blink

#endif  // BackgroundBleedAvoidance_h
