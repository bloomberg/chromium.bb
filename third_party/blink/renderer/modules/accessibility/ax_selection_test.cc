// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_selection.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_position.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_selection_test.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace test {

//
// Basic tests.
//

TEST_F(AccessibilitySelectionTest, FromCurrentSelection) {
  GetPage().GetSettings().SetScriptEnabled(true);
  SetBodyInnerHTML(R"HTML(
      <p id="paragraph1">Hello.</p>
      <p id="paragraph2">How are you?</p>
      )HTML");

  ASSERT_FALSE(AXSelection::FromCurrentSelection(GetDocument()).IsValid());

  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_NE(nullptr, script_element);
  script_element->setTextContent(R"SCRIPT(
      let text1 = document.querySelectorAll('p')[0].firstChild;
      let paragraph2 = document.querySelectorAll('p')[1];
      let range = document.createRange();
      range.setStart(text1, 3);
      range.setEnd(paragraph2, 1);
      let selection = getSelection();
      selection.removeAllRanges();
      selection.addRange(range);
      )SCRIPT");
  GetDocument().body()->AppendChild(script_element);
  UpdateAllLifecyclePhasesForTest();

  const AXObject* ax_static_text_1 =
      GetAXObjectByElementId("paragraph1")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text_1);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text_1->RoleValue());
  const AXObject* ax_paragraph_2 = GetAXObjectByElementId("paragraph2");
  ASSERT_NE(nullptr, ax_paragraph_2);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph_2->RoleValue());

  const auto ax_selection = AXSelection::FromCurrentSelection(GetDocument());
  ASSERT_TRUE(ax_selection.IsValid());

  EXPECT_TRUE(ax_selection.Base().IsTextPosition());
  EXPECT_EQ(ax_static_text_1, ax_selection.Base().ContainerObject());
  EXPECT_EQ(3, ax_selection.Base().TextOffset());

  EXPECT_FALSE(ax_selection.Extent().IsTextPosition());
  EXPECT_EQ(ax_paragraph_2, ax_selection.Extent().ContainerObject());
  EXPECT_EQ(1, ax_selection.Extent().ChildIndex());

  EXPECT_EQ(
      "++<Paragraph>\n"
      "++++<StaticText: Hel^lo.>\n"
      "++<Paragraph>\n"
      "++++<StaticText: How are you?>\n|",
      GetSelectionText(ax_selection));
}

TEST_F(AccessibilitySelectionTest, ClearCurrentSelection) {
  GetPage().GetSettings().SetScriptEnabled(true);
  SetBodyInnerHTML(R"HTML(
      <p>Hello.</p>
      <p>How are you?</p>
      )HTML");

  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_NE(nullptr, script_element);
  script_element->setTextContent(R"SCRIPT(
      let text1 = document.querySelectorAll('p')[0].firstChild;
      let paragraph2 = document.querySelectorAll('p')[1];
      let range = document.createRange();
      range.setStart(text1, 3);
      range.setEnd(paragraph2, 1);
      let selection = getSelection();
      selection.removeAllRanges();
      selection.addRange(range);
      )SCRIPT");
  GetDocument().body()->AppendChild(script_element);
  UpdateAllLifecyclePhasesForTest();

  SelectionInDOMTree selection = Selection().GetSelectionInDOMTree();
  ASSERT_FALSE(selection.IsNone());

  AXSelection::ClearCurrentSelection(GetDocument());
  selection = Selection().GetSelectionInDOMTree();
  EXPECT_TRUE(selection.IsNone());

  const auto ax_selection = AXSelection::FromCurrentSelection(GetDocument());
  EXPECT_FALSE(ax_selection.IsValid());
  EXPECT_EQ("", GetSelectionText(ax_selection));
}

TEST_F(AccessibilitySelectionTest, CancelSelect) {
  GetPage().GetSettings().SetScriptEnabled(true);
  SetBodyInnerHTML(R"HTML(
      <p id="paragraph1">Hello.</p>
      <p id="paragraph2">How are you?</p>
      )HTML");

  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_NE(nullptr, script_element);
  script_element->setTextContent(R"SCRIPT(
      document.addEventListener("selectstart", (e) => {
        e.preventDefault();
      }, false);
      )SCRIPT");
  GetDocument().body()->AppendChild(script_element);
  UpdateAllLifecyclePhasesForTest();

  const AXObject* ax_static_text_1 =
      GetAXObjectByElementId("paragraph1")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text_1);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text_1->RoleValue());
  const AXObject* ax_paragraph_2 = GetAXObjectByElementId("paragraph2");
  ASSERT_NE(nullptr, ax_paragraph_2);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph_2->RoleValue());

  AXSelection::Builder builder;
  AXSelection ax_selection =
      builder
          .SetBase(AXPosition::CreatePositionInTextObject(*ax_static_text_1, 3))
          .SetExtent(AXPosition::CreateLastPositionInObject(*ax_paragraph_2))
          .Build();

  EXPECT_FALSE(ax_selection.Select()) << "The operation has been cancelled.";
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(AXSelection::FromCurrentSelection(GetDocument()).IsValid());

  GetDocument().RemoveAllEventListeners();

  EXPECT_TRUE(ax_selection.Select()) << "The operation should now go through.";
  EXPECT_FALSE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_EQ(
      "++<Paragraph>\n"
      "++++<StaticText: Hel^lo.>\n"
      "++<Paragraph>\n"
      "++++<StaticText: How are you?>\n|",
      GetSelectionText(AXSelection::FromCurrentSelection(GetDocument())));
}

TEST_F(AccessibilitySelectionTest, DocumentRangeMatchesSelection) {
  SetBodyInnerHTML(R"HTML(
      <p id="paragraph1">Hello.</p>
      <p id="paragraph2">How are you?</p>
      )HTML");

  const AXObject* ax_static_text_1 =
      GetAXObjectByElementId("paragraph1")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text_1);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text_1->RoleValue());
  const AXObject* ax_paragraph_2 = GetAXObjectByElementId("paragraph2");
  ASSERT_NE(nullptr, ax_paragraph_2);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph_2->RoleValue());

  AXSelection::Builder builder;
  AXSelection ax_selection =
      builder
          .SetBase(AXPosition::CreatePositionInTextObject(*ax_static_text_1, 3))
          .SetExtent(AXPosition::CreateLastPositionInObject(*ax_paragraph_2))
          .Build();
  EXPECT_TRUE(ax_selection.Select());
  ASSERT_FALSE(Selection().GetSelectionInDOMTree().IsNone());
  ASSERT_NE(nullptr, Selection().DocumentCachedRange());
  EXPECT_EQ(String("lo.\n      How are you?"),
            Selection().DocumentCachedRange()->toString());
}

TEST_F(AccessibilitySelectionTest, SetSelectionInText) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">Hello</p>)HTML");

  const Node* text = GetDocument().QuerySelector("p")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());

  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  const auto ax_base =
      AXPosition::CreatePositionInTextObject(*ax_static_text, 3);
  const auto ax_extent = AXPosition::CreatePositionAfterObject(*ax_static_text);

  AXSelection::Builder builder;
  const AXSelection ax_selection =
      builder.SetBase(ax_base).SetExtent(ax_extent).Build();
  const SelectionInDOMTree dom_selection = ax_selection.AsSelection();
  EXPECT_EQ(text, dom_selection.Base().AnchorNode());
  EXPECT_EQ(3, dom_selection.Base().OffsetInContainerNode());
  EXPECT_EQ(text, dom_selection.Extent().AnchorNode());
  EXPECT_EQ(5, dom_selection.Extent().OffsetInContainerNode());
  EXPECT_EQ(
      "++<Paragraph>\n"
      "++++<StaticText: Hel^lo|>\n",
      GetSelectionText(ax_selection));
}

TEST_F(AccessibilitySelectionTest, SetSelectionInTextWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">     Hello</p>)HTML");

  const Node* text = GetDocument().QuerySelector("p")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());

  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  const auto ax_base =
      AXPosition::CreatePositionInTextObject(*ax_static_text, 3);
  const auto ax_extent = AXPosition::CreatePositionAfterObject(*ax_static_text);

  AXSelection::Builder builder;
  const AXSelection ax_selection =
      builder.SetBase(ax_base).SetExtent(ax_extent).Build();
  const SelectionInDOMTree dom_selection = ax_selection.AsSelection();
  EXPECT_EQ(text, dom_selection.Base().AnchorNode());
  EXPECT_EQ(8, dom_selection.Base().OffsetInContainerNode());
  EXPECT_EQ(text, dom_selection.Extent().AnchorNode());
  EXPECT_EQ(10, dom_selection.Extent().OffsetInContainerNode());
  EXPECT_EQ(
      "++<Paragraph>\n"
      "++++<StaticText: Hel^lo|>\n",
      GetSelectionText(ax_selection));
}

//
// Get selection tests.
// Retrieving a selection with endpoints which have no corresponding objects in
// the accessibility tree, e.g. which are aria-hidden, should shrink or extend
// the |AXSelection| to valid endpoints.
//

TEST_F(AccessibilitySelectionTest, SetSelectionInARIAHidden) {
  SetBodyInnerHTML(R"HTML(
      <div id="main" role="main">
        <p id="beforeHidden">Before aria-hidden.</p>
        <p id="hidden1" aria-hidden="true">Aria-hidden 1.</p>
        <p id="betweenHidden">In between two aria-hidden elements.</p>
        <p id="hidden2" aria-hidden="true">Aria-hidden 2.</p>
        <p id="afterHidden">After aria-hidden.</p>
      </div>
      )HTML");

  const Node* hidden_1 = GetElementById("hidden1");
  ASSERT_NE(nullptr, hidden_1);
  const Node* hidden_2 = GetElementById("hidden2");
  ASSERT_NE(nullptr, hidden_2);

  const AXObject* ax_main = GetAXObjectByElementId("main");
  ASSERT_NE(nullptr, ax_main);
  ASSERT_EQ(ax::mojom::Role::kMain, ax_main->RoleValue());
  const AXObject* ax_before = GetAXObjectByElementId("beforeHidden");
  ASSERT_NE(nullptr, ax_before);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_before->RoleValue());
  const AXObject* ax_between = GetAXObjectByElementId("betweenHidden");
  ASSERT_NE(nullptr, ax_between);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_between->RoleValue());
  const AXObject* ax_after = GetAXObjectByElementId("afterHidden");
  ASSERT_NE(nullptr, ax_after);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_after->RoleValue());

  ASSERT_NE(nullptr, GetAXObjectByElementId("hidden1"));
  ASSERT_TRUE(GetAXObjectByElementId("hidden1")->AccessibilityIsIgnored());
  ASSERT_NE(nullptr, GetAXObjectByElementId("hidden2"));
  ASSERT_TRUE(GetAXObjectByElementId("hidden2")->AccessibilityIsIgnored());

  const auto hidden_1_first = Position::FirstPositionInNode(*hidden_1);
  const auto hidden_2_first = Position::FirstPositionInNode(*hidden_2);
  const auto selection = SelectionInDOMTree::Builder()
                             .SetBaseAndExtent(hidden_1_first, hidden_2_first)
                             .Build();

  const auto ax_selection_shrink = AXSelection::FromSelection(
      selection, AXSelectionBehavior::kShrinkToValidDOMRange);
  const auto ax_selection_extend = AXSelection::FromSelection(
      selection, AXSelectionBehavior::kExtendToValidDOMRange);

  // The shrunk selection should encompass only the |AXObject| between the two
  // aria-hidden elements and nothing else. This means that its anchor should be
  // before and its focus after the |AXObject| in question.
  ASSERT_FALSE(ax_selection_shrink.Base().IsTextPosition());
  EXPECT_EQ(ax_main, ax_selection_shrink.Base().ContainerObject());
  EXPECT_EQ(ax_between->IndexInParent(),
            ax_selection_shrink.Base().ChildIndex());
  ASSERT_FALSE(ax_selection_shrink.Extent().IsTextPosition());
  EXPECT_EQ(ax_between, ax_selection_shrink.Extent().ContainerObject());
  EXPECT_EQ(1, ax_selection_shrink.Extent().ChildIndex());

  // The extended selection should start after the children of the paragraph
  // before the first aria-hidden element and end right before the paragraph
  // after the last aria-hidden element.
  EXPECT_FALSE(ax_selection_extend.Base().IsTextPosition());
  EXPECT_EQ(ax_before, ax_selection_extend.Base().ContainerObject());
  EXPECT_EQ(1, ax_selection_extend.Base().ChildIndex());
  EXPECT_FALSE(ax_selection_extend.Extent().IsTextPosition());
  EXPECT_EQ(ax_main, ax_selection_extend.Extent().ContainerObject());
  EXPECT_EQ(ax_after->IndexInParent(),
            ax_selection_extend.Extent().ChildIndex());

  // Even though the two AX selections have different anchors and foci, the text
  // selected in the accessibility tree should not differ, because any
  // differences in the equivalent DOM selections concern elements that are
  // aria-hidden. However, the AX selections should still differ if converted to
  // DOM selections.
  const std::string selection_text(
      "++<Main>\n"
      "++++<Paragraph>\n"
      "++++++<StaticText: Before aria-hidden.>\n"
      "^++++<Paragraph>\n"
      "++++++<StaticText: In between two aria-hidden elements.>\n"
      "|++++<Paragraph>\n"
      "++++++<StaticText: After aria-hidden.>\n");
  EXPECT_EQ(selection_text, GetSelectionText(ax_selection_shrink));
  EXPECT_EQ(selection_text, GetSelectionText(ax_selection_extend));
}

//
// Set selection tests.
// Setting the selection from an |AXSelection| that has endpoints which are not
// present in the layout tree should shrink or extend the selection to visible
// endpoints.
//

TEST_F(AccessibilitySelectionTest, SetSelectionAroundListBullet) {
  SetBodyInnerHTML(R"HTML(
      <div role="main">
        <ul>
          <li id="item1">Item 1.</li>
          <li id="item2">Item 2.</li>
        </ul>
      </div>
      )HTML");

  const Node* item_1 = GetElementById("item1");
  ASSERT_NE(nullptr, item_1);
  ASSERT_FALSE(item_1->IsTextNode());
  const Node* item_2 = GetElementById("item2");
  ASSERT_NE(nullptr, item_2);
  ASSERT_FALSE(item_2->IsTextNode());
  const Node* text_2 = item_2->firstChild();
  ASSERT_NE(nullptr, text_2);
  ASSERT_TRUE(text_2->IsTextNode());

  const AXObject* ax_item_1 = GetAXObjectByElementId("item1");
  ASSERT_NE(nullptr, ax_item_1);
  ASSERT_EQ(ax::mojom::Role::kListItem, ax_item_1->RoleValue());
  const AXObject* ax_bullet_1 = ax_item_1->FirstChild();
  ASSERT_NE(nullptr, ax_bullet_1);
  ASSERT_EQ(ax::mojom::Role::kListMarker, ax_bullet_1->RoleValue());
  const AXObject* ax_item_2 = GetAXObjectByElementId("item2");
  ASSERT_NE(nullptr, ax_item_2);
  ASSERT_EQ(ax::mojom::Role::kListItem, ax_item_2->RoleValue());
  const AXObject* ax_text_2 = ax_item_2->LastChild();
  ASSERT_NE(nullptr, ax_text_2);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_text_2->RoleValue());

  AXSelection::Builder builder;
  AXSelection ax_selection =
      builder.SetBase(AXPosition::CreateFirstPositionInObject(*ax_bullet_1))
          .SetExtent(AXPosition::CreateLastPositionInObject(*ax_text_2))
          .Build();

  // The list bullet is not included in the DOM tree. Shrinking the
  // |AXSelection| should skip over it by creating an anchor before the first
  // child of the first <li>, i.e. the text node containing the text "Item 1.".
  // This should be further optimized to a "before children" position at the
  // first <li>.
  ax_selection.Select(AXSelectionBehavior::kShrinkToValidDOMRange);
  const SelectionInDOMTree shrunk_selection =
      Selection().GetSelectionInDOMTree();

  EXPECT_EQ(item_1, shrunk_selection.Base().AnchorNode());
  EXPECT_TRUE(shrunk_selection.Base().IsBeforeChildren());
  ASSERT_TRUE(shrunk_selection.Extent().IsOffsetInAnchor());
  EXPECT_EQ(text_2, shrunk_selection.Extent().AnchorNode());
  EXPECT_EQ(7, shrunk_selection.Extent().OffsetInContainerNode());

  // The list bullet is not included in the DOM tree. Extending the
  // |AXSelection| should move the anchor to before the first <li>.
  ax_selection.Select(AXSelectionBehavior::kExtendToValidDOMRange);
  const SelectionInDOMTree extended_selection =
      Selection().GetSelectionInDOMTree();

  ASSERT_TRUE(extended_selection.Base().IsOffsetInAnchor());
  EXPECT_EQ(item_1->parentNode(), extended_selection.Base().AnchorNode());
  EXPECT_EQ(static_cast<int>(item_1->NodeIndex()),
            extended_selection.Base().OffsetInContainerNode());
  ASSERT_TRUE(extended_selection.Extent().IsOffsetInAnchor());
  EXPECT_EQ(text_2, extended_selection.Extent().AnchorNode());
  EXPECT_EQ(7, extended_selection.Extent().OffsetInContainerNode());

  // The |AXSelection| should remain unaffected by any shrinking and should
  // include both list bullets.
  EXPECT_EQ(
      "++<Main>\n"
      "++++<List>\n"
      "++++++<ListItem>\n"
      "++++++++<ListMarker: \xE2\x80\xA2 >\n"
      "++++++++<StaticText: Item 1.>\n"
      "++++++<ListItem>\n"
      "++++++++<ListMarker: \xE2\x80\xA2 >\n"
      "++++++++<StaticText: Item 2.|>\n",
      GetSelectionText(ax_selection));
}

//
// Declarative tests.
//

TEST_F(AccessibilitySelectionTest, ARIAHidden) {
  RunSelectionTest("aria-hidden");
}

TEST_F(AccessibilitySelectionTest, List) {
  RunSelectionTest("list");
}

TEST_F(AccessibilitySelectionTest, table) {
  RunSelectionTest("table");
}

}  // namespace test
}  // namespace blink
