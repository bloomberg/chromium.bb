// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"

#include "base/logging.h"

namespace blink {

bool SchedulingPolicy::IsFeatureSticky(SchedulingPolicy::Feature feature) {
  switch (feature) {
    case Feature::kWebSocket:
    case Feature::kWebRTC:
      return false;
    case Feature::kStickyFeatureForTesting:
      return true;
    case Feature::kCount:
      NOTREACHED();
      return false;
  }
}

}  // namespace blink
