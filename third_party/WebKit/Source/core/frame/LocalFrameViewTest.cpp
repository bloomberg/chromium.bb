// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/LocalFrameView.h"

#include <memory>

#include "bindings/core/v8/ExceptionState.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLElement.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutObject.h"
#include "core/loader/EmptyClients.h"
#include "core/page/Page.h"
#include "core/paint/PaintLayer.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/heap/Handle.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
      public ::testing::Test {
 protected:
  LocalFrameViewTest()
      : ScopedRootLayerScrollingForTest(GetParam()),
        chrome_client_(new AnimationMockChromeClient) {
    EXPECT_CALL(ChromeClient(), AttachRootGraphicsLayer(_, _))
        .Times(AnyNumber());
  }

  ~LocalFrameViewTest() {
    ::testing::Mock::VerifyAndClearExpectations(&ChromeClient());
  }

  void SetUp() override {
    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = chrome_client_.Get();
    page_holder_ = DummyPageHolder::Create(IntSize(800, 600), &clients);
    page_holder_->GetPage().GetSettings().SetAcceleratedCompositingEnabled(
        true);
  }

  Document& GetDocument() { return page_holder_->GetDocument(); }
  AnimationMockChromeClient& ChromeClient() { return *chrome_client_; }

 private:
  Persistent<AnimationMockChromeClient> chrome_client_;
  std::unique_ptr<DummyPageHolder> page_holder_;
};

INSTANTIATE_TEST_CASE_P(All, LocalFrameViewTest, ::testing::Bool());

TEST_P(LocalFrameViewTest, SetPaintInvalidationDuringUpdateAllLifecyclePhases) {
  GetDocument().body()->setInnerHTML("<div id='a' style='color: blue'>A</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  GetDocument().getElementById("a")->setAttribute(HTMLNames::styleAttr,
                                                  "color: green");
  ChromeClient().has_scheduled_animation_ = false;
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(ChromeClient().has_scheduled_animation_);
}

TEST_P(LocalFrameViewTest, SetPaintInvalidationOutOfUpdateAllLifecyclePhases) {
  GetDocument().body()->setInnerHTML("<div id='a' style='color: blue'>A</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();
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
  GetDocument().body()->setInnerHTML(
      "<div style='width:1000px;height:1000px'></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

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
  GetDocument().body()->setInnerHTML(
      "<style>.container { height: 200%; }"
      "#sticky { position: sticky; top: 0; height: 50px; }</style>"
      "<div class='container'><div id='sticky'></div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

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

  GetDocument().body()->setInnerHTML(
      "<style>.container { height: 200%; }"
      "#sticky1 { position: sticky; top: 0; height: 50px; }"
      "#sticky2 { position: sticky; height: 50px; }</style>"
      "<div class='container'>"
      "  <div id='sticky1'></div>"
      "  <div id='sticky2'></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

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

}  // namespace
}  // namespace blink
