// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_position.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"

namespace blink {

//
// Basic tests.
//

TEST_F(AccessibilityTest, PositionInText) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">Hello</p>)HTML");
  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionInTextObject(*ax_static_text, 3);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(3, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ObjectAfterPosition());
}

// To prevent surprises when comparing equality of two |AXPosition|s, position
// before text object should be the same as position in text object at offset 0.
TEST_F(AccessibilityTest, PositionBeforeText) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">Hello</p>)HTML");
  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionBeforeObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(0, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ObjectAfterPosition());
}

TEST_F(AccessibilityTest, PositionBeforeTextWithFirstLetterCSSRule) {
  SetBodyInnerHTML(
      R"HTML(<style>p ::first-letter { color: red; font-size: 200%; }</style>
      <p id="paragraph">Hello</p>)HTML");
  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionBeforeObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(0, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ObjectAfterPosition());
}

// To prevent surprises when comparing equality of two |AXPosition|s, position
// after text object should be the same as position in text object at offset
// text length.
TEST_F(AccessibilityTest, PositionAfterText) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">Hello</p>)HTML");
  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionAfterObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(5, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ObjectAfterPosition());
}

TEST_F(AccessibilityTest, PositionBeforeLineBreak) {
  SetBodyInnerHTML(R"HTML(Hello<br id="br">there)HTML");
  const AXObject* ax_br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, ax_br);
  ASSERT_EQ(AccessibilityRole::kLineBreakRole, ax_br->RoleValue());

  const auto ax_position = AXPosition::CreatePositionBeforeObject(*ax_br);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument().body(), position.AnchorNode());
  EXPECT_EQ(1, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_br, ax_position_from_dom.ObjectAfterPosition());
}

TEST_F(AccessibilityTest, PositionAfterLineBreak) {
  SetBodyInnerHTML(R"HTML(Hello<br id="br">there)HTML");
  const AXObject* ax_br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, ax_br);
  ASSERT_EQ(AccessibilityRole::kLineBreakRole, ax_br->RoleValue());
  const AXObject* ax_static_text = GetAXRootObject()->DeepestLastChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position = AXPosition::CreatePositionAfterObject(*ax_br);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument().body(), position.AnchorNode());
  EXPECT_EQ(2, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_static_text, ax_position_from_dom.ObjectAfterPosition());
}

TEST_F(AccessibilityTest, FirstPositionInDivContainer) {
  SetBodyInnerHTML(R"HTML(<div id="div">Hello<br>there</div>)HTML");
  const Element* div = GetElementById("div");
  ASSERT_NE(nullptr, div);
  const AXObject* ax_div = GetAXObjectByElementId("div");
  ASSERT_NE(nullptr, ax_div);
  ASSERT_EQ(AccessibilityRole::kGenericContainerRole, ax_div->RoleValue());
  const AXObject* ax_static_text = GetAXRootObject()->DeepestFirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position = AXPosition::CreateFirstPositionInObject(*ax_div);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(div, position.AnchorNode());
  EXPECT_EQ(0, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_static_text, ax_position_from_dom.ObjectAfterPosition());
}

TEST_F(AccessibilityTest, LastPositionInDivContainer) {
  SetBodyInnerHTML(R"HTML(<div id="div">Hello<br>there</div>
                   <div id="divNext">Next div</div>)HTML");
  const Element* div = GetElementById("div");
  ASSERT_NE(nullptr, div);
  const AXObject* ax_div = GetAXObjectByElementId("div");
  ASSERT_NE(nullptr, ax_div);
  ASSERT_EQ(AccessibilityRole::kGenericContainerRole, ax_div->RoleValue());
  const AXObject* ax_div_next = GetAXObjectByElementId("divNext");
  ASSERT_NE(nullptr, ax_div_next);
  ASSERT_EQ(AccessibilityRole::kGenericContainerRole, ax_div_next->RoleValue());

  const auto ax_position = AXPosition::CreateLastPositionInObject(*ax_div);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(div, position.AnchorNode());
  EXPECT_TRUE(position.GetPosition().IsAfterChildren());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_div_next, ax_position_from_dom.ObjectAfterPosition());
}

TEST_F(AccessibilityTest, FirstPositionInTextContainer) {
  SetBodyInnerHTML(R"HTML(<div id="div">Hello</div>)HTML");
  const Node* text = GetElementById("div")->firstChild();
  ASSERT_NE(nullptr, text);
  const AXObject* ax_static_text = GetAXObjectByElementId("div")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreateFirstPositionInObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(0, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ObjectAfterPosition());
}

TEST_F(AccessibilityTest, LastPositionInTextContainer) {
  SetBodyInnerHTML(R"HTML(<div id="div">Hello</div>)HTML");
  const Node* text = GetElementById("div")->lastChild();
  ASSERT_NE(nullptr, text);
  const AXObject* ax_static_text = GetAXObjectByElementId("div")->LastChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreateLastPositionInObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(5, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ObjectAfterPosition());
}

//
// Test comparing two AXPosition objects based on their position in the
// accessibility tree.
//

TEST_F(AccessibilityTest, AXPositionComparisonOperators) {
  SetBodyInnerHTML(R"HTML(<input id="input" type="text" value="value">
                   <p id="paragraph">hello<br>there</p>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const auto root_first = AXPosition::CreateFirstPositionInObject(*root);
  const auto root_last = AXPosition::CreateLastPositionInObject(*root);

  const AXObject* input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, input);
  const auto input_before = AXPosition::CreatePositionBeforeObject(*input);
  const auto input_after = AXPosition::CreatePositionAfterObject(*input);

  const AXObject* paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, paragraph);
  ASSERT_NE(nullptr, paragraph->FirstChild());
  ASSERT_NE(nullptr, paragraph->LastChild());
  const auto paragraph_before =
      AXPosition::CreatePositionBeforeObject(*paragraph->FirstChild());
  const auto paragraph_after =
      AXPosition::CreatePositionAfterObject(*paragraph->LastChild());
  const auto paragraph_start =
      AXPosition::CreatePositionInTextObject(*paragraph->FirstChild(), 0);
  const auto paragraph_end =
      AXPosition::CreatePositionInTextObject(*paragraph->LastChild(), 5);

  EXPECT_TRUE(root_first == root_first);
  EXPECT_TRUE(root_last == root_last);
  EXPECT_FALSE(root_first != root_first);
  EXPECT_TRUE(root_first != root_last);

  EXPECT_TRUE(root_first < root_last);
  EXPECT_TRUE(root_first <= root_first);
  EXPECT_TRUE(root_last > root_first);
  EXPECT_TRUE(root_last >= root_last);

  EXPECT_TRUE(input_before == root_first);
  EXPECT_TRUE(input_after > root_first);
  EXPECT_TRUE(input_after >= root_first);
  EXPECT_FALSE(input_before < root_first);
  EXPECT_TRUE(input_before <= root_first);

  //
  // Text positions.
  //

  EXPECT_TRUE(paragraph_before == paragraph_start);
  EXPECT_TRUE(paragraph_after == paragraph_end);
  EXPECT_TRUE(paragraph_start < paragraph_end);
}

//
// Test converting to and from visible text with white space.
// The accessibility tree is based on visible text with white space compressed,
// vs. the DOM tree where white space is preserved.
//

TEST_F(AccessibilityTest, PositionInTextWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">     Hello     </p>)HTML");
  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionInTextObject(*ax_static_text, 3);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(8, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ObjectAfterPosition());
}

TEST_F(AccessibilityTest, PositionBeforeTextWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">     Hello     </p>)HTML");
  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionBeforeObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(5, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ObjectAfterPosition());
}

TEST_F(AccessibilityTest, PositionAfterTextWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">     Hello     </p>)HTML");
  const Node* text = GetElementById("paragraph")->lastChild();
  ASSERT_NE(nullptr, text);
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->LastChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionAfterObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(10, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ObjectAfterPosition());
}

TEST_F(AccessibilityTest, PositionBeforeLineBreakWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(Hello     <br id="br">     there)HTML");
  const AXObject* ax_br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, ax_br);
  ASSERT_EQ(AccessibilityRole::kLineBreakRole, ax_br->RoleValue());

  const auto ax_position = AXPosition::CreatePositionBeforeObject(*ax_br);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument().body(), position.AnchorNode());
  EXPECT_EQ(1, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_br, ax_position_from_dom.ObjectAfterPosition());
}

TEST_F(AccessibilityTest, PositionAfterLineBreakWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(Hello     <br id="br">     there)HTML");
  const AXObject* ax_br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, ax_br);
  ASSERT_EQ(AccessibilityRole::kLineBreakRole, ax_br->RoleValue());
  const AXObject* ax_static_text = GetAXRootObject()->DeepestLastChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position = AXPosition::CreatePositionAfterObject(*ax_br);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument().body(), position.AnchorNode());
  EXPECT_EQ(2, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_static_text, ax_position_from_dom.ObjectAfterPosition());
}

TEST_F(AccessibilityTest, FirstPositionInDivContainerWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<div id="div">     Hello<br>there     </div>)HTML");
  const Element* div = GetElementById("div");
  ASSERT_NE(nullptr, div);
  const AXObject* ax_div = GetAXObjectByElementId("div");
  ASSERT_NE(nullptr, ax_div);
  ASSERT_EQ(AccessibilityRole::kGenericContainerRole, ax_div->RoleValue());
  const AXObject* ax_static_text = GetAXRootObject()->DeepestFirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position = AXPosition::CreateFirstPositionInObject(*ax_div);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(div, position.AnchorNode());
  EXPECT_EQ(0, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_static_text, ax_position_from_dom.ObjectAfterPosition());
}

TEST_F(AccessibilityTest, LastPositionInDivContainerWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<div id="div">     Hello<br>there     </div>
                   <div id="divNext">Next div</div>)HTML");
  const Element* div = GetElementById("div");
  ASSERT_NE(nullptr, div);
  const AXObject* ax_div = GetAXObjectByElementId("div");
  ASSERT_NE(nullptr, ax_div);
  ASSERT_EQ(AccessibilityRole::kGenericContainerRole, ax_div->RoleValue());
  const AXObject* ax_div_next = GetAXObjectByElementId("divNext");
  ASSERT_NE(nullptr, ax_div_next);
  ASSERT_EQ(AccessibilityRole::kGenericContainerRole, ax_div_next->RoleValue());

  const auto ax_position = AXPosition::CreateLastPositionInObject(*ax_div);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(div, position.AnchorNode());
  EXPECT_TRUE(position.GetPosition().IsAfterChildren());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_div_next, ax_position_from_dom.ObjectAfterPosition());
}

TEST_F(AccessibilityTest, FirstPositionInTextContainerWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<div id="div">     Hello     </div>)HTML");
  const Node* text = GetElementById("div")->firstChild();
  ASSERT_NE(nullptr, text);
  const AXObject* ax_static_text = GetAXObjectByElementId("div")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreateFirstPositionInObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(5, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ObjectAfterPosition());
}

TEST_F(AccessibilityTest, LastPositionInTextContainerWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<div id="div">     Hello     </div>)HTML");
  const Node* text = GetElementById("div")->lastChild();
  ASSERT_NE(nullptr, text);
  const AXObject* ax_static_text = GetAXObjectByElementId("div")->LastChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(AccessibilityRole::kStaticTextRole, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreateLastPositionInObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(10, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ObjectAfterPosition());
}

//
// Test affinity.
// We need to distinguish between the caret at the end of one line and the
// beginning of the next.
//

TEST_F(AccessibilityTest, PositionInTextWithAffinity) {}

TEST_F(AccessibilityTest, PositionFromTextPositionWithAffinity) {}

TEST_F(AccessibilityTest, PositionInTextWithAffinityAndWhiteSpace) {}

TEST_F(AccessibilityTest, PositionFromTextPositionWithAffinityAndWhiteSpace) {}

//
// Test converting to and from accessibility positions with offsets in labels
// and alt text. Alt text, aria-label and other ARIA relationships can cause the
// accessible name of an object to be different than its DOM text.
//

TEST_F(AccessibilityTest, PositionInHTMLLabel) {}

TEST_F(AccessibilityTest, PositionInARIALabel) {}

TEST_F(AccessibilityTest, PositionInARIALabelledBy) {}

TEST_F(AccessibilityTest, PositionInPlaceholder) {}

TEST_F(AccessibilityTest, PositionInAltText) {}

TEST_F(AccessibilityTest, PositionInTitle) {}

//
// Some objects are accessibility ignored.
//

TEST_F(AccessibilityTest, PositionInIgnoredObject) {}

//
// Aria-hidden can cause things in the DOM to be hidden from accessibility.
//

TEST_F(AccessibilityTest, BeforePositionInARIAHidden) {}

TEST_F(AccessibilityTest, AfterPositionInARIAHidden) {}

TEST_F(AccessibilityTest, FromPositionInARIAHidden) {}

//
// Canvas fallback can cause things to be in the accessibility tree that are not
// in the layout tree.
//

TEST_F(AccessibilityTest, PositionInCanvas) {}

//
// Some layout objects, e.g. list bullets and CSS::before/after content, appear
// in the accessibility tree but is not present in the DOM.
//

TEST_F(AccessibilityTest, PositionBeforeListBullet) {}

TEST_F(AccessibilityTest, PositionAfterListBullet) {}

TEST_F(AccessibilityTest, PositionInCSSContent) {}

//
// Objects deriving from |AXMockObject|, e.g. table columns, are in the
// accessibility tree but are neither in the DOM or layout trees.
//

TEST_F(AccessibilityTest, PositionInTableColumn) {}

}  // namespace blink
