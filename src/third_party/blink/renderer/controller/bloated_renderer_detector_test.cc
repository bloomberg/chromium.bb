// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/bloated_renderer_detector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/wtf/scoped_mock_clock.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class BloatedRendererDetectorTest : public testing::Test {
 public:
  static TimeDelta GetMockAfterCooldown() {
    return TimeDelta::FromMinutes(
        BloatedRendererDetector::kMinimumCooldownInMinutes + 1);
  }

  static TimeDelta GetMockBeforeCooldown() {
    return TimeDelta::FromMinutes(
        BloatedRendererDetector::kMinimumCooldownInMinutes - 1);
  }
};

TEST_F(BloatedRendererDetectorTest, ForwardToBrowser) {
  WTF::ScopedMockClock clock;
  clock.Advance(GetMockAfterCooldown());
  BloatedRendererDetector detector(TimeTicks{});
  EXPECT_EQ(NearV8HeapLimitHandling::kForwardedToBrowser,
            detector.OnNearV8HeapLimitOnMainThreadImpl());
}

TEST_F(BloatedRendererDetectorTest, CooldownTime) {
  WTF::ScopedMockClock clock;
  clock.Advance(GetMockBeforeCooldown());
  BloatedRendererDetector detector(TimeTicks{});
  EXPECT_EQ(NearV8HeapLimitHandling::kIgnoredDueToCooldownTime,
            detector.OnNearV8HeapLimitOnMainThreadImpl());
}

TEST_F(BloatedRendererDetectorTest, MultipleDetections) {
  WTF::ScopedMockClock clock;
  clock.Advance(GetMockAfterCooldown());
  BloatedRendererDetector detector(TimeTicks{});
  EXPECT_EQ(NearV8HeapLimitHandling::kForwardedToBrowser,
            detector.OnNearV8HeapLimitOnMainThreadImpl());
  EXPECT_EQ(NearV8HeapLimitHandling::kIgnoredDueToCooldownTime,
            detector.OnNearV8HeapLimitOnMainThreadImpl());
  clock.Advance(GetMockAfterCooldown());
  EXPECT_EQ(NearV8HeapLimitHandling::kForwardedToBrowser,
            detector.OnNearV8HeapLimitOnMainThreadImpl());
}

}  // namespace blink
