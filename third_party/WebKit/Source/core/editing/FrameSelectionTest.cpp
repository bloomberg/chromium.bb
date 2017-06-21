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
#include "core/frame/LocalFrameView.h"
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
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SetBaseAndExtent(EphemeralRange(
                                   Position(text, 3), Position(text, 6)))
                               .Build(),
                           0);
  sample->setAttribute(HTMLNames::styleAttr, "display:none");
  // Move |VisibleSelection| before "abc".
  UpdateAllLifecyclePhases();
  const EphemeralRange& range =
      FirstEphemeralRangeOf(Selection().ComputeVisibleSelectionInDOMTree());
  EXPECT_EQ(Position(sample->nextSibling(), 0), range.StartPosition())
      << "firstRange() should return current selection value";
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
    LocalFrameView& frame_view = GetDummyPageHolder().GetFrameView();
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
  EXPECT_EQ(Position(two, 3), VisibleSelectionInDOMTree().End());
  EXPECT_EQ(PositionInFlatTree(two, 0),
            GetVisibleSelectionInFlatTree().Start());
  EXPECT_EQ(PositionInFlatTree(two, 3), GetVisibleSelectionInFlatTree().End());
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
                .Collapse(Position::BeforeNode(*input))
                .Extend(Position(last_child, 3))
                .Build(),
            result_in_dom_tree);
  EXPECT_EQ(SelectionInFlatTree::Builder(result_in_flat_tree)
                .Collapse(PositionInFlatTree::BeforeNode(*input))
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
      << "If handles weren't present before "
         "selectAll. Then they shouldn't be present "
         "after it.";

  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(end_of_text)
                               .SetIsHandleVisible(true)
                               .Build());
  EXPECT_TRUE(Selection().IsHandleVisible());
  Selection().SelectAll();
  EXPECT_TRUE(Selection().IsHandleVisible())
      << "If handles were present before "
         "selectAll. Then they should be present "
         "after it.";
}

TEST_F(FrameSelectionTest, BoldCommandPreservesHandle) {
  SetBodyContent("<div id=sample>abc</div>");
  Element* sample = GetDocument().getElementById("sample");
  const Position end_of_text(sample->firstChild(), 3);
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(end_of_text)
                               .SetIsHandleVisible(false)
                               .Build());
  EXPECT_FALSE(Selection().IsHandleVisible());
  Selection().SelectAll();
  GetDocument().execCommand("bold", false, "", ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(Selection().IsHandleVisible())
      << "If handles weren't present before "
         "bold command. Then they shouldn't "
         "be present after it.";

  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(end_of_text)
                               .SetIsHandleVisible(true)
                               .Build());
  EXPECT_TRUE(Selection().IsHandleVisible());
  Selection().SelectAll();
  GetDocument().execCommand("bold", false, "", ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(Selection().IsHandleVisible())
      << "If handles were present before "
         "bold command. Then they should "
         "be present after it.";
}

TEST_F(FrameSelectionTest, SelectionOnRangeHidesHandles) {
  Text* text = AppendTextNode("Hello, World!");
  GetDocument().View()->UpdateAllLifecyclePhases();
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 5))
          .SetIsHandleVisible(false)
          .Build());

  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SetBaseAndExtent(EphemeralRange(
                                   Position(text, 0), Position(text, 12)))
                               .Build(),
                           0);

  EXPECT_FALSE(Selection().IsHandleVisible())
      << "After SetSelection on Range, handles shouldn't be present.";

  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 5))
          .SetIsHandleVisible(true)
          .Build());

  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SetBaseAndExtent(EphemeralRange(
                                   Position(text, 0), Position(text, 12)))
                               .Build(),
                           0);

  EXPECT_FALSE(Selection().IsHandleVisible())
      << "After SetSelection on Range, handles shouldn't be present.";
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

TEST_F(FrameSelectionTest, CaretInShadowTree) {
  SetBodyContent("<p id=host></p>bar");
  ShadowRoot* shadow_root =
      SetShadowContent("<div contenteditable id='ce'>foo</div>", "host");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const ce = shadow_root->getElementById("ce");
  ce->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  ce->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Caret is now hidden.
}

TEST_F(FrameSelectionTest, CaretInTextControl) {
  SetBodyContent("<input id='field'>");  // <input> hosts a shadow tree.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const field = GetDocument().getElementById("field");
  field->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  field->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Caret is now hidden.
}

TEST_F(FrameSelectionTest, RangeInShadowTree) {
  SetBodyContent("<p id='host'></p>");
  ShadowRoot* shadow_root = SetShadowContent("hey", "host");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Node* text_node = shadow_root->firstChild();
  Selection().SetSelection(
      SelectionInFlatTree::Builder()
          .SetBaseAndExtent(PositionInFlatTree(text_node, 0),
                            PositionInFlatTree(text_node, 3))
          .Build());
  EXPECT_EQ_SELECTED_TEXT("hey");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  GetDocument().body()->focus();  // Move focus to document body.
  EXPECT_EQ_SELECTED_TEXT("hey");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());
}

TEST_F(FrameSelectionTest, RangeInTextControl) {
  SetBodyContent("<input id='field' value='hola'>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const field = GetDocument().getElementById("field");
  field->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Selection().SelectAll();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  field->blur();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());
}

// crbug.com/692898
TEST_F(FrameSelectionTest, FocusingLinkHidesCaretInTextControl) {
  SetBodyContent(
      "<input id='field'>"
      "<a href='www' id='alink'>link</a>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const field = GetDocument().getElementById("field");
  field->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Element* const alink = GetDocument().getElementById("alink");
  alink->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());
}

// crbug.com/692898
TEST_F(FrameSelectionTest, FocusingLinkHidesRangeInTextControl) {
  SetBodyContent(
      "<input id='field' value='hola'>"
      "<a href='www' id='alink'>link</a>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const field = GetDocument().getElementById("field");
  field->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Selection().SelectAll();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Element* const alink = GetDocument().getElementById("alink");
  alink->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());
}

TEST_F(FrameSelectionTest, FocusingButtonHidesRangeInReadOnlyTextControl) {
  SetBodyContent(
      "<textarea readonly>Berlin</textarea>"
      "<input type='submit' value='Submit'>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const textarea = GetDocument().QuerySelector("textarea");
  textarea->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());

  Selection().SelectAll();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Element* const submit = GetDocument().QuerySelector("input");
  submit->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());
}

TEST_F(FrameSelectionTest, FocusingButtonHidesRangeInDisabledTextControl) {
  SetBodyContent(
      "<textarea disabled>Berlin</textarea>"
      "<input type='submit' value='Submit'>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const textarea = GetDocument().QuerySelector("textarea");
  textarea->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());

  // We use a double click to create the selection [Berlin].
  // FrameSelection::SelectAll (= textarea.select() in JavaScript) would have
  // been shorter, but currently that doesn't work on a *disabled* text control.
  const IntRect elem_bounds = textarea->BoundsInViewport();
  WebMouseEvent double_click(WebMouseEvent::kMouseDown, 0,
                             WebInputEvent::kTimeStampForTesting);
  double_click.SetPositionInWidget(elem_bounds.X(), elem_bounds.Y());
  double_click.SetPositionInScreen(elem_bounds.X(), elem_bounds.Y());
  double_click.button = WebMouseEvent::Button::kLeft;
  double_click.click_count = 2;
  double_click.SetFrameScale(1);

  GetFrame().GetEventHandler().HandleMousePressEvent(double_click);
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Element* const submit = GetDocument().QuerySelector("input");
  submit->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());
}

// crbug.com/713051
TEST_F(FrameSelectionTest, FocusingNonEditableParentHidesCaretInTextControl) {
  SetBodyContent(
      "<div tabindex='-1' id='parent'>"
      "  <input id='field'>"
      "</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const field = GetDocument().getElementById("field");
  field->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  // Here the selection belongs to <input>'s shadow tree and that tree has a
  // non-editable parent that is focused.
  Element* const parent = GetDocument().getElementById("parent");
  parent->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Focus is outside <input>
                                        // so caret should not be visible.

  parent->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Caret is still hidden.
}

// crbug.com/713051
TEST_F(FrameSelectionTest, FocusingNonEditableParentHidesRangeInTextControl) {
  SetBodyContent(
      "<div tabindex='-1' id='parent'>"
      "  <input id='field' value='hola'>"
      "</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const field = GetDocument().getElementById("field");
  field->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Selection().SelectAll();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  // Here the selection belongs to <input>'s shadow tree and that tree has a
  // non-editable parent that is focused.
  Element* const parent = GetDocument().getElementById("parent");
  parent->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Focus is outside <input>
                                        // so range should not be visible.

  parent->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Range is still hidden.
}

TEST_F(FrameSelectionTest, CaretInEditableDiv) {
  SetBodyContent("<div contenteditable id='ce'>blabla</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const ce = GetDocument().getElementById("ce");
  ce->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  ce->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Caret is now hidden.
}

TEST_F(FrameSelectionTest, RangeInEditableDiv) {
  SetBodyContent("<div contenteditable id='ce'>blabla</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const ce = GetDocument().getElementById("ce");
  ce->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Selection().SelectAll();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  ce->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());  // Range is still visible.
}

TEST_F(FrameSelectionTest, RangeInEditableDivInShadowTree) {
  SetBodyContent("<p id='host'></p>");
  ShadowRoot* shadow_root =
      SetShadowContent("<div id='ce' contenteditable>foo</div>", "host");

  Element* const ce = shadow_root->getElementById("ce");
  ce->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Selection().SelectAll();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  ce->blur();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());  // Range is still visible.
}

TEST_F(FrameSelectionTest, FocusingLinkHidesCaretInContentEditable) {
  SetBodyContent(
      "<div contenteditable id='ce'>blabla</div>"
      "<a href='www' id='alink'>link</a>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const ce = GetDocument().getElementById("ce");
  ce->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Element* const alink = GetDocument().getElementById("alink");
  alink->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());
}

TEST_F(FrameSelectionTest, FocusingLinkKeepsRangeInContentEditable) {
  SetBodyContent(
      "<div contenteditable id='ce'>blabla</div>"
      "<a href='www' id='alink'>link</a>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const ce = GetDocument().getElementById("ce");
  ce->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Selection().SelectAll();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Element* const alink = GetDocument().getElementById("alink");
  alink->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());
}

TEST_F(FrameSelectionTest, FocusingEditableParentKeepsEditableCaret) {
  SetBodyContent(
      "<div contenteditable tabindex='-1' id='parent'>"
      "<div contenteditable id='ce'>blabla</div>"
      "</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  // TODO(editing-dev): Blink should be able to focus the inner <div>.
  //  Element* const ce = GetDocument().getElementById("ce");
  //  ce->focus();
  //  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  //  EXPECT_FALSE(Selection().IsHidden());

  Element* const parent = GetDocument().getElementById("parent");
  parent->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());  // Focus is within editing boundary,
                                         // caret should be visible.

  parent->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Focus is outside editing boundary
                                        // so caret should be hidden.
}

TEST_F(FrameSelectionTest, FocusingEditableParentKeepsEditableRange) {
  SetBodyContent(
      "<div contenteditable tabindex='-1' id='parent'>"
      "<div contenteditable id='ce'>blabla</div>"
      "</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  // TODO(editing-dev): Blink should be able to focus the inner <div>.
  //  Element* const ce = GetDocument().getElementById("ce");
  //  ce->focus();
  //  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  //  EXPECT_FALSE(Selection().IsHidden());

  //  Selection().SelectAll();
  //  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  //  EXPECT_FALSE(Selection().IsHidden());

  Element* const parent = GetDocument().getElementById("parent");
  parent->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());  // Focus is within editing boundary,
                                         // range should be visible.

  Selection().SelectAll();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  parent->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());  // Focus is outside editing boundary
                                         // but range should still be visible.
}

TEST_F(FrameSelectionTest, FocusingNonEditableParentHidesEditableCaret) {
  SetBodyContent(
      "<div tabindex='-1' id='parent'>"
      "<div contenteditable id='ce'>blabla</div>"
      "</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const ce = GetDocument().getElementById("ce");
  ce->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  // Here the selection belongs to <div>'s shadow tree and that tree has a
  // non-editable parent that is focused.
  Element* const parent = GetDocument().getElementById("parent");
  parent->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Focus is outside editing boundary
                                        // so caret should be hidden.

  parent->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Caret is still hidden.
}

TEST_F(FrameSelectionTest, FocusingNonEditableParentKeepsEditableRange) {
  SetBodyContent(
      "<div tabindex='-1' id='parent'>"
      "<div contenteditable id='ce'>blabla</div>"
      "</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const ce = GetDocument().getElementById("ce");
  ce->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Selection().SelectAll();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  // Here the selection belongs to <div>'s shadow tree and that tree has a
  // non-editable parent that is focused.
  Element* const parent = GetDocument().getElementById("parent");
  parent->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());  // Focus is outside editing boundary
                                         // but range should still be visible.

  parent->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());  // Range is still visible.
}

// crbug.com/707143
TEST_F(FrameSelectionTest, RangeContainsFocus) {
  SetBodyContent(
      "<div>"
      "  <div>"
      "    <span id='start'>start</span>"
      "  </div>"
      "  <a href='www' id='alink'>link</a>"
      "  <div>line 1</div>"
      "  <div>line 2</div>"
      "  <div>line 3</div>"
      "  <div>line 4</div>"
      "  <span id='end'>end</span>"
      "  <div></div>"
      "</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const start = GetDocument().getElementById("start");
  Element* const end = GetDocument().getElementById("end");
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(start, 0), Position(end, 3))
          .Build());
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Element* const alink = GetDocument().getElementById("alink");
  alink->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());  // Range still visible.
}

// crbug.com/707143
TEST_F(FrameSelectionTest, RangeOutsideFocus) {
  // Here the selection sits on a sub tree that hasn't the focused element.
  // This test case is the reason why we separate FrameSelection::HasFocus() and
  // FrameSelection::IsHidden(). Even when the selection's DOM nodes are
  // completely disconnected from the focused node, we still want the selection
  // to be visible (not hidden).
  SetBodyContent(
      "<a href='www' id='alink'>link</a>"
      "<div>"
      "  <div>"
      "    <span id='start'>start</span>"
      "  </div>"
      "  <div>line 1</div>"
      "  <div>line 2</div>"
      "  <div>line 3</div>"
      "  <div>line 4</div>"
      "  <span id='end'>end</span>"
      "  <div></div>"
      "</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const start = GetDocument().getElementById("start");
  Element* const end = GetDocument().getElementById("end");
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(start, 0), Position(end, 3))
          .Build());
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Element* const alink = GetDocument().getElementById("alink");
  alink->focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());  // Range still visible.
}

// crbug.com/725457
TEST_F(FrameSelectionTest, InconsistentVisibleSelectionNoCrash) {
  SetBodyContent("foo<div id=host><span id=anchor>bar</span></div>baz");
  SetShadowContent("shadow", "host");

  Element* anchor = GetDocument().getElementById("anchor");

  // |start| and |end| are valid Positions in DOM tree, but do not participate
  // in flat tree. They should be canonicalized to null VisiblePositions, but
  // are currently not. See crbug.com/729636 for details.
  const Position& start = Position::BeforeNode(*anchor);
  const Position& end = Position::AfterNode(anchor);
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(start).Extend(end).Build());

  // Shouldn't crash inside.
  EXPECT_FALSE(Selection().SelectionHasFocus());
}

}  // namespace blink
