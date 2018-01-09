// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintPropertyTreeBuilderTest_h
#define PaintPropertyTreeBuilderTest_h

#include "core/paint/PaintControllerPaintTest.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

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

#endif  // PaintPropertyTreeBuilderTest_h
