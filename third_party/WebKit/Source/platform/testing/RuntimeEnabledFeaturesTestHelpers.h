// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RuntimeEnabledFeaturesTestHelpers_h
#define RuntimeEnabledFeaturesTestHelpers_h

#include "platform/RuntimeEnabledFeatures.h"
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
    RuntimeEnabledFeatures::compositeOpaqueFixedPositionEnabled,
    RuntimeEnabledFeatures::setCompositeOpaqueFixedPositionEnabled>
    ScopedCompositeFixedPositionForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled,
    RuntimeEnabledFeatures::setCompositeOpaqueScrollersEnabled>
    ScopedCompositeOpaqueScrollersForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::compositorWorkerEnabled,
    RuntimeEnabledFeatures::setCompositorWorkerEnabled>
    ScopedCompositorWorkerForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::rootLayerScrollingEnabled,
    RuntimeEnabledFeatures::setRootLayerScrollingEnabled>
    ScopedRootLayerScrollingForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::slimmingPaintV2Enabled,
    RuntimeEnabledFeatures::setSlimmingPaintV2Enabled>
    ScopedSlimmingPaintV2ForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled,
    RuntimeEnabledFeatures::setSlimmingPaintInvalidationEnabled>
    ScopedSlimmingPaintInvalidationForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled,
    RuntimeEnabledFeatures::setPaintUnderInvalidationCheckingEnabled>
    ScopedPaintUnderInvalidationCheckingForTest;

}  // namespace blink

#endif  // RuntimeEnabledFeaturesTestHelpers_h
