// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintPropertyTreePrinter.h"

#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTestHelper.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

#if DCHECK_IS_ON()

namespace blink {

typedef bool TestParamRootLayerScrolling;
class PaintPropertyTreePrinterTest
    : public ::testing::WithParamInterface<TestParamRootLayerScrolling>,
      private ScopedSlimmingPaintV2ForTest,
      private ScopedRootLayerScrollingForTest,
      public RenderingTest {
 public:
  PaintPropertyTreePrinterTest()
      : ScopedSlimmingPaintV2ForTest(true),
        ScopedRootLayerScrollingForTest(GetParam()),
        RenderingTest(SingleChildLocalFrameClient::Create()) {}

 private:
  void SetUp() override {
    Settings::SetMockScrollbarsEnabled(true);

    RenderingTest::SetUp();
    EnableCompositing();
  }

  void TearDown() override {
    RenderingTest::TearDown();

    Settings::SetMockScrollbarsEnabled(false);
  }
};

INSTANTIATE_TEST_CASE_P(All, PaintPropertyTreePrinterTest, ::testing::Bool());

TEST_P(PaintPropertyTreePrinterTest, SimpleTransformTree) {
  SetBodyInnerHTML("hello world");
  String transform_tree_as_string =
      transformPropertyTreeAsString(*GetDocument().View());
  EXPECT_THAT(transform_tree_as_string.Ascii().data(),
              ::testing::MatchesRegex("root .*"
                                      "  .*Translation \\(.*\\) .*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleClipTree) {
  SetBodyInnerHTML("hello world");
  String clip_tree_as_string = clipPropertyTreeAsString(*GetDocument().View());
  EXPECT_THAT(clip_tree_as_string.Ascii().data(),
              ::testing::MatchesRegex("root .*"
                                      "  .*Clip \\(.*\\) .*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleEffectTree) {
  SetBodyInnerHTML("<div style='opacity: 0.9;'>hello world</div>");
  String effect_tree_as_string =
      effectPropertyTreeAsString(*GetDocument().View());
  EXPECT_THAT(effect_tree_as_string.Ascii().data(),
              ::testing::MatchesRegex("root .*"
                                      "  Effect \\(LayoutBlockFlow DIV\\) .*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleScrollTree) {
  SetBodyInnerHTML("<div style='height: 4000px;'>hello world</div>");
  String scroll_tree_as_string =
      scrollPropertyTreeAsString(*GetDocument().View());
  EXPECT_THAT(scroll_tree_as_string.Ascii().data(),
              ::testing::MatchesRegex("root .*"
                                      "  Scroll \\(.*\\) .*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleTransformTreePath) {
  SetBodyInnerHTML(
      "<div id='transform' style='transform: translate3d(10px, 10px, "
      "0px);'></div>");
  LayoutObject* transformed_object =
      GetDocument().getElementById("transform")->GetLayoutObject();
  const auto* transformed_object_properties =
      transformed_object->FirstFragment()->PaintProperties();
  String transform_path_as_string =
      transformed_object_properties->Transform()->ToTreeString();
  EXPECT_THAT(transform_path_as_string.Ascii().data(),
              ::testing::MatchesRegex("root .* transform.*"
                                      "  .* transform.*"
                                      "    .* transform.*"
                                      "       .* transform.*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleClipTreePath) {
  SetBodyInnerHTML(
      "<div id='clip' style='position: absolute; clip: rect(10px, 80px, 70px, "
      "40px);'></div>");
  LayoutObject* clipped_object =
      GetDocument().getElementById("clip")->GetLayoutObject();
  const auto* clipped_object_properties =
      clipped_object->FirstFragment()->PaintProperties();
  String clip_path_as_string =
      clipped_object_properties->CssClip()->ToTreeString();
  EXPECT_THAT(clip_path_as_string.Ascii().data(),
              ::testing::MatchesRegex("root .* rect.*"
                                      "  .* rect.*"
                                      "    .* rect.*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleEffectTreePath) {
  SetBodyInnerHTML("<div id='effect' style='opacity: 0.9;'></div>");
  LayoutObject* effect_object =
      GetDocument().getElementById("effect")->GetLayoutObject();
  const auto* effect_object_properties =
      effect_object->FirstFragment()->PaintProperties();
  String effect_path_as_string =
      effect_object_properties->Effect()->ToTreeString();
  EXPECT_THAT(effect_path_as_string.Ascii().data(),
              ::testing::MatchesRegex("root .* opacity.*"
                                      "  .* opacity.*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleScrollTreePath) {
  SetBodyInnerHTML(
      "<div id='scroll' style='overflow: scroll; height: 100px;'>"
      "  <div id='forceScroll' style='height: 4000px;'></div>"
      "</div>");
  LayoutObject* scroll_object =
      GetDocument().getElementById("scroll")->GetLayoutObject();
  const auto* scroll_object_properties =
      scroll_object->FirstFragment()->PaintProperties();
  String scroll_path_as_string = scroll_object_properties->ScrollTranslation()
                                     ->ScrollNode()
                                     ->ToTreeString();
  EXPECT_THAT(scroll_path_as_string.Ascii().data(),
              ::testing::MatchesRegex("root .* parent.*"
                                      "  .* parent.*"));
}

}  // namespace blink

#endif  // if DCHECK_IS_ON()
