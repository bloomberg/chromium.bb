// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RuntimeEnabledFeaturesTestHelpers_h
#define RuntimeEnabledFeaturesTestHelpers_h

#include "platform/runtime_enabled_features.h"
#include "platform/wtf/Assertions.h"

namespace blink {

template <bool (&getter)(), void (&setter)(bool)>
class ScopedRuntimeEnabledFeatureForTest {
 public:
  ScopedRuntimeEnabledFeatureForTest(bool enabled)
      : enabled_(enabled), original_(getter()) {
    setter(enabled);
  }

  ~ScopedRuntimeEnabledFeatureForTest() {
    CHECK_EQ(enabled_, getter());
    setter(original_);
  }

 private:
  bool enabled_;
  bool original_;
};

typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::CompositeOpaqueFixedPositionEnabled,
    RuntimeEnabledFeatures::SetCompositeOpaqueFixedPositionEnabled>
    ScopedCompositeFixedPositionForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::CompositeOpaqueScrollersEnabled,
    RuntimeEnabledFeatures::SetCompositeOpaqueScrollersEnabled>
    ScopedCompositeOpaqueScrollersForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::AnimationWorkletEnabled,
    RuntimeEnabledFeatures::SetAnimationWorkletEnabled>
    ScopedAnimationWorkleForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::RootLayerScrollingEnabled,
    RuntimeEnabledFeatures::SetRootLayerScrollingEnabled>
    ScopedRootLayerScrollingForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::SlimmingPaintV2Enabled,
    RuntimeEnabledFeatures::SetSlimmingPaintV2Enabled>
    ScopedSlimmingPaintV2ForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled,
    RuntimeEnabledFeatures::SetPaintUnderInvalidationCheckingEnabled>
    ScopedPaintUnderInvalidationCheckingForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::AccessibilityObjectModelEnabled,
    RuntimeEnabledFeatures::SetAccessibilityObjectModelEnabled>
    ScopedAccessibilityObjectModelForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::MojoBlobsEnabled,
    RuntimeEnabledFeatures::SetMojoBlobsEnabled>
    ScopedMojoBlobsForTest;

}  // namespace blink

#endif  // RuntimeEnabledFeaturesTestHelpers_h
