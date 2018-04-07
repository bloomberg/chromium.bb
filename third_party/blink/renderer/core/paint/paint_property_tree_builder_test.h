// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_PROPERTY_TREE_BUILDER_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_PROPERTY_TREE_BUILDER_TEST_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class TransformPaintPropertyNode;
class ClipPaintPropertyNode;
class ScrollPaintPropertyNode;
class LayoutPoint;

typedef bool TestParamRootLayerScrolling;
class PaintPropertyTreeBuilderTest : public PaintControllerPaintTest {
 public:
  PaintPropertyTreeBuilderTest()
      : PaintControllerPaintTest(SingleChildLocalFrameClient::Create()) {}

 protected:
  void LoadTestData(const char* file_name);

  // The following helpers return paint property nodes associated with the main
  // LocalFrameView, accounting for differences from the RootLayerScrolls
  // setting.
  const TransformPaintPropertyNode* FramePreTranslation(
      const LocalFrameView* = nullptr);
  const TransformPaintPropertyNode* FrameScrollTranslation(
      const LocalFrameView* = nullptr);
  const ClipPaintPropertyNode* FrameContentClip(
      const LocalFrameView* = nullptr);
  const ScrollPaintPropertyNode* FrameScroll(const LocalFrameView* = nullptr);

  // Return the local border box's paint offset. For more details, see
  // ObjectPaintProperties::localBorderBoxProperties().
  LayoutPoint PaintOffset(const LayoutObject*);

  const ObjectPaintProperties* PaintPropertiesForElement(const char* name);

 private:
  void SetUp() override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_PROPERTY_TREE_BUILDER_TEST_H_
