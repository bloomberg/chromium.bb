// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_selection.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_position.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_selection_test.h"

namespace blink {

//
// Basic tests.
//

TEST_F(AccessibilitySelectionTest, SetSelectionInText) {
  SetBodyInnerHTML(R"HTML(<p id='paragraph'>Hello</p>)HTML");

  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());

  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);

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
  EXPECT_EQ("<Paragraph: ><StaticText: Hel^lo|>",
            GetSelectionText(ax_selection));
}

TEST_F(AccessibilitySelectionTest, SetSelectionInTextWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<p id='paragraph'>     Hello</p>)HTML");

  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());

  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChild();
  ASSERT_NE(nullptr, ax_static_text);

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
  EXPECT_EQ("<Paragraph: ><StaticText: Hel^lo|>",
            GetSelectionText(ax_selection));
}

//
// Get selection tests.
// Retrieving a selection with endpoints which have no corresponding objects in
// the accessibility tree, e.g. which are aria-hidden, should shring
// |AXSelection| to valid endpoints.
//

TEST_F(AccessibilitySelectionTest, SetSelectionInARIAHidden) {
  SetSelectionText(R"HTML(
      <div role="main">
        <p>Before aria-hidden.</p>
        ^<p aria-hidden="true">Aria-hidden 1.</p>
        <p>In the middle of aria-hidden.</p>
        <p aria-hidden="true">Aria-hidden 2.</p>|
        <p>After aria-hidden.</p>
      </div>
      )HTML");

  const SelectionInDOMTree selection =
      GetFrame().Selection().GetSelectionInDOMTree();

  const auto ax_selection_shrink = AXSelection::FromSelection(
      selection, AXSelectionBehavior::kShrinkToValidDOMRange);
  EXPECT_EQ("", GetSelectionText(ax_selection_shrink));

  const auto ax_selection_extend = AXSelection::FromSelection(
      selection, AXSelectionBehavior::kExtendToValidDOMRange);
  EXPECT_EQ("", GetSelectionText(ax_selection_extend));
}

//
// Set selection tests.
// Setting the selection from an |AXSelection| that has endpoints which are not
// present in the layout tree should shring the selection to visible endpoints.
//

TEST_F(AccessibilitySelectionTest, SetSelectionAroundListBullet) {
  SetSelectionText(R"HTML(
      <div role="main">
        <ul>
          ^<li>Item 1.</li>
          <li>Item 2.</li>|
        </ul>
      </div>
      )HTML");

  const SelectionInDOMTree selection =
      GetFrame().Selection().GetSelectionInDOMTree();

  const auto ax_selection_shrink = AXSelection::FromSelection(
      selection, AXSelectionBehavior::kShrinkToValidDOMRange);
  EXPECT_EQ("", GetSelectionText(ax_selection_shrink));

  const auto ax_selection_extend = AXSelection::FromSelection(
      selection, AXSelectionBehavior::kExtendToValidDOMRange);
  EXPECT_EQ("", GetSelectionText(ax_selection_extend));
}

}  // namespace blink
