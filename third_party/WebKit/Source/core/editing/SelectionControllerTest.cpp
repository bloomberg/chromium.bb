// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/SelectionController.h"

#include "core/editing/EditingTestBase.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/input/EventHandler.h"

namespace blink {

class SelectionControllerTest : public EditingTestBase {
 protected:
  SelectionControllerTest() = default;

  const VisibleSelection& VisibleSelectionInDOMTree() const {
    return Selection().ComputeVisibleSelectionInDOMTreeDeprecated();
  }

  const VisibleSelectionInFlatTree& GetVisibleSelectionInFlatTree() const {
    return Selection().GetSelectionInFlatTree();
  }

  void SetCaretAtHitTestResult(const HitTestResult&);
  void SetNonDirectionalSelectionIfNeeded(const SelectionInFlatTree&,
                                          TextGranularity);

 private:
  DISALLOW_COPY_AND_ASSIGN(SelectionControllerTest);
};

void SelectionControllerTest::SetCaretAtHitTestResult(
    const HitTestResult& hit_test_result) {
  GetFrame().GetEventHandler().GetSelectionController().SetCaretAtHitTestResult(
      hit_test_result);
}

void SelectionControllerTest::SetNonDirectionalSelectionIfNeeded(
    const SelectionInFlatTree& new_selection,
    TextGranularity granularity) {
  GetFrame()
      .GetEventHandler()
      .GetSelectionController()
      .SetNonDirectionalSelectionIfNeeded(
          new_selection, granularity,
          SelectionController::kDoNotAdjustEndpoints,
          HandleVisibility::kNotVisible);
}

TEST_F(SelectionControllerTest, setNonDirectionalSelectionIfNeeded) {
  const char* body_content = "<span id=top>top</span><span id=host></span>";
  const char* shadow_content = "<span id=bottom>bottom</span>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* top = GetDocument().getElementById("top")->firstChild();
  Node* bottom = shadow_root->getElementById("bottom")->firstChild();
  Node* host = GetDocument().getElementById("host");

  // top to bottom
  SetNonDirectionalSelectionIfNeeded(SelectionInFlatTree::Builder()
                                         .Collapse(PositionInFlatTree(top, 1))
                                         .Extend(PositionInFlatTree(bottom, 3))
                                         .Build(),
                                     kCharacterGranularity);
  EXPECT_EQ(Position(top, 1), VisibleSelectionInDOMTree().Base());
  EXPECT_EQ(Position::BeforeNode(*host), VisibleSelectionInDOMTree().Extent());
  EXPECT_EQ(Position(top, 1), VisibleSelectionInDOMTree().Start());
  EXPECT_EQ(Position(top, 3), VisibleSelectionInDOMTree().End());

  EXPECT_EQ(PositionInFlatTree(top, 1), GetVisibleSelectionInFlatTree().Base());
  EXPECT_EQ(PositionInFlatTree(bottom, 3),
            GetVisibleSelectionInFlatTree().Extent());
  EXPECT_EQ(PositionInFlatTree(top, 1),
            GetVisibleSelectionInFlatTree().Start());
  EXPECT_EQ(PositionInFlatTree(bottom, 3),
            GetVisibleSelectionInFlatTree().End());

  // bottom to top
  SetNonDirectionalSelectionIfNeeded(
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree(bottom, 3))
          .Extend(PositionInFlatTree(top, 1))
          .Build(),
      kCharacterGranularity);
  EXPECT_EQ(Position(bottom, 3), VisibleSelectionInDOMTree().Base());
  EXPECT_EQ(Position::BeforeNode(*bottom->parentNode()),
            VisibleSelectionInDOMTree().Extent());
  EXPECT_EQ(Position(bottom, 0), VisibleSelectionInDOMTree().Start());
  EXPECT_EQ(Position(bottom, 3), VisibleSelectionInDOMTree().End());

  EXPECT_EQ(PositionInFlatTree(bottom, 3),
            GetVisibleSelectionInFlatTree().Base());
  EXPECT_EQ(PositionInFlatTree(top, 1),
            GetVisibleSelectionInFlatTree().Extent());
  EXPECT_EQ(PositionInFlatTree(top, 1),
            GetVisibleSelectionInFlatTree().Start());
  EXPECT_EQ(PositionInFlatTree(bottom, 3),
            GetVisibleSelectionInFlatTree().End());
}

TEST_F(SelectionControllerTest, setCaretAtHitTestResult) {
  const char* body_content = "<div id='sample' contenteditable>sample</div>";
  SetBodyContent(body_content);
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().createElement("script");
  script->setInnerHTML(
      "var sample = document.getElementById('sample');"
      "sample.addEventListener('onselectstart', "
      "  event => elem.parentNode.removeChild(elem));");
  GetDocument().body()->AppendChild(script);
  GetDocument().View()->UpdateAllLifecyclePhases();
  GetFrame().GetEventHandler().GetSelectionController().HandleGestureLongPress(
      GetFrame().GetEventHandler().HitTestResultAtPoint(IntPoint(8, 8)));
}

// For http://crbug.com/704827
TEST_F(SelectionControllerTest, setCaretAtHitTestResultWithNullPosition) {
  SetBodyContent(
      "<style>"
      "#sample:before {content: '&nbsp;'}"
      "#sample { user-select: none; }"
      "</style>"
      "<div id=sample></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Hit "&nbsp;" in before pseudo element of "sample".
  SetCaretAtHitTestResult(
      GetFrame().GetEventHandler().HitTestResultAtPoint(IntPoint(10, 10)));

  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
}

}  // namespace blink
