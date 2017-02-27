// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
        RenderingTest(SingleChildLocalFrameClient::create()) {}

 protected:
  void loadTestData(const char* fileName);

  // The following helpers return paint property nodes associated with the main
  // FrameView, accounting for differences from the RootLayerScrolls setting.
  const TransformPaintPropertyNode* framePreTranslation();
  const TransformPaintPropertyNode* frameScrollTranslation();
  const ClipPaintPropertyNode* frameContentClip();
  const ScrollPaintPropertyNode* frameScroll(FrameView* = nullptr);

  // Return the local border box's paint offset. For more details, see
  // ObjectPaintProperties::localBorderBoxProperties().
  LayoutPoint paintOffset(const LayoutObject*);

  const ObjectPaintProperties* paintPropertiesForElement(const char* name);

 private:
  void SetUp() override;
  void TearDown() override;
};

}  // namespace blink
