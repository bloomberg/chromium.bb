// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/DragController.h"

#include "core/clipboard/DataObject.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/PerformanceMonitor.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLElement.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutView.h"
#include "core/page/AutoscrollController.h"
#include "core/page/DragData.h"
#include "core/page/DragSession.h"
#include "core/testing/DummyPageHolder.h"
#include "core/testing/sim/SimDisplayItemList.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "core/timing/Performance.h"
#include "platform/DragImage.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class DragControllerTest : public ::testing::Test {
 protected:
  DragControllerTest() = default;
  ~DragControllerTest() override = default;

  Document& GetDocument() const { return dummy_page_holder_->GetDocument(); }
  LocalFrame& GetFrame() const { return *GetDocument().GetFrame(); }
  Performance* GetPerformance() const { return performance_; }

  void SetBodyContent(const std::string& body_content) {
    GetDocument().body()->setInnerHTML(String::FromUTF8(body_content.c_str()));
    UpdateAllLifecyclePhases();
  }

  void UpdateAllLifecyclePhases() {
    GetDocument().View()->UpdateAllLifecyclePhases();
  }

 private:
  void SetUp() override {
    dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
    performance_ = Performance::Create(&GetFrame());
  }

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<Performance> performance_;
};

TEST_F(DragControllerTest, dragImageForSelectionUsesPageScaleFactor) {
  SetBodyContent(
      "<div>Hello world! This tests that the bitmap for drag image is scaled "
      "by page scale factor</div>");
  GetFrame().GetPage()->GetVisualViewport().SetScale(1);
  GetFrame().Selection().SelectAll();
  UpdateAllLifecyclePhases();
  const std::unique_ptr<DragImage> image1(
      DragController::DragImageForSelection(GetFrame(), 0.75f));
  GetFrame().GetPage()->GetVisualViewport().SetScale(2);
  GetFrame().Selection().SelectAll();
  UpdateAllLifecyclePhases();
  const std::unique_ptr<DragImage> image2(
      DragController::DragImageForSelection(GetFrame(), 0.75f));

  EXPECT_GT(image1->Size().Width(), 0);
  EXPECT_GT(image1->Size().Height(), 0);
  EXPECT_EQ(image1->Size().Width() * 2, image2->Size().Width());
  EXPECT_EQ(image1->Size().Height() * 2, image2->Size().Height());
}

class DragControllerSimTest : public ::testing::WithParamInterface<bool>,
                              private ScopedRootLayerScrollingForTest,
                              public SimTest {
 public:
  DragControllerSimTest() : ScopedRootLayerScrollingForTest(GetParam()) {}
};

INSTANTIATE_TEST_CASE_P(All, DragControllerSimTest, ::testing::Bool());

// Tests that dragging a URL onto a WebWidget that doesn't navigate on Drag and
// Drop clears out the Autoscroll state. Regression test for
// https://crbug.com/733996.
TEST_P(DragControllerSimTest, DropURLOnNonNavigatingClearsState) {
  WebView().GetPage()->GetSettings().SetNavigateOnDragDrop(false);
  WebView().Resize(WebSize(800, 600));
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  // Page must be scrollable so that Autoscroll is engaged.
  main_resource.Complete(
      "<!DOCTYPE html>"
      "<style>body,html { height: 1000px; width: 1000px; }</style>");

  Compositor().BeginFrame();

  DataObject* object = DataObject::Create();
  object->SetURLAndTitle("https://www.example.com/index.html", "index");
  DragData data(
      object, IntPoint(10, 10), IntPoint(10, 10),
      static_cast<DragOperation>(kDragOperationCopy | kDragOperationLink |
                                 kDragOperationMove));

  WebView().GetPage()->GetDragController().DragEnteredOrUpdated(
      &data, *GetDocument().GetFrame());

  // The page should tell the AutoscrollController about the drag.
  EXPECT_TRUE(
      WebView().GetPage()->GetAutoscrollController().AutoscrollInProgress());

  WebView().GetPage()->GetDragController().PerformDrag(
      &data, *GetDocument().GetFrame());

  // Once we've "performed" the drag (in which nothing happens), the
  // AutoscrollController should have been cleared.
  EXPECT_FALSE(
      WebView().GetPage()->GetAutoscrollController().AutoscrollInProgress());
}

}  // namespace blink
