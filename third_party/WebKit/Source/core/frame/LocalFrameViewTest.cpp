// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/LocalFrameView.h"

#include <memory>

#include "core/html/HTMLElement.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"

#include "core/paint/PaintPropertyTreePrinter.h"

using ::testing::_;
using ::testing::AnyNumber;

namespace blink {
namespace {

class AnimationMockChromeClient : public EmptyChromeClient {
 public:
  AnimationMockChromeClient() : has_scheduled_animation_(false) {}

  // ChromeClient
  MOCK_METHOD2(AttachRootGraphicsLayer,
               void(GraphicsLayer*, LocalFrame* localRoot));
  MOCK_METHOD3(MockSetToolTip, void(LocalFrame*, const String&, TextDirection));
  void SetToolTip(LocalFrame& frame,
                  const String& tooltip_text,
                  TextDirection dir) override {
    MockSetToolTip(&frame, tooltip_text, dir);
  }

  void ScheduleAnimation(const PlatformFrameView*) override {
    has_scheduled_animation_ = true;
  }
  bool has_scheduled_animation_;
};

typedef bool TestParamRootLayerScrolling;
class LocalFrameViewTest
    : public ::testing::WithParamInterface<TestParamRootLayerScrolling>,
      private ScopedRootLayerScrollingForTest,
      public RenderingTest {
 protected:
  LocalFrameViewTest()
      : ScopedRootLayerScrollingForTest(GetParam()),
        RenderingTest(SingleChildLocalFrameClient::Create()),
        chrome_client_(new AnimationMockChromeClient) {
    EXPECT_CALL(ChromeClient(), AttachRootGraphicsLayer(_, _))
        .Times(AnyNumber());
  }

  ~LocalFrameViewTest() {
    ::testing::Mock::VerifyAndClearExpectations(&ChromeClient());
  }

  ChromeClient& GetChromeClient() const override { return *chrome_client_; }

  void SetUp() override {
    RenderingTest::SetUp();
    EnableCompositing();
  }

  AnimationMockChromeClient& ChromeClient() const { return *chrome_client_; }

 private:
  Persistent<AnimationMockChromeClient> chrome_client_;
};

INSTANTIATE_TEST_CASE_P(All, LocalFrameViewTest, ::testing::Bool());

TEST_P(LocalFrameViewTest, SetPaintInvalidationDuringUpdateAllLifecyclePhases) {
  SetBodyInnerHTML("<div id='a' style='color: blue'>A</div>");
  GetDocument().getElementById("a")->setAttribute(HTMLNames::styleAttr,
                                                  "color: green");
  ChromeClient().has_scheduled_animation_ = false;
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(ChromeClient().has_scheduled_animation_);
}

TEST_P(LocalFrameViewTest, SetPaintInvalidationOutOfUpdateAllLifecyclePhases) {
  SetBodyInnerHTML("<div id='a' style='color: blue'>A</div>");
  ChromeClient().has_scheduled_animation_ = false;
  GetDocument()
      .getElementById("a")
      ->GetLayoutObject()
      ->SetShouldDoFullPaintInvalidation();
  EXPECT_TRUE(ChromeClient().has_scheduled_animation_);
  ChromeClient().has_scheduled_animation_ = false;
  GetDocument()
      .getElementById("a")
      ->GetLayoutObject()
      ->SetShouldDoFullPaintInvalidation();
  EXPECT_TRUE(ChromeClient().has_scheduled_animation_);
  ChromeClient().has_scheduled_animation_ = false;
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(ChromeClient().has_scheduled_animation_);
}

// If we don't hide the tooltip on scroll, it can negatively impact scrolling
// performance. See crbug.com/586852 for details.
TEST_P(LocalFrameViewTest, HideTooltipWhenScrollPositionChanges) {
  SetBodyInnerHTML("<div style='width:1000px;height:1000px'></div>");

  EXPECT_CALL(ChromeClient(),
              MockSetToolTip(GetDocument().GetFrame(), String(), _));
  GetDocument().View()->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(1, 1), kUserScroll);

  // Programmatic scrolling should not dismiss the tooltip, so setToolTip
  // should not be called for this invocation.
  EXPECT_CALL(ChromeClient(),
              MockSetToolTip(GetDocument().GetFrame(), String(), _))
      .Times(0);
  GetDocument().View()->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(2, 2), kProgrammaticScroll);
}

// NoOverflowInIncrementVisuallyNonEmptyPixelCount tests fail if the number of
// pixels is calculated in 32-bit integer, because 65536 * 65536 would become 0
// if it was calculated in 32-bit and thus it would be considered as empty.
TEST_P(LocalFrameViewTest, NoOverflowInIncrementVisuallyNonEmptyPixelCount) {
  EXPECT_FALSE(GetDocument().View()->IsVisuallyNonEmpty());
  GetDocument().View()->IncrementVisuallyNonEmptyPixelCount(
      IntSize(65536, 65536));
  EXPECT_TRUE(GetDocument().View()->IsVisuallyNonEmpty());
}

// This test addresses http://crbug.com/696173, in which a call to
// LocalFrameView::UpdateLayersAndCompositingAfterScrollIfNeeded during layout
// caused a crash as the code was incorrectly assuming that the ancestor
// overflow layer would always be valid.
TEST_P(LocalFrameViewTest,
       ViewportConstrainedObjectsHandledCorrectlyDuringLayout) {
  SetBodyInnerHTML(
      "<style>.container { height: 200%; }"
      "#sticky { position: sticky; top: 0; height: 50px; }</style>"
      "<div class='container'><div id='sticky'></div></div>");

  LayoutBoxModelObject* sticky = ToLayoutBoxModelObject(
      GetDocument().getElementById("sticky")->GetLayoutObject());

  // Deliberately invalidate the ancestor overflow layer. This approximates
  // http://crbug.com/696173, in which the ancestor overflow layer can be null
  // during layout.
  sticky->Layer()->UpdateAncestorOverflowLayer(nullptr);

  // This call should not crash.
  GetDocument().View()->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 100), kProgrammaticScroll);
}

TEST_P(LocalFrameViewTest, StyleChangeUpdatesViewportConstrainedObjects) {
  // When using root layer scrolling there is no concept of viewport constrained
  // objects, so skip this test.
  if (RuntimeEnabledFeatures::RootLayerScrollingEnabled())
    return;

  SetBodyInnerHTML(
      "<style>.container { height: 200%; }"
      "#sticky1 { position: sticky; top: 0; height: 50px; }"
      "#sticky2 { position: sticky; height: 50px; }</style>"
      "<div class='container'>"
      "  <div id='sticky1'></div>"
      "  <div id='sticky2'></div>"
      "</div>");

  LayoutBoxModelObject* sticky1 = ToLayoutBoxModelObject(
      GetDocument().getElementById("sticky1")->GetLayoutObject());
  LayoutBoxModelObject* sticky2 = ToLayoutBoxModelObject(
      GetDocument().getElementById("sticky2")->GetLayoutObject());

  EXPECT_TRUE(
      GetDocument().View()->ViewportConstrainedObjects()->Contains(sticky1));
  // #sticky2 is not viewport constrained because it has no anchor edges.
  EXPECT_FALSE(
      GetDocument().View()->ViewportConstrainedObjects()->Contains(sticky2));

  // Making the element non-sticky should remove it from the set of
  // viewport-constrained objects.
  GetDocument().getElementById("sticky1")->setAttribute(HTMLNames::styleAttr,
                                                        "position: relative");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_FALSE(
      GetDocument().View()->ViewportConstrainedObjects()->Contains(sticky1));

  // And making it sticky1 again should put it back in that list.
  GetDocument().getElementById("sticky1")->setAttribute(HTMLNames::styleAttr,
                                                        "");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_TRUE(
      GetDocument().View()->ViewportConstrainedObjects()->Contains(sticky1));

  // Adding an anchor edge on the non-anchored sticky should add it to the
  // viewport-constrained objects.
  GetDocument().getElementById("sticky2")->setAttribute(HTMLNames::styleAttr,
                                                        "top: 0");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_TRUE(
      GetDocument().View()->ViewportConstrainedObjects()->Contains(sticky2));

  // Removing the anchor edge on a sticky position element should remove it from
  // the viewport-constrained objects.
  GetDocument().getElementById("sticky2")->setAttribute(HTMLNames::styleAttr,
                                                        "");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_FALSE(
      GetDocument().View()->ViewportConstrainedObjects()->Contains(sticky2));
}

TEST_P(LocalFrameViewTest, UpdateLifecyclePhasesForPrintingDetachedFrame) {
  SetBodyInnerHTML("<iframe style='display: none'></iframe>");
  SetChildFrameHTML("A");

  ChildDocument().SetPrinting(Document::kPrinting);
  ChildDocument().View()->UpdateLifecyclePhasesForPrinting();

  // The following checks that the detached frame has been walked for PrePaint.
  EXPECT_EQ(DocumentLifecycle::kPrePaintClean,
            GetDocument().Lifecycle().GetState());
  EXPECT_EQ(DocumentLifecycle::kPrePaintClean,
            ChildDocument().Lifecycle().GetState());
  if (RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    auto* child_layout_view = ChildDocument().GetLayoutView();
    ASSERT_TRUE(child_layout_view->FirstFragment());
    EXPECT_TRUE(child_layout_view->FirstFragment()->PaintProperties());
  } else {
    EXPECT_TRUE(ChildDocument().View()->PreTranslation());
  }
}

}  // namespace
}  // namespace blink
