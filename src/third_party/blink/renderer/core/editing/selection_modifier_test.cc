// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/selection_modifier.h"

#include "third_party/blink/renderer/core/editing/editing_behavior.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class SelectionModifierTest : public EditingTestBase {
 protected:
  std::string MoveBackwardByLine(SelectionModifier& modifier) {
    modifier.Modify(SelectionModifyAlteration::kMove,
                    SelectionModifyDirection::kBackward,
                    TextGranularity::kLine);
    return GetSelectionTextFromBody(modifier.Selection().AsSelection());
  }

  std::string MoveForwardByLine(SelectionModifier& modifier) {
    modifier.Modify(SelectionModifyAlteration::kMove,
                    SelectionModifyDirection::kForward, TextGranularity::kLine);
    return GetSelectionTextFromBody(modifier.Selection().AsSelection());
  }
};

TEST_F(SelectionModifierTest, ExtendForwardByWordNone) {
  SetBodyContent("abc");
  SelectionModifier modifier(GetFrame(), SelectionInDOMTree());
  modifier.Modify(SelectionModifyAlteration::kExtend,
                  SelectionModifyDirection::kForward, TextGranularity::kWord);
  // We should not crash here. See http://crbug.com/832061
  EXPECT_EQ(SelectionInDOMTree(), modifier.Selection().AsSelection());
}

TEST_F(SelectionModifierTest, MoveForwardByWordNone) {
  SetBodyContent("abc");
  SelectionModifier modifier(GetFrame(), SelectionInDOMTree());
  modifier.Modify(SelectionModifyAlteration::kMove,
                  SelectionModifyDirection::kForward, TextGranularity::kWord);
  // We should not crash here. See http://crbug.com/832061
  EXPECT_EQ(SelectionInDOMTree(), modifier.Selection().AsSelection());
}

TEST_F(SelectionModifierTest, MoveByLineHorizontal) {
  LoadAhem();
  InsertStyleElement(
      "p {"
      "font: 10px/20px Ahem;"
      "padding: 10px;"
      "writing-mode: horizontal-tb;"
      "}");
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("<p>ab|c<br>d<br><br>ghi</p>");
  SelectionModifier modifier(GetFrame(), selection);

  EXPECT_EQ("<p>abc<br>d|<br><br>ghi</p>", MoveForwardByLine(modifier));
  EXPECT_EQ("<p>abc<br>d<br>|<br>ghi</p>", MoveForwardByLine(modifier));
  EXPECT_EQ("<p>abc<br>d<br><br>gh|i</p>", MoveForwardByLine(modifier));

  EXPECT_EQ("<p>abc<br>d<br>|<br>ghi</p>", MoveBackwardByLine(modifier));
  EXPECT_EQ("<p>abc<br>d|<br><br>ghi</p>", MoveBackwardByLine(modifier));
  EXPECT_EQ("<p>ab|c<br>d<br><br>ghi</p>", MoveBackwardByLine(modifier));
}

TEST_F(SelectionModifierTest, MoveByLineVertical) {
  LoadAhem();
  InsertStyleElement(
      "p {"
      "font: 10px/20px Ahem;"
      "padding: 10px;"
      "writing-mode: vertical-rl;"
      "}");
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("<p>ab|c<br>d<br><br>ghi</p>");
  SelectionModifier modifier(GetFrame(), selection);

  EXPECT_EQ("<p>abc<br>d|<br><br>ghi</p>", MoveForwardByLine(modifier));
  EXPECT_EQ("<p>abc<br>d<br>|<br>ghi</p>", MoveForwardByLine(modifier));
  EXPECT_EQ("<p>abc<br>d<br><br>gh|i</p>", MoveForwardByLine(modifier));

  EXPECT_EQ("<p>abc<br>d<br>|<br>ghi</p>", MoveBackwardByLine(modifier));
  EXPECT_EQ("<p>abc<br>d|<br><br>ghi</p>", MoveBackwardByLine(modifier));
  EXPECT_EQ("<p>ab|c<br>d<br><br>ghi</p>", MoveBackwardByLine(modifier));
}

TEST_F(SelectionModifierTest, PreviousLineWithDisplayNone) {
  InsertStyleElement("body{font-family: monospace}");
  const SelectionInDOMTree selection = SetSelectionTextToBody(
      "<div contenteditable>"
      "<div>foo bar</div>"
      "<div>foo <b style=\"display:none\">qux</b> bar baz|</div>"
      "</div>");
  SelectionModifier modifier(GetFrame(), selection);
  modifier.Modify(SelectionModifyAlteration::kMove,
                  SelectionModifyDirection::kBackward, TextGranularity::kLine);
  EXPECT_EQ(
      "<div contenteditable>"
      "<div>foo bar|</div>"
      "<div>foo <b style=\"display:none\">qux</b> bar baz</div>"
      "</div>",
      GetSelectionTextFromBody(modifier.Selection().AsSelection()));
}

// For http://crbug.com/1104582
TEST_F(SelectionModifierTest, PreviousSentenceWithNull) {
  InsertStyleElement("b {display:inline-block}");
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("<b><ruby><a>|</a></ruby></b>");
  SelectionModifier modifier(GetFrame(), selection);
  // We call |PreviousSentence()| with null-position.
  EXPECT_FALSE(modifier.Modify(SelectionModifyAlteration::kMove,
                               SelectionModifyDirection::kBackward,
                               TextGranularity::kSentence));
}

// For http://crbug.com/1100971
TEST_F(SelectionModifierTest, StartOfSentenceWithNull) {
  InsertStyleElement("b {display:inline-block}");
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("|<b><ruby><a></a></ruby></b>");
  SelectionModifier modifier(GetFrame(), selection);
  // We call |StartOfSentence()| with null-position.
  EXPECT_FALSE(modifier.Modify(SelectionModifyAlteration::kMove,
                               SelectionModifyDirection::kBackward,
                               TextGranularity::kSentenceBoundary));
}

TEST_F(SelectionModifierTest, MoveCaretWithShadow) {
  const char* body_content =
      "a a"
      "<div id='host'>"
      "<span slot='e'>e e</span>"
      "<span slot='c'>c c</span>"
      "</div>"
      "f f";
  const char* shadow_content =
      "b b"
      "<slot name='c'></slot>"
      "d d"
      "<slot name='e'></slot>";
  LoadAhem();
  InsertStyleElement("body {font-family: Ahem}");
  SetBodyContent(body_content);
  Element* host = GetDocument().getElementById("host");
  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML(shadow_content);
  UpdateAllLifecyclePhasesForTest();

  Element* body = GetDocument().body();
  Node* a = body->childNodes()->item(0);
  Node* b = shadow_root.childNodes()->item(0);
  Node* c = host->QuerySelector("[slot=c]")->firstChild();
  Node* d = shadow_root.childNodes()->item(2);
  Node* e = host->QuerySelector("[slot=e]")->firstChild();
  Node* f = body->childNodes()->item(2);

  auto makeSelection = [&](Position position) {
    return SelectionInDOMTree::Builder().Collapse(position).Build();
  };
  SelectionModifyAlteration move = SelectionModifyAlteration::kMove;
  SelectionModifyDirection direction;
  TextGranularity granularity;

  {
    // Test moving forward, character by character.
    direction = SelectionModifyDirection::kForward;
    granularity = TextGranularity::kCharacter;
    SelectionModifier modifier(GetFrame(), makeSelection(Position(body, 0)));
    EXPECT_EQ(Position(a, 0), modifier.Selection().Base());
    for (Node* node : {a, b, c, d, e, f}) {
      if (node == b || node == f) {
        modifier.Modify(move, direction, granularity);
        EXPECT_EQ(Position(node, 0), modifier.Selection().Base());
      }
      modifier.Modify(move, direction, granularity);
      EXPECT_EQ(Position(node, 1), modifier.Selection().Base());
      modifier.Modify(move, direction, granularity);
      EXPECT_EQ(Position(node, 2), modifier.Selection().Base());
      modifier.Modify(move, direction, granularity);
      EXPECT_EQ(Position(node, 3), modifier.Selection().Base());
    }
  }
  {
    // Test moving backward, character by character.
    direction = SelectionModifyDirection::kBackward;
    granularity = TextGranularity::kCharacter;
    SelectionModifier modifier(GetFrame(), makeSelection(Position(body, 3)));
    for (Node* node : {f, e, d, c, b, a}) {
      EXPECT_EQ(Position(node, 3), modifier.Selection().Base());
      modifier.Modify(move, direction, granularity);
      EXPECT_EQ(Position(node, 2), modifier.Selection().Base());
      modifier.Modify(move, direction, granularity);
      EXPECT_EQ(Position(node, 1), modifier.Selection().Base());
      modifier.Modify(move, direction, granularity);
      if (node == f || node == b) {
        EXPECT_EQ(Position(node, 0), modifier.Selection().Base());
        modifier.Modify(move, direction, granularity);
      }
    }
    EXPECT_EQ(Position(a, 0), modifier.Selection().Base());
  }
  {
    // Test moving forward, word by word.
    direction = SelectionModifyDirection::kForward;
    granularity = TextGranularity::kWord;
    bool skip_space =
        GetFrame().GetEditor().Behavior().ShouldSkipSpaceWhenMovingRight();
    SelectionModifier modifier(GetFrame(), makeSelection(Position(body, 0)));
    EXPECT_EQ(Position(a, 0), modifier.Selection().Base());
    for (Node* node : {a, b, c, d, e, f}) {
      if (node == b || node == f) {
        modifier.Modify(move, direction, granularity);
        EXPECT_EQ(Position(node, 0), modifier.Selection().Base());
      }
      modifier.Modify(move, direction, granularity);
      EXPECT_EQ(Position(node, skip_space ? 2 : 1),
                modifier.Selection().Base());
      if (node == a || node == e || node == f) {
        modifier.Modify(move, direction, granularity);
        EXPECT_EQ(Position(node, 3), modifier.Selection().Base());
      }
    }
  }
  {
    // Test moving backward, word by word.
    direction = SelectionModifyDirection::kBackward;
    granularity = TextGranularity::kWord;
    SelectionModifier modifier(GetFrame(), makeSelection(Position(body, 3)));
    for (Node* node : {f, e, d, c, b, a}) {
      if (node == f || node == e || node == a) {
        EXPECT_EQ(Position(node, 3), modifier.Selection().Base());
        modifier.Modify(move, direction, granularity);
      }
      EXPECT_EQ(Position(node, 2), modifier.Selection().Base());
      modifier.Modify(move, direction, granularity);
      if (node == f || node == b) {
        EXPECT_EQ(Position(node, 0), modifier.Selection().Base());
        modifier.Modify(move, direction, granularity);
      }
    }
    EXPECT_EQ(Position(a, 0), modifier.Selection().Base());
  }

  // Place the contents into different lines
  InsertStyleElement("span {display: block}");
  UpdateAllLifecyclePhasesForTest();

  {
    // Test moving forward, line by line.
    direction = SelectionModifyDirection::kForward;
    granularity = TextGranularity::kLine;
    for (int i = 0; i <= 3; ++i) {
      SelectionModifier modifier(GetFrame(), makeSelection(Position(a, i)));
      for (Node* node : {a, b, c, d, e, f}) {
        EXPECT_EQ(Position(node, i), modifier.Selection().Base());
        modifier.Modify(move, direction, granularity);
      }
      EXPECT_EQ(Position(f, 3), modifier.Selection().Base());
    }
  }
  {
    // Test moving backward, line by line.
    direction = SelectionModifyDirection::kBackward;
    granularity = TextGranularity::kLine;
    for (int i = 0; i <= 3; ++i) {
      SelectionModifier modifier(GetFrame(), makeSelection(Position(f, i)));
      for (Node* node : {f, e, d, c, b, a}) {
        EXPECT_EQ(Position(node, i), modifier.Selection().Base());
        modifier.Modify(move, direction, granularity);
      }
      EXPECT_EQ(Position(a, 0), modifier.Selection().Base());
    }
  }
}

// For https://crbug.com/1155342 and https://crbug.com/1155309
TEST_F(SelectionModifierTest, PreviousParagraphOfObject) {
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("<object>|</object>");
  SelectionModifier modifier(GetFrame(), selection);
  modifier.Modify(SelectionModifyAlteration::kMove,
                  SelectionModifyDirection::kBackward,
                  TextGranularity::kParagraph);
  EXPECT_EQ("|<object></object>",
            GetSelectionTextFromBody(modifier.Selection().AsSelection()));
}

// For https://crbug.com/1177295
TEST_F(SelectionModifierTest, PositionDisconnectedInFlatTree1) {
  const SelectionInDOMTree selection = SetSelectionTextToBody(
      "<div id=a><div id=b><div id=c>^x|</div></div></div>");
  SetShadowContent("", "a");
  SetShadowContent("", "b");
  SetShadowContent("", "c");
  SelectionModifier modifier(GetFrame(), selection);
  modifier.Modify(SelectionModifyAlteration::kMove,
                  SelectionModifyDirection::kBackward,
                  TextGranularity::kParagraph);
  EXPECT_EQ("<div id=\"a\"><div id=\"b\"><div id=\"c\">x</div></div></div>",
            GetSelectionTextFromBody(modifier.Selection().AsSelection()));
}

// For https://crbug.com/1177295
TEST_F(SelectionModifierTest, PositionDisconnectedInFlatTree2) {
  SetBodyContent("<div id=host>x</div>y");
  SetShadowContent("", "host");
  Element* host = GetElementById("host");
  Node* text = host->firstChild();
  Position positions[] = {
      Position::BeforeNode(*host),         Position::FirstPositionInNode(*host),
      Position::LastPositionInNode(*host), Position::AfterNode(*host),
      Position::BeforeNode(*text),         Position::FirstPositionInNode(*text),
      Position::LastPositionInNode(*text), Position::AfterNode(*text)};
  for (const Position& base : positions) {
    EXPECT_TRUE(base.IsConnected());
    bool flat_base_is_connected = ToPositionInFlatTree(base).IsConnected();
    EXPECT_EQ(base.AnchorNode() == host, flat_base_is_connected);
    for (const Position& extent : positions) {
      const SelectionInDOMTree& selection =
          SelectionInDOMTree::Builder().SetBaseAndExtent(base, extent).Build();
      Selection().SetSelection(selection, SetSelectionOptions());
      SelectionModifier modifier(GetFrame(), selection);
      modifier.Modify(SelectionModifyAlteration::kExtend,
                      SelectionModifyDirection::kForward,
                      TextGranularity::kParagraph);
      bool flat_extent_is_connected =
          ToPositionInFlatTree(selection.Extent()).IsConnected();
      EXPECT_EQ(flat_base_is_connected && flat_extent_is_connected
                    ? "<div id=\"host\">x</div>^y|"
                    : "<div id=\"host\">x</div>y",
                GetSelectionTextFromBody(modifier.Selection().AsSelection()));
    }
  }
}

}  // namespace blink
