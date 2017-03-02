// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/FrameView.h"

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

using testing::_;
using testing::AnyNumber;

namespace blink {
namespace {

class MockChromeClient : public EmptyChromeClient {
 public:
  MockChromeClient() : m_hasScheduledAnimation(false) {}

  // ChromeClient
  MOCK_METHOD2(attachRootGraphicsLayer,
               void(GraphicsLayer*, LocalFrame* localRoot));
  MOCK_METHOD3(mockSetToolTip, void(LocalFrame*, const String&, TextDirection));
  void setToolTip(LocalFrame& frame,
                  const String& tooltipText,
                  TextDirection dir) override {
    mockSetToolTip(&frame, tooltipText, dir);
  }

  void scheduleAnimation(FrameViewBase*) override { m_hasScheduledAnimation = true; }
  bool m_hasScheduledAnimation;
};

typedef bool TestParamRootLayerScrolling;
class FrameViewTest
    : public testing::WithParamInterface<TestParamRootLayerScrolling>,
      private ScopedRootLayerScrollingForTest,
      public testing::Test {
 protected:
  FrameViewTest()
      : ScopedRootLayerScrollingForTest(GetParam()),
        m_chromeClient(new MockChromeClient) {
    EXPECT_CALL(chromeClient(), attachRootGraphicsLayer(_, _))
        .Times(AnyNumber());
  }

  ~FrameViewTest() {
    testing::Mock::VerifyAndClearExpectations(&chromeClient());
  }

  void SetUp() override {
    Page::PageClients clients;
    fillWithEmptyClients(clients);
    clients.chromeClient = m_chromeClient.get();
    m_pageHolder = DummyPageHolder::create(IntSize(800, 600), &clients);
    m_pageHolder->page().settings().setAcceleratedCompositingEnabled(true);
  }

  Document& document() { return m_pageHolder->document(); }
  MockChromeClient& chromeClient() { return *m_chromeClient; }

 private:
  Persistent<MockChromeClient> m_chromeClient;
  std::unique_ptr<DummyPageHolder> m_pageHolder;
};

INSTANTIATE_TEST_CASE_P(All, FrameViewTest, ::testing::Bool());

TEST_P(FrameViewTest, SetPaintInvalidationDuringUpdateAllLifecyclePhases) {
  document().body()->setInnerHTML("<div id='a' style='color: blue'>A</div>");
  document().view()->updateAllLifecyclePhases();
  document().getElementById("a")->setAttribute(HTMLNames::styleAttr,
                                               "color: green");
  chromeClient().m_hasScheduledAnimation = false;
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(chromeClient().m_hasScheduledAnimation);
}

TEST_P(FrameViewTest, SetPaintInvalidationOutOfUpdateAllLifecyclePhases) {
  document().body()->setInnerHTML("<div id='a' style='color: blue'>A</div>");
  document().view()->updateAllLifecyclePhases();
  chromeClient().m_hasScheduledAnimation = false;
  document()
      .getElementById("a")
      ->layoutObject()
      ->setShouldDoFullPaintInvalidation();
  EXPECT_TRUE(chromeClient().m_hasScheduledAnimation);
  chromeClient().m_hasScheduledAnimation = false;
  document()
      .getElementById("a")
      ->layoutObject()
      ->setShouldDoFullPaintInvalidation();
  EXPECT_TRUE(chromeClient().m_hasScheduledAnimation);
  chromeClient().m_hasScheduledAnimation = false;
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(chromeClient().m_hasScheduledAnimation);
}

// If we don't hide the tooltip on scroll, it can negatively impact scrolling
// performance. See crbug.com/586852 for details.
TEST_P(FrameViewTest, HideTooltipWhenScrollPositionChanges) {
  document().body()->setInnerHTML(
      "<div style='width:1000px;height:1000px'></div>");
  document().view()->updateAllLifecyclePhases();

  EXPECT_CALL(chromeClient(), mockSetToolTip(document().frame(), String(), _));
  document().view()->layoutViewportScrollableArea()->setScrollOffset(
      ScrollOffset(1, 1), UserScroll);

  // Programmatic scrolling should not dismiss the tooltip, so setToolTip
  // should not be called for this invocation.
  EXPECT_CALL(chromeClient(), mockSetToolTip(document().frame(), String(), _))
      .Times(0);
  document().view()->layoutViewportScrollableArea()->setScrollOffset(
      ScrollOffset(2, 2), ProgrammaticScroll);
}

// NoOverflowInIncrementVisuallyNonEmptyPixelCount tests fail if the number of
// pixels is calculated in 32-bit integer, because 65536 * 65536 would become 0
// if it was calculated in 32-bit and thus it would be considered as empty.
TEST_P(FrameViewTest, NoOverflowInIncrementVisuallyNonEmptyPixelCount) {
  EXPECT_FALSE(document().view()->isVisuallyNonEmpty());
  document().view()->incrementVisuallyNonEmptyPixelCount(IntSize(65536, 65536));
  EXPECT_TRUE(document().view()->isVisuallyNonEmpty());
}

// This test addresses http://crbug.com/696173, in which a call to
// FrameView::updateLayersAndCompositingAfterScrollIfNeeded during layout caused
// a crash as the code was incorrectly assuming that the ancestor overflow layer
// would always be valid.
TEST_P(FrameViewTest, ViewportConstrainedObjectsHandledCorrectlyDuringLayout) {
  document().body()->setInnerHTML(
      "<style>.container { height: 200%; }"
      "#sticky { position: sticky; top: 0; height: 50px; }</style>"
      "<div class='container'><div id='sticky'></div></div>");
  document().view()->updateAllLifecyclePhases();

  LayoutBoxModelObject* sticky = toLayoutBoxModelObject(
      document().getElementById("sticky")->layoutObject());

  // Deliberately invalidate the ancestor overflow layer. This approximates
  // http://crbug.com/696173, in which the ancestor overflow layer can be null
  // during layout.
  sticky->layer()->updateAncestorOverflowLayer(nullptr);

  // This call should not crash.
  document().view()->layoutViewportScrollableArea()->setScrollOffset(
      ScrollOffset(0, 100), ProgrammaticScroll);
}

TEST_P(FrameViewTest, StyleChangeUpdatesViewportConstrainedObjects) {
  // When using root layer scrolling there is no concept of viewport constrained
  // objects, so skip this test.
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled())
    return;

  document().body()->setInnerHTML(
      "<style>.container { height: 200%; }"
      "#sticky { position: sticky; top: 0; height: 50px; }</style>"
      "<div class='container'><div id='sticky'></div></div>");
  document().view()->updateAllLifecyclePhases();

  LayoutBoxModelObject* sticky = toLayoutBoxModelObject(
      document().getElementById("sticky")->layoutObject());

  EXPECT_TRUE(
      document().view()->viewportConstrainedObjects()->contains(sticky));

  // Making the element non-sticky should remove it from the set of
  // viewport-constrained objects.
  document().getElementById("sticky")->setAttribute(HTMLNames::styleAttr,
                                                    "position: relative");
  document().view()->updateAllLifecyclePhases();

  EXPECT_FALSE(
      document().view()->viewportConstrainedObjects()->contains(sticky));

  // And making it sticky again should put it back in that list.
  document().getElementById("sticky")->setAttribute(HTMLNames::styleAttr, "");
  document().view()->updateAllLifecyclePhases();

  EXPECT_TRUE(
      document().view()->viewportConstrainedObjects()->contains(sticky));
}

}  // namespace
}  // namespace blink
