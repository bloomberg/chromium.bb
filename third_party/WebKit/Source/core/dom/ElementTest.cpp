// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Element.h"

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/paint/PaintLayer.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

TEST(ElementTest, SupportsFocus) {
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create();
  Document& document = pageHolder->document();
  DCHECK(isHTMLHtmlElement(document.documentElement()));
  document.setDesignMode("on");
  document.view()->updateAllLifecyclePhases();
  EXPECT_TRUE(document.documentElement()->supportsFocus())
      << "<html> with designMode=on should be focusable.";
}

// Verifies that calling getBoundingClientRect cleans compositor inputs.
// Cleaning the compositor inputs is required so that position:sticky elements
// and their descendants have the correct location.
TEST(ElementTest, GetBoundingClientRectUpdatesCompositorInputs) {
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create();
  Document& document = pageHolder->document();

  document.view()->updateAllLifecyclePhases();
  EXPECT_EQ(DocumentLifecycle::PaintClean, document.lifecycle().state());

  document.body()->setInnerHTML("<div id='test'></div>");
  EXPECT_EQ(DocumentLifecycle::VisualUpdatePending,
            document.lifecycle().state());

  document.body()->getBoundingClientRect();
  EXPECT_EQ(DocumentLifecycle::CompositingClean, document.lifecycle().state());
}

// Verifies that calling scrollIntoView* cleans compositor inputs. Cleaning the
// compositor inputs is required so that position:sticky elements and their
// descendants have the correct location.
TEST(ElementTest, ScrollIntoViewUpdatesCompositorInputs) {
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create();
  Document& document = pageHolder->document();

  document.view()->updateAllLifecyclePhases();
  EXPECT_EQ(DocumentLifecycle::PaintClean, document.lifecycle().state());

  document.body()->setInnerHTML("<div id='test'></div>");
  EXPECT_EQ(DocumentLifecycle::VisualUpdatePending,
            document.lifecycle().state());

  document.body()->scrollIntoView();
  EXPECT_EQ(DocumentLifecycle::CompositingClean, document.lifecycle().state());

  document.body()->setInnerHTML("<div id='test2'></div>");
  EXPECT_EQ(DocumentLifecycle::VisualUpdatePending,
            document.lifecycle().state());

  document.body()->scrollIntoViewIfNeeded();
  EXPECT_EQ(DocumentLifecycle::CompositingClean, document.lifecycle().state());
}

// Verifies that calling offsetTop/offsetLeft cleans compositor inputs.
// Cleaning the compositor inputs is required so that position:sticky elements
// and their descendants have the correct location.
TEST(ElementTest, OffsetTopAndLeftUpdateCompositorInputs) {
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create();
  Document& document = pageHolder->document();

  document.view()->updateAllLifecyclePhases();
  EXPECT_EQ(DocumentLifecycle::PaintClean, document.lifecycle().state());

  document.body()->setInnerHTML("<div id='test'></div>");
  EXPECT_EQ(DocumentLifecycle::VisualUpdatePending,
            document.lifecycle().state());

  document.body()->offsetTop();
  EXPECT_EQ(DocumentLifecycle::CompositingClean, document.lifecycle().state());

  document.body()->setInnerHTML("<div id='test2'></div>");
  EXPECT_EQ(DocumentLifecycle::VisualUpdatePending,
            document.lifecycle().state());

  document.body()->offsetLeft();
  EXPECT_EQ(DocumentLifecycle::CompositingClean, document.lifecycle().state());
}

TEST(ElementTest, OffsetTopAndLeftCorrectForStickyElementsAfterInsertion) {
  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create();
  Document& document = pageHolder->document();

  document.body()->setInnerHTML(
      "<style>body { margin: 0 }"
      "#scroller { overflow: scroll; height: 100px; width: 100px; }"
      "#sticky { height: 25px; position: sticky; top: 0; left: 25px; }"
      "#padding { height: 500px; width: 300px; }</style>"
      "<div id='scroller'><div id='writer'></div><div id='sticky'></div>"
      "<div id='padding'></div></div>");
  document.view()->updateAllLifecyclePhases();

  Element* scroller = document.getElementById("scroller");
  Element* writer = document.getElementById("writer");
  Element* sticky = document.getElementById("sticky");

  ASSERT_TRUE(scroller);
  ASSERT_TRUE(writer);
  ASSERT_TRUE(sticky);

  scroller->scrollTo(0, 200.0);

  // After scrolling, the position:sticky element should be offset to stay at
  // the top of the viewport.
  EXPECT_EQ(scroller->scrollTop(), sticky->offsetTop());

  // Insert a new <div> above the sticky. This will dirty layout.
  writer->setInnerHTML("<div style='height: 100px;'></div>");
  EXPECT_EQ(DocumentLifecycle::VisualUpdatePending,
            document.lifecycle().state());

  // Querying the new offset of the sticky element should now cause both layout
  // and compositing inputs to be clean and should return the correct offset.
  int offsetTop = sticky->offsetTop();
  EXPECT_EQ(DocumentLifecycle::CompositingClean, document.lifecycle().state());
  EXPECT_FALSE(
      sticky->layoutBoxModelObject()->layer()->needsCompositingInputsUpdate());
  EXPECT_EQ(scroller->scrollTop(), offsetTop);

  scroller->scrollTo(50.0, 0);

  // After scrolling, the position:sticky element should be offset to stay 25
  // pixels from the left of the viewport.
  EXPECT_EQ(scroller->scrollLeft() + 25, sticky->offsetLeft());

  // Insert a new <div> above the sticky. This will dirty layout.
  writer->setInnerHTML("<div style='width: 700px;'></div>");
  EXPECT_EQ(DocumentLifecycle::VisualUpdatePending,
            document.lifecycle().state());

  // Again getting the offset should cause layout and compositing to be clean.
  int offsetLeft = sticky->offsetLeft();
  EXPECT_EQ(DocumentLifecycle::CompositingClean, document.lifecycle().state());
  EXPECT_FALSE(
      sticky->layoutBoxModelObject()->layer()->needsCompositingInputsUpdate());
  EXPECT_EQ(scroller->scrollLeft() + 25, offsetLeft);
}

}  // namespace blink
