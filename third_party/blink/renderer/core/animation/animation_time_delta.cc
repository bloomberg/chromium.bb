// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_time_delta.h"

namespace blink {

#if !defined(BLINK_ANIMATION_USE_TIME_DELTA)
std::ostream& operator<<(std::ostream& os, AnimationTimeDelta time) {
  return os << time.InSecondsF() << " s";
}
#endif

}  // namespace blink
