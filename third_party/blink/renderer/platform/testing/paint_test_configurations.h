// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_TEST_CONFIGURATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_TEST_CONFIGURATIONS_H_

#include <gtest/gtest.h>
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

enum {
  kRootLayerScrolling = 1 << 0,
  kSlimmingPaintV175 = 1 << 1,
  kBlinkGenPropertyTrees = 1 << 2,
  kSlimmingPaintV2 = 1 << 3,
  kUnderInvalidationChecking = 1 << 4,
};

class PaintTestConfigurations
    : public testing::WithParamInterface<unsigned>,
      private ScopedRootLayerScrollingForTest,
      private ScopedSlimmingPaintV175ForTest,
      private ScopedBlinkGenPropertyTreesForTest,
      private ScopedSlimmingPaintV2ForTest,
      private ScopedPaintUnderInvalidationCheckingForTest {
 public:
  PaintTestConfigurations()
      : ScopedRootLayerScrollingForTest(GetParam() & kRootLayerScrolling),
        ScopedSlimmingPaintV175ForTest(GetParam() & kSlimmingPaintV175),
        ScopedBlinkGenPropertyTreesForTest(GetParam() & kBlinkGenPropertyTrees),
        ScopedSlimmingPaintV2ForTest(GetParam() & kSlimmingPaintV2),
        ScopedPaintUnderInvalidationCheckingForTest(
            GetParam() & kUnderInvalidationChecking) {}
};

static constexpr unsigned kAllSlimmingPaintTestConfigurations[] = {
    0,
    kSlimmingPaintV175,
    kBlinkGenPropertyTrees | kSlimmingPaintV175 | kRootLayerScrolling,
    kSlimmingPaintV2,
    kRootLayerScrolling,
    kSlimmingPaintV175 | kRootLayerScrolling,
    kSlimmingPaintV2 | kRootLayerScrolling,
};

static constexpr unsigned kSlimmingPaintNonV1TestConfigurations[] = {
    kSlimmingPaintV175,
    kSlimmingPaintV175 | kRootLayerScrolling,
    kSlimmingPaintV2,
    kSlimmingPaintV2 | kRootLayerScrolling,
    kBlinkGenPropertyTrees | kSlimmingPaintV175 | kRootLayerScrolling,
};

static constexpr unsigned kSlimmingPaintV2TestConfigurations[] = {
    kSlimmingPaintV2, kSlimmingPaintV2 | kRootLayerScrolling,
};

static constexpr unsigned kSlimmingPaintVersions[] = {
    0, kSlimmingPaintV175, kBlinkGenPropertyTrees, kSlimmingPaintV2,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_PAINT_TEST_CONFIGURATIONS_H_
