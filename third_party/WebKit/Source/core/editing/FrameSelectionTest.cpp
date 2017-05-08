// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/FrameSelection.h"

#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/FrameCaret.h"
#include "core/editing/SelectionController.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLBodyElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutBlock.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/StdLibExtras.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class FrameSelectionTest : public EditingTestBase {
 protected:
  const VisibleSelection& VisibleSelectionInDOMTree() const {
    return Selection().ComputeVisibleSelectionInDOMTreeDeprecated();
  }
  const VisibleSelectionInFlatTree& GetVisibleSelectionInFlatTree() const {
    return Selection().GetSelectionInFlatTree();
  }

  Text* AppendTextNode(const String& data);
  int LayoutCount() const {
    return GetDummyPageHolder().GetFrameView().LayoutCount();
  }

  PositionWithAffinity CaretPosition() const {
    return Selection().frame_caret_->CaretPosition();
  }

 private:
  Persistent<Text> text_node_;
};

Text* FrameSelectionTest::AppendTextNode(const String& data) {
  Text* text = GetDocument().createTextNode(data);
  GetDocument().body()->AppendChild(text);
  return text;
}

TEST_F(FrameSelectionTest, FirstEphemeralRangeOf) {
  SetBodyContent("<div id=sample>0123456789</div>abc");
  Element* const sample = GetDocument().getElementById("sample");
  Node* const text = sample->firstChild();
  Selection().SetSelectedRange(
      EphemeralRange(Position(text, 3), Position(text, 6)), VP_DEFAULT_AFFINITY,
      SelectionDirectionalMode::kNonDirectional, 0);
  sample->setAttribute(HTMLNames::styleAttr, "display:none");
  // Move |VisibleSelection| before "abc".
  UpdateAllLifecyclePhases();
  const EphemeralRange& range =
      FirstEphemeralRangeOf(Selection().ComputeVisibleSelectionInDOMTree());
  EXPECT_EQ(Position(sample->nextSibling(), 0), range.StartPosition())
      << "firstRagne() should return current selection value";
  EXPECT_EQ(Position(sample->nextSibling(), 0), range.EndPosition());
}

TEST_F(FrameSelectionTest, SetValidSelection) {
  Text* text = AppendTextNode("Hello, World!");
  GetDocument().View()->UpdateAllLifecyclePhases();
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 5))
          .Build());
  EXPECT_FALSE(
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsNone());
}

TEST_F(FrameSelectionTest, PaintCaretShouldNotLayout) {
  Text* text = AppendTextNode("Hello, World!");
  GetDocument().View()->UpdateAllLifecyclePhases();

  GetDocument().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  GetDocument().body()->focus();
  EXPECT_TRUE(GetDocument().body()->IsFocused());

  Selection().SetCaretVisible(true);
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(Position(text, 0)).Build());
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsCaret());
  EXPECT_TRUE(ToLayoutBlock(GetDocument().body()->GetLayoutObject())
                  ->ShouldPaintCursorCaret());

  int start_count = LayoutCount();
  {
    // To force layout in next updateLayout calling, widen view.
    FrameView& frame_view = GetDummyPageHolder().GetFrameView();
    IntRect frame_rect = frame_view.FrameRect();
    frame_rect.SetWidth(frame_rect.Width() + 1);
    frame_rect.SetHeight(frame_rect.Height() + 1);
    GetDummyPageHolder().GetFrameView().SetFrameRect(frame_rect);
  }
  std::unique_ptr<PaintController> paint_controller = PaintController::Create();
  {
    GraphicsContext context(*paint_controller);
    Selection().PaintCaret(context, LayoutPoint());
  }
  paint_controller->CommitNewDisplayItems();
  EXPECT_EQ(start_count, LayoutCount());
}

#define EXPECT_EQ_SELECTED_TEXT(text) \
  EXPECT_EQ(text, WebString(Selection().SelectedText()).Utf8())

TEST_F(FrameSelectionTest, SelectWordAroundPosition) {
  // "Foo Bar  Baz,"
  Text* text = AppendTextNode("Foo Bar&nbsp;&nbsp;Baz,");
  UpdateAllLifecyclePhases();

  // "Fo|o Bar  Baz,"
  EXPECT_TRUE(Selection().SelectWordAroundPosition(
      CreateVisiblePosition(Position(text, 2))));
  EXPECT_EQ_SELECTED_TEXT("Foo");
  // "Foo| Bar  Baz,"
  EXPECT_TRUE(Selection().SelectWordAroundPosition(
      CreateVisiblePosition(Position(text, 3))));
  EXPECT_EQ_SELECTED_TEXT("Foo");
  // "Foo Bar | Baz,"
  EXPECT_FALSE(Selection().SelectWordAroundPosition(
      CreateVisiblePosition(Position(text, 13))));
  // "Foo Bar  Baz|,"
  EXPECT_TRUE(Selection().SelectWordAroundPosition(
      CreateVisiblePosition(Position(text, 22))));
  EXPECT_EQ_SELECTED_TEXT("Baz");
}

// crbug.com/657996
TEST_F(FrameSelectionTest, SelectWordAroundPosition2) {
  SetBodyContent(
      "<p style='width:70px; font-size:14px'>foo bar<em>+</em> baz</p>");
  // "foo bar
  //  b|az"
  Node* const baz = GetDocument().body()->firstChild()->lastChild();
  EXPECT_TRUE(Selection().SelectWordAroundPosition(
      CreateVisiblePosition(Position(baz, 2))));
  EXPECT_EQ_SELECTED_TEXT("baz");
}

TEST_F(FrameSelectionTest, ModifyExtendWithFlatTree) {
  SetBodyContent("<span id=host></span>one");
  SetShadowContent("two<content></content>", "host");
  Element* host = GetDocument().getElementById("host");
  Node* const two = FlatTreeTraversal::FirstChild(*host);
  // Select "two" for selection in DOM tree
  // Select "twoone" for selection in Flat tree
  Selection().SetSelection(
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree(host, 0))
          .Extend(PositionInFlatTree(GetDocument().body(), 2))
          .Build());
  Selection().Modify(FrameSelection::kAlterationExtend, kDirectionForward,
                     kWordGranularity);
  EXPECT_EQ(Position(two, 0), VisibleSelectionInDOMTree().Start());
  EXPECT_EQ(Position(two, 3), VisibleSelectionInDOMTree().end());
  EXPECT_EQ(PositionInFlatTree(two, 0),
            GetVisibleSelectionInFlatTree().Start());
  EXPECT_EQ(PositionInFlatTree(two, 3), GetVisibleSelectionInFlatTree().end());
}

TEST_F(FrameSelectionTest, ModifyWithUserTriggered) {
  SetBodyContent("<div id=sample>abc</div>");
  Element* sample = GetDocument().getElementById("sample");
  const Position end_of_text(sample->firstChild(), 3);
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(end_of_text).Build());

  EXPECT_FALSE(Selection().Modify(FrameSelection::kAlterationMove,
                                  kDirectionForward, kCharacterGranularity,
                                  kNotUserTriggered))
      << "Selection.modify() returns false for non-user-triggered call when "
         "selection isn't modified.";
  EXPECT_EQ(end_of_text,
            Selection().ComputeVisibleSelectionInDOMTreeDeprecated().Start())
      << "Selection isn't modified";

  EXPECT_TRUE(Selection().Modify(FrameSelection::kAlterationMove,
                                 kDirectionForward, kCharacterGranularity,
                                 kUserTriggered))
      << "Selection.modify() returns true for user-triggered call";
  EXPECT_EQ(end_of_text,
            Selection().ComputeVisibleSelectionInDOMTreeDeprecated().Start())
      << "Selection isn't modified";
}

TEST_F(FrameSelectionTest, MoveRangeSelectionTest) {
  // "Foo Bar Baz,"
  Text* text = AppendTextNode("Foo Bar Baz,");
  UpdateAllLifecyclePhases();

  // Itinitializes with "Foo B|a>r Baz," (| means start and > means end).
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 5), Position(text, 6))
          .Build());
  EXPECT_EQ_SELECTED_TEXT("a");

  // "Foo B|ar B>az," with the Character granularity.
  Selection().MoveRangeSelection(CreateVisiblePosition(Position(text, 5)),
                                 CreateVisiblePosition(Position(text, 9)),
                                 kCharacterGranularity);
  EXPECT_EQ_SELECTED_TEXT("ar B");
  // "Foo B|ar B>az," with the Word granularity.
  Selection().MoveRangeSelection(CreateVisiblePosition(Position(text, 5)),
                                 CreateVisiblePosition(Position(text, 9)),
                                 kWordGranularity);
  EXPECT_EQ_SELECTED_TEXT("Bar Baz");
  // "Fo<o B|ar Baz," with the Character granularity.
  Selection().MoveRangeSelection(CreateVisiblePosition(Position(text, 5)),
                                 CreateVisiblePosition(Position(text, 2)),
                                 kCharacterGranularity);
  EXPECT_EQ_SELECTED_TEXT("o B");
  // "Fo<o B|ar Baz," with the Word granularity.
  Selection().MoveRangeSelection(CreateVisiblePosition(Position(text, 5)),
                                 CreateVisiblePosition(Position(text, 2)),
                                 kWordGranularity);
  EXPECT_EQ_SELECTED_TEXT("Foo Bar");
}

// For http://crbug.com/695317
TEST_F(FrameSelectionTest, SelectAllWithInputElement) {
  SetBodyContent("<input>123");
  Element* const input = GetDocument().QuerySelector("input");
  Node* const last_child = GetDocument().body()->lastChild();
  Selection().SelectAll();
  const SelectionInDOMTree& result_in_dom_tree =
      Selection().ComputeVisibleSelectionInDOMTree().AsSelection();
  const SelectionInFlatTree& result_in_flat_tree =
      Selection().ComputeVisibleSelectionInFlatTree().AsSelection();
  EXPECT_EQ(SelectionInDOMTree::Builder(result_in_dom_tree)
                .Collapse(Position::BeforeNode(input))
                .Extend(Position(last_child, 3))
                .Build(),
            result_in_dom_tree);
  EXPECT_EQ(SelectionInFlatTree::Builder(result_in_flat_tree)
                .Collapse(PositionInFlatTree::BeforeNode(input))
                .Extend(PositionInFlatTree(last_child, 3))
                .Build(),
            result_in_flat_tree);
}

TEST_F(FrameSelectionTest, SelectAllWithUnselectableRoot) {
  Element* select = GetDocument().createElement("select");
  GetDocument().ReplaceChild(select, GetDocument().documentElement());
  GetDocument().UpdateStyleAndLayout();
  Selection().SelectAll();
  EXPECT_TRUE(Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsNone())
      << "Nothing should be selected if the "
         "content of the documentElement is not "
         "selctable.";
}

TEST_F(FrameSelectionTest, SelectAllPreservesHandle) {
  SetBodyContent("<div id=sample>abc</div>");
  Element* sample = GetDocument().getElementById("sample");
  const Position end_of_text(sample->firstChild(), 3);
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(end_of_text)
                               .SetIsHandleVisible(false)
                               .Build());
  EXPECT_FALSE(Selection().IsHandleVisible());
  Selection().SelectAll();
  EXPECT_FALSE(Selection().IsHandleVisible())
      << "If handles weren't present before"
         "selectAll. Then they shouldn't be present"
         "after it.";

  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(end_of_text)
                               .SetIsHandleVisible(true)
                               .Build());
  EXPECT_TRUE(Selection().IsHandleVisible());
  Selection().SelectAll();
  EXPECT_TRUE(Selection().IsHandleVisible())
      << "If handles were present before"
         "selectAll. Then they should be present"
         "after it.";
}

TEST_F(FrameSelectionTest, SetSelectedRangePreservesHandle) {
  Text* text = AppendTextNode("Hello, World!");
  GetDocument().View()->UpdateAllLifecyclePhases();
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 5))
          .SetIsHandleVisible(false)
          .Build());

  Selection().SetSelectedRange(
      EphemeralRange(Position(text, 0), Position(text, 12)),
      VP_DEFAULT_AFFINITY, SelectionDirectionalMode::kNonDirectional, 0);

  EXPECT_FALSE(Selection().IsHandleVisible())
      << "If handles weren't present before"
         "setSelectedRange they shouldn't be present"
         "after it.";

  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 5))
          .SetIsHandleVisible(true)
          .Build());

  Selection().SetSelectedRange(
      EphemeralRange(Position(text, 0), Position(text, 12)),
      VP_DEFAULT_AFFINITY, SelectionDirectionalMode::kNonDirectional, 0);

  EXPECT_TRUE(Selection().IsHandleVisible())
      << "If handles were present before"
         "selectSetSelectedRange they should be present after it.";
}

// Regression test for crbug.com/702756
// Test case excerpted from editing/undo/redo_correct_selection.html
TEST_F(FrameSelectionTest, SelectInvalidPositionInFlatTreeDoesntCrash) {
  SetBodyContent("foo<option><select></select></option>");
  Element* body = GetDocument().body();
  Element* select = GetDocument().QuerySelector("select");
  Node* foo = body->firstChild();
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(body, 0))
                               // SELECT@AfterAnchor is invalid in flat tree.
                               .Extend(Position::AfterNode(select))
                               .Build());
  // Should not crash inside.
  const VisibleSelectionInFlatTree& selection =
      Selection().ComputeVisibleSelectionInFlatTree();

  // This only records the current behavior. It might be changed in the future.
  EXPECT_EQ(PositionInFlatTree(foo, 0), selection.Base());
  EXPECT_EQ(PositionInFlatTree(foo, 0), selection.Extent());
}

}  // namespace blink
