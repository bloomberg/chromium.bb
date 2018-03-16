// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/RenderedPosition.h"

#include "build/build_config.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/testing/EditingTestBase.h"
#include "core/frame/Settings.h"
#include "core/html/forms/HTMLInputElement.h"
#include "core/layout/LayoutBox.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "core/paint/compositing/CompositedSelection.h"
#include "platform/testing/UseMockScrollbarSettings.h"
#include "platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class RenderedPositionTest : public ::testing::WithParamInterface<bool>,
                             private ScopedRootLayerScrollingForTest,
                             public EditingTestBase {
 public:
  RenderedPositionTest() : ScopedRootLayerScrollingForTest(GetParam()) {}
  void SetUp() override {
    EditingTestBase::SetUp();
    GetPage().GetSettings().SetAcceleratedCompositingEnabled(true);
    GetDocument().View()->SetParentVisible(true);
    GetDocument().View()->SetSelfVisible(true);
    LoadAhem();
  }

  void FocusAndSelectAll() {
    HTMLInputElement* target =
        ToHTMLInputElement(GetDocument().getElementById("target"));
    DCHECK(target);
    target->focus();
    Selection().SetSelection(
        SelectionInDOMTree::Builder()
            .SelectAllChildren(*target->InnerEditorElement())
            .Build(),
        SetSelectionOptions::Builder().SetShouldShowHandle(true).Build());
    UpdateAllLifecyclePhases();
  }

 private:
  UseMockScrollbarSettings mock_scrollbars_;
};

INSTANTIATE_TEST_CASE_P(All, RenderedPositionTest, ::testing::Bool());

TEST_P(RenderedPositionTest, ComputeCompositedSelection) {
  SetBodyContent(R"HTML(
      <!DOCTYPE html>
      input {
        font: 10px/1 Ahem;
        padding: 0;
        border: 0;
      }
      <input id=target width=20 value='test test test test test tes tes test'
      style='width: 100px; height: 20px;'>
  )HTML");

  FocusAndSelectAll();

  const CompositedSelection& composited_selection =
      RenderedPosition::ComputeCompositedSelection(Selection());
  EXPECT_FALSE(composited_selection.start.hidden);
  EXPECT_TRUE(composited_selection.end.hidden);
}

TEST_P(RenderedPositionTest, PositionInScrollableRoot) {
  SetBodyContent(R"HTML(
      <!DOCTYPE html>
      <style>
        body {
           margin: 0;
           height: 2000px;
           width: 2000px;
        }
        input {
          font: 10px/1 Ahem;
          padding: 0;
          border: 0;
          width: 100px;
          height: 20px;
          position: absolute;
          top: 900px;
          left: 1000px;
        }
      </style>
      <input id=target width=20 value='test test test test test tes tes test'>
  )HTML");

  FocusAndSelectAll();

  ScrollableArea* root_scroller = GetDocument().View()->GetScrollableArea();
  root_scroller->SetScrollOffset(ScrollOffset(800, 500), kProgrammaticScroll);
  ASSERT_EQ(ScrollOffset(800, 500), root_scroller->GetScrollOffset());

  UpdateAllLifecyclePhases();

  const CompositedSelection& composited_selection =
      RenderedPosition::ComputeCompositedSelection(Selection());

  // Top-left corner should be around (1000, 905) - 10px centered in 20px
  // height.
  EXPECT_EQ(FloatPoint(1000, 905),
            composited_selection.start.edge_top_in_layer);
  EXPECT_EQ(FloatPoint(1000, 915),
            composited_selection.start.edge_bottom_in_layer);
  EXPECT_EQ(FloatPoint(1369, 905), composited_selection.end.edge_top_in_layer);
  EXPECT_EQ(FloatPoint(1369, 915),
            composited_selection.end.edge_bottom_in_layer);
}

TEST_P(RenderedPositionTest, PositionInScroller) {
  SetBodyContent(R"HTML(
      <!DOCTYPE html>
      <style>
        body {
           margin: 0;
           height: 2000px;
           width: 2000px;
        }
        input {
          font: 10px/1 Ahem;
          padding: 0;
          border: 0;
          width: 100px;
          height: 20px;
          position: absolute;
          top: 900px;
          left: 1000px;
        }

        #scroller {
          width: 300px;
          height: 300px;
          position: absolute;
          left: 300px;
          top: 400px;
          overflow: scroll;
          border: 200px;
          will-change: transform;
        }

        #space {
          width: 2000px;
          height: 2000px;
        }
      </style>
      <div id="scroller">
        <div id="space"></div>
        <input id=target width=20 value='test test test test test tes tes test'>
      </div>
  )HTML");

  FocusAndSelectAll();

  Element* e = GetDocument().getElementById("scroller");
  PaintLayerScrollableArea* scroller =
      ToLayoutBox(e->GetLayoutObject())->GetScrollableArea();
  scroller->SetScrollOffset(ScrollOffset(900, 800), kProgrammaticScroll);
  ASSERT_EQ(ScrollOffset(900, 800), scroller->GetScrollOffset());

  UpdateAllLifecyclePhases();

  const CompositedSelection& composited_selection =
      RenderedPosition::ComputeCompositedSelection(Selection());

  // Top-left corner should be around (1000, 905) - 10px centered in 20px
  // height.
  EXPECT_EQ(FloatPoint(1000, 905),
            composited_selection.start.edge_top_in_layer);
  EXPECT_EQ(FloatPoint(1000, 915),
            composited_selection.start.edge_bottom_in_layer);
  EXPECT_EQ(FloatPoint(1369, 905), composited_selection.end.edge_top_in_layer);
  EXPECT_EQ(FloatPoint(1369, 915),
            composited_selection.end.edge_bottom_in_layer);
}

}  // namespace blink
