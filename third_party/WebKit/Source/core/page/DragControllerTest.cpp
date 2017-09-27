// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/DragController.h"

#include "core/clipboard/DataObject.h"
#include "core/clipboard/DataTransfer.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/page/AutoscrollController.h"
#include "core/page/DragData.h"
#include "core/page/DragSession.h"
#include "core/page/DragState.h"
#include "core/testing/sim/SimDisplayItemList.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "platform/DragImage.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class DragMockChromeClient : public EmptyChromeClient {
 public:
  DragMockChromeClient() {}

  void StartDragging(LocalFrame*,
                     const WebDragData&,
                     WebDragOperationsMask,
                     const WebImage& drag_image,
                     const WebPoint& drag_image_offset) {
    last_drag_image_size = drag_image.Size();
    last_drag_image_offset = drag_image_offset;
  }

  WebSize last_drag_image_size;
  WebPoint last_drag_image_offset;
};

class DragControllerTest : public RenderingTest {
 protected:
  DragControllerTest()
      : RenderingTest(SingleChildLocalFrameClient::Create()),
        chrome_client_(new DragMockChromeClient) {}
  LocalFrame& GetFrame() const { return *GetDocument().GetFrame(); }
  ChromeClient& GetChromeClient() const override { return *chrome_client_; }
  DragMockChromeClient& ChromeClient() const { return *chrome_client_; }

  void UpdateAllLifecyclePhases() {
    GetDocument().View()->UpdateAllLifecyclePhases();
  }

 private:
  Persistent<DragMockChromeClient> chrome_client_;
};

TEST_F(DragControllerTest, DragImageForSelectionUsesPageScaleFactor) {
  SetBodyInnerHTML(
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

TEST_F(DragControllerTest, DragImageForSelectionClipsToViewport) {
  SetBodyInnerHTML(
      "<style>"
      "  * { margin: 0; } "
      "  html, body { height: 2000px; }"
      "  div {"
      "    width: 20px;"
      "    height: 1000px;"
      "    font-size: 30px;"
      "    overflow: hidden;"
      "    margin-top: 2px;"
      "  }"
      "</style>"
      "<div>"
      "  a<br>b<br>c<br>d<br>e<br>f<br>g<br>h<br>i<br>j<br>k<br>l<br>m<br>n<br>"
      "  a<br>b<br>c<br>d<br>e<br>f<br>g<br>h<br>i<br>j<br>k<br>l<br>m<br>n<br>"
      "  a<br>b<br>c<br>d<br>e<br>f<br>g<br>h<br>i<br>j<br>k<br>l<br>m<br>n<br>"
      "</div>");
  const int page_scale_factor = 2;
  GetFrame().GetPage()->SetPageScaleFactor(page_scale_factor);
  GetFrame().Selection().SelectAll();

  const int node_width = 20;
  const int node_height = 1000;
  const int node_margin_top = 2;
  const int viewport_height_dip = 600;
  const int viewport_height_css = viewport_height_dip / page_scale_factor;

  // The top of the node should be visible but the bottom should be outside the
  // viewport.
  FloatRect expected_selection(0, node_margin_top, node_width,
                               viewport_height_css - node_margin_top);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(GetFrame()));
  auto selection_image(DragController::DragImageForSelection(GetFrame(), 1));
  IntSize expected_image_size(RoundedIntSize(expected_selection.Size()));
  expected_image_size.Scale(page_scale_factor);
  EXPECT_EQ(expected_image_size, selection_image->Size());

  // Scroll 500 css px down so the top of the node is outside the viewport.
  // Because the viewport is scaled to 300 css px tall, the bottom of the node
  // should also be outside the viewport. Therefore, the selection should cover
  // the entire viewport.
  int scroll_offset = 500;
  LocalFrameView* frame_view = GetDocument().View();
  frame_view->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, scroll_offset), kProgrammaticScroll);
  expected_selection =
      FloatRect(0, scroll_offset, node_width, viewport_height_css);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(GetFrame()));
  selection_image = DragController::DragImageForSelection(GetFrame(), 1);
  expected_image_size = IntSize(RoundedIntSize(expected_selection.Size()));
  expected_image_size.Scale(page_scale_factor);
  EXPECT_EQ(expected_image_size, selection_image->Size());

  // Scroll 800 css px down so the top of the node is outside the viewport and
  // the bottom of the node is now visible.
  scroll_offset = 800;
  frame_view->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, scroll_offset), kProgrammaticScroll);
  expected_selection = FloatRect(0, scroll_offset, node_width,
                                 node_height + node_margin_top - scroll_offset);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(GetFrame()));
  selection_image = DragController::DragImageForSelection(GetFrame(), 1);
  expected_image_size = IntSize(RoundedIntSize(expected_selection.Size()));
  expected_image_size.Scale(page_scale_factor);
  EXPECT_EQ(expected_image_size, selection_image->Size());
}

TEST_F(DragControllerTest, DragImageForSelectionClipsChildFrameToViewport) {
  SetBodyInnerHTML(
      "<style>"
      "  * { margin: 0; } "
      "  html, body { height: 2000px; }"
      "  iframe {"
      "    margin-top: 200px;"
      "    border: none;"
      "    width: 50px;"
      "    height: 50px;"
      "  }"
      "</style>"
      "<iframe></iframe>");
  SetChildFrameHTML(
      "<style>"
      "  * { margin: 0; } "
      "  html, body { height: 2000px; }"
      "  div {"
      "    width: 30px;"
      "    height: 20px;"
      "    font-size: 30px;"
      "    overflow: hidden;"
      "    margin-top: 5px;"
      "    margin-bottom: 500px;"
      "  }"
      "</style>"
      "<div>abcdefg</div>");
  UpdateAllLifecyclePhases();
  auto& child_frame = *ToLocalFrame(GetFrame().Tree().FirstChild());
  child_frame.Selection().SelectAll();

  // The iframe's selection rect is in the frame's local coordinates and should
  // not include the iframe's margin.
  FloatRect expected_selection(0, 5, 30, 20);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(child_frame));
  auto selection_image(DragController::DragImageForSelection(child_frame, 1));
  IntSize expected_image_size(RoundedIntSize(expected_selection.Size()));
  EXPECT_EQ(expected_image_size, selection_image->Size());

  // The iframe's selection rect is in the frame's local coordinates and should
  // not include scroll offset.
  int scroll_offset = 50;
  LocalFrameView* frame_view = GetDocument().View();
  frame_view->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, scroll_offset), kProgrammaticScroll);
  expected_selection = FloatRect(0, 5, 30, 20);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(child_frame));
  selection_image = DragController::DragImageForSelection(child_frame, 1);
  expected_image_size = IntSize(RoundedIntSize(expected_selection.Size()));
  EXPECT_EQ(expected_image_size, selection_image->Size());

  // The parent frame's scroll offset of 210 should cause the iframe content to
  // be shifted which should cause the iframe's selection rect to be clipped by
  // the visual viewport.
  scroll_offset = 210;
  frame_view->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, scroll_offset), kProgrammaticScroll);
  expected_selection = FloatRect(0, 10, 30, 15);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(child_frame));
  selection_image = DragController::DragImageForSelection(child_frame, 1);
  expected_image_size = IntSize(RoundedIntSize(expected_selection.Size()));
  EXPECT_EQ(expected_image_size, selection_image->Size());

  // Scrolling the iframe should shift the content so it is further under the
  // visual viewport clip.
  int iframe_scroll_offset = 7;
  child_frame.View()->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, iframe_scroll_offset), kProgrammaticScroll);
  expected_selection = FloatRect(0, 17, 30, 8);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(child_frame));
  selection_image = DragController::DragImageForSelection(child_frame, 1);
  expected_image_size = IntSize(RoundedIntSize(expected_selection.Size()));
  EXPECT_EQ(expected_image_size, selection_image->Size());
}

TEST_F(DragControllerTest,
       DragImageForSelectionClipsChildFrameToViewportWithPageScaleFactor) {
  SetBodyInnerHTML(
      "<style>"
      "  * { margin: 0; } "
      "  html, body { height: 2000px; }"
      "  iframe {"
      "    margin-top: 200px;"
      "    border: none;"
      "    width: 50px;"
      "    height: 50px;"
      "  }"
      "</style>"
      "<iframe></iframe>");
  SetChildFrameHTML(
      "<style>"
      "  * { margin: 0; } "
      "  html, body { height: 2000px; }"
      "  div {"
      "    width: 30px;"
      "    height: 20px;"
      "    font-size: 30px;"
      "    overflow: hidden;"
      "    margin-top: 5px;"
      "    margin-bottom: 500px;"
      "  }"
      "</style>"
      "<div>abcdefg</div>");
  const int page_scale_factor = 2;
  GetFrame().GetPage()->SetPageScaleFactor(page_scale_factor);
  UpdateAllLifecyclePhases();
  auto& child_frame = *ToLocalFrame(GetFrame().Tree().FirstChild());
  child_frame.Selection().SelectAll();

  // The iframe's selection rect is in the frame's local coordinates and should
  // not include the iframe's margin.
  FloatRect expected_selection(0, 5, 30, 20);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(child_frame));
  auto selection_image(DragController::DragImageForSelection(child_frame, 1));
  IntSize expected_image_size(RoundedIntSize(expected_selection.Size()));
  expected_image_size.Scale(page_scale_factor);
  EXPECT_EQ(expected_image_size, selection_image->Size());

  // The iframe's selection rect is in the frame's local coordinates and should
  // not include the parent frame's scroll offset.
  int scroll_offset = 50;
  LocalFrameView* frame_view = GetDocument().View();
  frame_view->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, scroll_offset), kProgrammaticScroll);
  expected_selection = FloatRect(0, 5, 30, 20);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(child_frame));
  selection_image = DragController::DragImageForSelection(child_frame, 1);
  expected_image_size = IntSize(RoundedIntSize(expected_selection.Size()));
  expected_image_size.Scale(page_scale_factor);
  EXPECT_EQ(expected_image_size, selection_image->Size());

  // The parent frame's scroll offset of 210 should cause the iframe content to
  // be shifted which should cause the iframe's selection rect to be clipped by
  // the visual viewport.
  scroll_offset = 210;
  frame_view->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, scroll_offset), kProgrammaticScroll);
  expected_selection = FloatRect(0, 10, 30, 15);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(child_frame));
  selection_image = DragController::DragImageForSelection(child_frame, 1);
  expected_image_size = IntSize(RoundedIntSize(expected_selection.Size()));
  expected_image_size.Scale(page_scale_factor);
  EXPECT_EQ(expected_image_size, selection_image->Size());

  // Scrolling the iframe should shift the content so it is further under the
  // visual viewport clip.
  int iframe_scroll_offset = 7;
  child_frame.View()->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, iframe_scroll_offset), kProgrammaticScroll);
  expected_selection = FloatRect(0, 17, 30, 8);
  EXPECT_EQ(expected_selection, DragController::ClippedSelection(child_frame));
  selection_image = DragController::DragImageForSelection(child_frame, 1);
  expected_image_size = IntSize(RoundedIntSize(expected_selection.Size()));
  expected_image_size.Scale(page_scale_factor);
  EXPECT_EQ(expected_image_size, selection_image->Size());
}

TEST_F(DragControllerTest, DragImageOffsetWithPageScaleFactor) {
  SetBodyInnerHTML(
      "<style>"
      "  * { margin: 0; } "
      "  div {"
      "    width: 50px;"
      "    height: 40px;"
      "    font-size: 30px;"
      "    overflow: hidden;"
      "    margin-top: 2px;"
      "  }"
      "</style>"
      "<div id='drag'>abcdefg<br>abcdefg<br>abcdefg</div>");
  const int page_scale_factor = 2;
  GetFrame().GetPage()->SetPageScaleFactor(page_scale_factor);
  GetFrame().Selection().SelectAll();

  WebMouseEvent mouse_event(WebInputEvent::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::kTimeStampForTesting);
  mouse_event.button = WebMouseEvent::Button::kRight;
  mouse_event.SetPositionInWidget(5, 10);

  auto& drag_state = GetFrame().GetPage()->GetDragController().GetDragState();
  drag_state.drag_type_ = kDragSourceActionSelection;
  drag_state.drag_src_ = GetDocument().getElementById("drag");
  drag_state.drag_data_transfer_ = DataTransfer::Create(
      DataTransfer::kDragAndDrop, kDataTransferWritable, DataObject::Create());
  GetFrame().GetPage()->GetDragController().StartDrag(
      &GetFrame(), drag_state, mouse_event, IntPoint(5, 10));

  IntSize expected_image_size = IntSize(50, 40);
  expected_image_size.Scale(page_scale_factor);
  EXPECT_EQ(expected_image_size, IntSize(ChromeClient().last_drag_image_size));
  // The drag image has a margin of 2px which should offset the selection
  // image by 2px from the dragged location of (5, 10).
  IntPoint expected_offset = IntPoint(5, 10 - 2);
  expected_offset.Scale(page_scale_factor, page_scale_factor);
  EXPECT_EQ(expected_offset, IntPoint(ChromeClient().last_drag_image_offset));
}

TEST_F(DragControllerTest, DragLinkWithPageScaleFactor) {
  SetBodyInnerHTML(
      "<style>"
      "  * { margin: 0; } "
      "  a {"
      "    width: 50px;"
      "    height: 40px;"
      "    font-size: 30px;"
      "    margin-top: 2px;"
      "    display: block;"
      "  }"
      "</style>"
      "<a id='drag' href='https://foobarbaz.com'>foobarbaz</a>");
  const int page_scale_factor = 2;
  GetFrame().GetPage()->SetPageScaleFactor(page_scale_factor);
  GetFrame().Selection().SelectAll();

  WebMouseEvent mouse_event(WebInputEvent::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::kTimeStampForTesting);
  mouse_event.button = WebMouseEvent::Button::kRight;
  mouse_event.SetFrameScale(1);
  mouse_event.SetPositionInWidget(5, 10);

  auto& drag_state = GetFrame().GetPage()->GetDragController().GetDragState();
  drag_state.drag_type_ = kDragSourceActionLink;
  drag_state.drag_src_ = GetDocument().getElementById("drag");
  drag_state.drag_data_transfer_ = DataTransfer::Create(
      DataTransfer::kDragAndDrop, kDataTransferWritable, DataObject::Create());
  GetFrame().GetPage()->GetDragController().StartDrag(
      &GetFrame(), drag_state, mouse_event, IntPoint(5, 10));

  IntSize link_image_size = IntSize(ChromeClient().last_drag_image_size);
  // The drag link image should be a textual representation of the drag url in a
  // system font (see: DragImageForLink in DragController.cpp) and should not be
  // an empty image.
  EXPECT_GT(link_image_size.Area(), 0u);
  // Unlike the drag image in DragImageOffsetWithPageScaleFactor, the link
  // image is not offset by margin because the link image is not based on the
  // link's painting but instead is a generated image of the link's url. Because
  // link_image_size is already scaled, no additional scaling is expected.
  IntPoint expected_offset = IntPoint(link_image_size.Width() / 2, 2);
  // The offset is mapped using integers which can introduce rounding errors
  // (see TODO in DragController::DoSystemDrag) so we accept values near our
  // expectation until more precise offset mapping is available.
  EXPECT_NEAR(expected_offset.X(), ChromeClient().last_drag_image_offset.x, 1);
  EXPECT_NEAR(expected_offset.Y(), ChromeClient().last_drag_image_offset.y, 1);
}

}  // namespace blink
