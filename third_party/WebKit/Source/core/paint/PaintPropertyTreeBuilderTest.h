// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintPropertyTreeBuilderTest_h
#define PaintPropertyTreeBuilderTest_h

#include "core/layout/LayoutTestHelper.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class TransformPaintPropertyNode;
class ClipPaintPropertyNode;
class ScrollPaintPropertyNode;
class LayoutPoint;

typedef bool TestParamRootLayerScrolling;
class PaintPropertyTreeBuilderTest
    : public ::testing::WithParamInterface<TestParamRootLayerScrolling>,
      private ScopedSlimmingPaintV2ForTest,
      private ScopedRootLayerScrollingForTest,
      public RenderingTest {
 public:
  PaintPropertyTreeBuilderTest()
      : ScopedSlimmingPaintV2ForTest(true),
        ScopedRootLayerScrollingForTest(GetParam()),
        RenderingTest(SingleChildLocalFrameClient::Create()) {}

 protected:
  void LoadTestData(const char* file_name);

  // The following helpers return paint property nodes associated with the main
  // LocalFrameView, accounting for differences from the RootLayerScrolls
  // setting.
  const TransformPaintPropertyNode* FramePreTranslation();
  const TransformPaintPropertyNode* FrameScrollTranslation();
  const ClipPaintPropertyNode* FrameContentClip();
  const ScrollPaintPropertyNode* FrameScroll(LocalFrameView* = nullptr);

  // Return the local border box's paint offset. For more details, see
  // ObjectPaintProperties::localBorderBoxProperties().
  LayoutPoint PaintOffset(const LayoutObject*);

  const ObjectPaintProperties* PaintPropertiesForElement(const char* name);

 private:
  void SetUp() override;
  void TearDown() override;
};

}  // namespace blink

#endif  // PaintPropertyTreeBuilderTest_h
