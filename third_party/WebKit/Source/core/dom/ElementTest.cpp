// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Element.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/editing/EditingTestBase.h"
#include "core/frame/LocalFrameView.h"
#include "core/geometry/DOMRect.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/paint/PaintLayer.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ElementTest : public EditingTestBase {};

TEST_F(ElementTest, SupportsFocus) {
  Document& document = GetDocument();
  DCHECK(isHTMLHtmlElement(document.documentElement()));
  document.setDesignMode("on");
  document.View()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(document.documentElement()->SupportsFocus())
      << "<html> with designMode=on should be focusable.";
}

TEST_F(ElementTest,
       GetBoundingClientRectCorrectForStickyElementsAfterInsertion) {
  Document& document = GetDocument();
  SetBodyContent(
      "<style>body { margin: 0 }"
      "#scroller { overflow: scroll; height: 100px; width: 100px; }"
      "#sticky { height: 25px; position: sticky; top: 0; left: 25px; }"
      "#padding { height: 500px; width: 300px; }</style>"
      "<div id='scroller'><div id='writer'></div><div id='sticky'></div>"
      "<div id='padding'></div></div>");

  Element* scroller = document.getElementById("scroller");
  Element* writer = document.getElementById("writer");
  Element* sticky = document.getElementById("sticky");

  ASSERT_TRUE(scroller);
  ASSERT_TRUE(writer);
  ASSERT_TRUE(sticky);

  scroller->scrollTo(50.0, 200.0);

  // The sticky element should remain at (0, 25) relative to the viewport due to
  // the constraints.
  DOMRect* bounding_client_rect = sticky->getBoundingClientRect();
  EXPECT_EQ(0, bounding_client_rect->top());
  EXPECT_EQ(25, bounding_client_rect->left());

  // Insert a new <div> above the sticky. This will dirty layout and invalidate
  // the sticky constraints.
  writer->setInnerHTML("<div style='height: 100px; width: 700px;'></div>");
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            document.Lifecycle().GetState());

  // Requesting the bounding client rect should cause both layout and
  // compositing inputs clean to be run, and the sticky result shouldn't change.
  bounding_client_rect = sticky->getBoundingClientRect();
  EXPECT_EQ(DocumentLifecycle::kCompositingInputsClean,
            document.Lifecycle().GetState());
  EXPECT_FALSE(sticky->GetLayoutBoxModelObject()
                   ->Layer()
                   ->NeedsCompositingInputsUpdate());
  EXPECT_EQ(0, bounding_client_rect->top());
  EXPECT_EQ(25, bounding_client_rect->left());
}

TEST_F(ElementTest, OffsetTopAndLeftCorrectForStickyElementsAfterInsertion) {
  Document& document = GetDocument();
  SetBodyContent(
      "<style>body { margin: 0 }"
      "#scroller { overflow: scroll; height: 100px; width: 100px; }"
      "#sticky { height: 25px; position: sticky; top: 0; left: 25px; }"
      "#padding { height: 500px; width: 300px; }</style>"
      "<div id='scroller'><div id='writer'></div><div id='sticky'></div>"
      "<div id='padding'></div></div>");

  Element* scroller = document.getElementById("scroller");
  Element* writer = document.getElementById("writer");
  Element* sticky = document.getElementById("sticky");

  ASSERT_TRUE(scroller);
  ASSERT_TRUE(writer);
  ASSERT_TRUE(sticky);

  scroller->scrollTo(50.0, 200.0);

  // The sticky element should be offset to stay at (0, 25) relative to the
  // viewport due to the constraints.
  EXPECT_EQ(scroller->scrollTop(), sticky->OffsetTop());
  EXPECT_EQ(scroller->scrollLeft() + 25, sticky->OffsetLeft());

  // Insert a new <div> above the sticky. This will dirty layout and invalidate
  // the sticky constraints.
  writer->setInnerHTML("<div style='height: 100px; width: 700px;'></div>");
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            document.Lifecycle().GetState());

  // Requesting either offset should cause both layout and compositing inputs
  // clean to be run, and the sticky result shouldn't change.
  EXPECT_EQ(scroller->scrollTop(), sticky->OffsetTop());
  EXPECT_EQ(DocumentLifecycle::kCompositingInputsClean,
            document.Lifecycle().GetState());
  EXPECT_FALSE(sticky->GetLayoutBoxModelObject()
                   ->Layer()
                   ->NeedsCompositingInputsUpdate());

  // Dirty layout again, since |OffsetTop| will have cleaned it.
  writer->setInnerHTML("<div style='height: 100px; width: 700px;'></div>");
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            document.Lifecycle().GetState());

  // Again requesting an offset should cause layout and compositing to be clean.
  EXPECT_EQ(scroller->scrollLeft() + 25, sticky->OffsetLeft());
  EXPECT_EQ(DocumentLifecycle::kCompositingInputsClean,
            document.Lifecycle().GetState());
  EXPECT_FALSE(sticky->GetLayoutBoxModelObject()
                   ->Layer()
                   ->NeedsCompositingInputsUpdate());
}

TEST_F(ElementTest, BoundsInViewportCorrectForStickyElementsAfterInsertion) {
  Document& document = GetDocument();
  SetBodyContent(
      "<style>body { margin: 0 }"
      "#scroller { overflow: scroll; height: 100px; width: 100px; }"
      "#sticky { height: 25px; position: sticky; top: 0; left: 25px; }"
      "#padding { height: 500px; width: 300px; }</style>"
      "<div id='scroller'><div id='writer'></div><div id='sticky'></div>"
      "<div id='padding'></div></div>");

  Element* scroller = document.getElementById("scroller");
  Element* writer = document.getElementById("writer");
  Element* sticky = document.getElementById("sticky");

  ASSERT_TRUE(scroller);
  ASSERT_TRUE(writer);
  ASSERT_TRUE(sticky);

  scroller->scrollTo(50.0, 200.0);

  // The sticky element should remain at (0, 25) relative to the viewport due to
  // the constraints.
  IntRect bounds_in_viewport = sticky->BoundsInViewport();
  EXPECT_EQ(0, bounds_in_viewport.Y());
  EXPECT_EQ(25, bounds_in_viewport.X());

  // Insert a new <div> above the sticky. This will dirty layout and invalidate
  // the sticky constraints.
  writer->setInnerHTML("<div style='height: 100px; width: 700px;'></div>");
  EXPECT_EQ(DocumentLifecycle::kVisualUpdatePending,
            document.Lifecycle().GetState());

  // Requesting the bounds in viewport should cause both layout and compositing
  // inputs clean to be run, and the sticky result shouldn't change.
  bounds_in_viewport = sticky->BoundsInViewport();
  EXPECT_EQ(DocumentLifecycle::kCompositingInputsClean,
            document.Lifecycle().GetState());
  EXPECT_FALSE(sticky->GetLayoutBoxModelObject()
                   ->Layer()
                   ->NeedsCompositingInputsUpdate());
  EXPECT_EQ(0, bounds_in_viewport.Y());
  EXPECT_EQ(25, bounds_in_viewport.X());
}

TEST_F(ElementTest, StickySubtreesAreTrackedCorrectly) {
  Document& document = GetDocument();
  SetBodyContent(
      "<div id='ancestor'>"
      "  <div id='outerSticky' style='position:sticky;'>"
      "    <div id='child'>"
      "      <div id='grandchild'></div>"
      "      <div id='innerSticky' style='position:sticky;'>"
      "        <div id='greatGrandchild'></div>"
      "      </div>"
      "    </div"
      "  </div>"
      "</div>");

  LayoutObject* ancestor =
      document.getElementById("ancestor")->GetLayoutObject();
  LayoutObject* outer_sticky =
      document.getElementById("outerSticky")->GetLayoutObject();
  LayoutObject* child = document.getElementById("child")->GetLayoutObject();
  LayoutObject* grandchild =
      document.getElementById("grandchild")->GetLayoutObject();
  LayoutObject* inner_sticky =
      document.getElementById("innerSticky")->GetLayoutObject();
  LayoutObject* great_grandchild =
      document.getElementById("greatGrandchild")->GetLayoutObject();

  EXPECT_FALSE(ancestor->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(outer_sticky->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(child->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(grandchild->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(inner_sticky->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(great_grandchild->StyleRef().SubtreeIsSticky());

  // This forces 'child' to fork it's StyleRareInheritedData, so that we can
  // ensure that the sticky subtree update behavior survives forking.
  document.getElementById("child")->SetInlineStyleProperty(
      CSSPropertyWebkitRubyPosition, CSSValueAfter);
  document.View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(DocumentLifecycle::kPaintClean, document.Lifecycle().GetState());

  EXPECT_EQ(RubyPosition::kBefore, outer_sticky->StyleRef().GetRubyPosition());
  EXPECT_EQ(RubyPosition::kAfter, child->StyleRef().GetRubyPosition());
  EXPECT_EQ(RubyPosition::kAfter, grandchild->StyleRef().GetRubyPosition());
  EXPECT_EQ(RubyPosition::kAfter, inner_sticky->StyleRef().GetRubyPosition());
  EXPECT_EQ(RubyPosition::kAfter,
            great_grandchild->StyleRef().GetRubyPosition());

  // Setting -webkit-ruby value shouldn't have affected the sticky subtree bit.
  EXPECT_TRUE(outer_sticky->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(child->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(grandchild->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(inner_sticky->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(great_grandchild->StyleRef().SubtreeIsSticky());

  // Now switch 'outerSticky' back to being non-sticky - all descendents between
  // it and the 'innerSticky' should be updated, and the 'innerSticky' should
  // fork it's StyleRareInheritedData to maintain the sticky subtree bit.
  document.getElementById("outerSticky")
      ->SetInlineStyleProperty(CSSPropertyPosition, CSSValueStatic);
  document.View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(DocumentLifecycle::kPaintClean, document.Lifecycle().GetState());

  EXPECT_FALSE(outer_sticky->StyleRef().SubtreeIsSticky());
  EXPECT_FALSE(child->StyleRef().SubtreeIsSticky());
  EXPECT_FALSE(grandchild->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(inner_sticky->StyleRef().SubtreeIsSticky());
  EXPECT_TRUE(great_grandchild->StyleRef().SubtreeIsSticky());
}

TEST_F(ElementTest, GetElementsByClassNameCrash) {
  // Test for a crash in NodeListsNodeData::AddCache().
  ASSERT_TRUE(GetDocument().InQuirksMode());
  GetDocument().body()->getElementsByClassName("ABC DEF");
  GetDocument().body()->getElementsByClassName("ABC DEF");
  // The test passes if no crash happens.
}

}  // namespace blink
