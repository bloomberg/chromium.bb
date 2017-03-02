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
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/StdLibExtras.h"

namespace blink {

class FrameSelectionTest : public EditingTestBase {
 protected:
  const VisibleSelection& visibleSelectionInDOMTree() const {
    return selection().computeVisibleSelectionInDOMTreeDeprecated();
  }
  const VisibleSelectionInFlatTree& visibleSelectionInFlatTree() const {
    return selection().selectionInFlatTree();
  }

  Text* appendTextNode(const String& data);
  int layoutCount() const {
    return dummyPageHolder().frameView().layoutCount();
  }

  PositionWithAffinity caretPosition() const {
    return selection().m_frameCaret->caretPosition();
  }

 private:
  Persistent<Text> m_textNode;
};

Text* FrameSelectionTest::appendTextNode(const String& data) {
  Text* text = document().createTextNode(data);
  document().body()->appendChild(text);
  return text;
}

TEST_F(FrameSelectionTest, SetValidSelection) {
  Text* text = appendTextNode("Hello, World!");
  document().view()->updateAllLifecyclePhases();
  selection().setSelection(
      SelectionInDOMTree::Builder()
          .setBaseAndExtent(Position(text, 0), Position(text, 5))
          .build());
  EXPECT_FALSE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isNone());
}

TEST_F(FrameSelectionTest, PaintCaretShouldNotLayout) {
  Text* text = appendTextNode("Hello, World!");
  document().view()->updateAllLifecyclePhases();

  document().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  document().body()->focus();
  EXPECT_TRUE(document().body()->isFocused());

  selection().setCaretVisible(true);
  selection().setSelection(
      SelectionInDOMTree::Builder().collapse(Position(text, 0)).build());
  document().view()->updateAllLifecyclePhases();
  EXPECT_TRUE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isCaret());
  EXPECT_TRUE(toLayoutBlock(document().body()->layoutObject())
                  ->shouldPaintCursorCaret());

  int startCount = layoutCount();
  {
    // To force layout in next updateLayout calling, widen view.
    FrameView& frameView = dummyPageHolder().frameView();
    IntRect frameRect = frameView.frameRect();
    frameRect.setWidth(frameRect.width() + 1);
    frameRect.setHeight(frameRect.height() + 1);
    dummyPageHolder().frameView().setFrameRect(frameRect);
  }
  std::unique_ptr<PaintController> paintController = PaintController::create();
  {
    GraphicsContext context(*paintController);
    selection().paintCaret(context, LayoutPoint());
  }
  paintController->commitNewDisplayItems();
  EXPECT_EQ(startCount, layoutCount());
}

#define EXPECT_EQ_SELECTED_TEXT(text) \
  EXPECT_EQ(text, WebString(selection().selectedText()).utf8())

TEST_F(FrameSelectionTest, SelectWordAroundPosition) {
  // "Foo Bar  Baz,"
  Text* text = appendTextNode("Foo Bar&nbsp;&nbsp;Baz,");
  updateAllLifecyclePhases();

  // "Fo|o Bar  Baz,"
  EXPECT_TRUE(selection().selectWordAroundPosition(
      createVisiblePosition(Position(text, 2))));
  EXPECT_EQ_SELECTED_TEXT("Foo");
  // "Foo| Bar  Baz,"
  EXPECT_TRUE(selection().selectWordAroundPosition(
      createVisiblePosition(Position(text, 3))));
  EXPECT_EQ_SELECTED_TEXT("Foo");
  // "Foo Bar | Baz,"
  EXPECT_FALSE(selection().selectWordAroundPosition(
      createVisiblePosition(Position(text, 13))));
  // "Foo Bar  Baz|,"
  EXPECT_TRUE(selection().selectWordAroundPosition(
      createVisiblePosition(Position(text, 22))));
  EXPECT_EQ_SELECTED_TEXT("Baz");
}

// crbug.com/657996
TEST_F(FrameSelectionTest, SelectWordAroundPosition2) {
  setBodyContent(
      "<p style='width:70px; font-size:14px'>foo bar<em>+</em> baz</p>");
  // "foo bar
  //  b|az"
  Node* const baz = document().body()->firstChild()->lastChild();
  EXPECT_TRUE(selection().selectWordAroundPosition(
      createVisiblePosition(Position(baz, 2))));
  // TODO(yoichio): We should select only "baz".
  EXPECT_EQ_SELECTED_TEXT(" baz");
}

TEST_F(FrameSelectionTest, ModifyExtendWithFlatTree) {
  setBodyContent("<span id=host></span>one");
  setShadowContent("two<content></content>", "host");
  Element* host = document().getElementById("host");
  Node* const two = FlatTreeTraversal::firstChild(*host);
  // Select "two" for selection in DOM tree
  // Select "twoone" for selection in Flat tree
  selection().setSelection(SelectionInFlatTree::Builder()
                               .collapse(PositionInFlatTree(host, 0))
                               .extend(PositionInFlatTree(document().body(), 2))
                               .build());
  selection().modify(FrameSelection::AlterationExtend, DirectionForward,
                     WordGranularity);
  EXPECT_EQ(Position(two, 0), visibleSelectionInDOMTree().start());
  EXPECT_EQ(Position(two, 3), visibleSelectionInDOMTree().end());
  EXPECT_EQ(PositionInFlatTree(two, 0), visibleSelectionInFlatTree().start());
  EXPECT_EQ(PositionInFlatTree(two, 3), visibleSelectionInFlatTree().end());
}

TEST_F(FrameSelectionTest, ModifyWithUserTriggered) {
  setBodyContent("<div id=sample>abc</div>");
  Element* sample = document().getElementById("sample");
  const Position endOfText(sample->firstChild(), 3);
  selection().setSelection(
      SelectionInDOMTree::Builder().collapse(endOfText).build());

  EXPECT_FALSE(selection().modify(FrameSelection::AlterationMove,
                                  DirectionForward, CharacterGranularity,
                                  NotUserTriggered))
      << "Selection.modify() returns false for non-user-triggered call when "
         "selection isn't modified.";
  EXPECT_EQ(endOfText,
            selection().computeVisibleSelectionInDOMTreeDeprecated().start())
      << "Selection isn't modified";

  EXPECT_TRUE(selection().modify(FrameSelection::AlterationMove,
                                 DirectionForward, CharacterGranularity,
                                 UserTriggered))
      << "Selection.modify() returns true for user-triggered call";
  EXPECT_EQ(endOfText,
            selection().computeVisibleSelectionInDOMTreeDeprecated().start())
      << "Selection isn't modified";
}

TEST_F(FrameSelectionTest, MoveRangeSelectionTest) {
  // "Foo Bar Baz,"
  Text* text = appendTextNode("Foo Bar Baz,");
  updateAllLifecyclePhases();

  // Itinitializes with "Foo B|a>r Baz," (| means start and > means end).
  selection().setSelection(
      SelectionInDOMTree::Builder()
          .setBaseAndExtent(Position(text, 5), Position(text, 6))
          .build());
  EXPECT_EQ_SELECTED_TEXT("a");

  // "Foo B|ar B>az," with the Character granularity.
  selection().moveRangeSelection(createVisiblePosition(Position(text, 5)),
                                 createVisiblePosition(Position(text, 9)),
                                 CharacterGranularity);
  EXPECT_EQ_SELECTED_TEXT("ar B");
  // "Foo B|ar B>az," with the Word granularity.
  selection().moveRangeSelection(createVisiblePosition(Position(text, 5)),
                                 createVisiblePosition(Position(text, 9)),
                                 WordGranularity);
  EXPECT_EQ_SELECTED_TEXT("Bar Baz");
  // "Fo<o B|ar Baz," with the Character granularity.
  selection().moveRangeSelection(createVisiblePosition(Position(text, 5)),
                                 createVisiblePosition(Position(text, 2)),
                                 CharacterGranularity);
  EXPECT_EQ_SELECTED_TEXT("o B");
  // "Fo<o B|ar Baz," with the Word granularity.
  selection().moveRangeSelection(createVisiblePosition(Position(text, 5)),
                                 createVisiblePosition(Position(text, 2)),
                                 WordGranularity);
  EXPECT_EQ_SELECTED_TEXT("Foo Bar");
}

// For http://crbug.com/695317
TEST_F(FrameSelectionTest, SelectAllWithInputElement) {
  setBodyContent("<input>123");
  Element* const input = document().querySelector("input");
  Node* const lastChild = document().body()->lastChild();
  selection().selectAll();
  const SelectionInDOMTree& resultInDOMTree =
      selection().computeVisibleSelectionInDOMTree().asSelection();
  const SelectionInFlatTree& resultInFlatTree =
      selection().computeVisibleSelectionInFlatTree().asSelection();
  EXPECT_EQ(SelectionInDOMTree::Builder(resultInDOMTree)
                .collapse(Position::beforeNode(input))
                .extend(Position(lastChild, 3))
                .build(),
            resultInDOMTree);
  EXPECT_EQ(SelectionInFlatTree::Builder(resultInFlatTree)
                .collapse(PositionInFlatTree::beforeNode(input))
                .extend(PositionInFlatTree(lastChild, 3))
                .build(),
            resultInFlatTree);
}

TEST_F(FrameSelectionTest, SelectAllWithUnselectableRoot) {
  Element* select = document().createElement("select");
  document().replaceChild(select, document().documentElement());
  selection().selectAll();
  EXPECT_TRUE(selection().computeVisibleSelectionInDOMTreeDeprecated().isNone())
      << "Nothing should be selected if the "
         "content of the documentElement is not "
         "selctable.";
}

TEST_F(FrameSelectionTest, SelectAllPreservesHandle) {
  setBodyContent("<div id=sample>abc</div>");
  Element* sample = document().getElementById("sample");
  const Position endOfText(sample->firstChild(), 3);
  selection().setSelection(SelectionInDOMTree::Builder()
                               .collapse(endOfText)
                               .setIsHandleVisible(false)
                               .build());
  EXPECT_FALSE(selection().isHandleVisible());
  selection().selectAll();
  EXPECT_FALSE(selection().isHandleVisible())
      << "If handles weren't present before"
         "selectAll. Then they shouldn't be present"
         "after it.";

  selection().setSelection(SelectionInDOMTree::Builder()
                               .collapse(endOfText)
                               .setIsHandleVisible(true)
                               .build());
  EXPECT_TRUE(selection().isHandleVisible());
  selection().selectAll();
  EXPECT_TRUE(selection().isHandleVisible())
      << "If handles were present before"
         "selectAll. Then they should be present"
         "after it.";
}

}  // namespace blink
