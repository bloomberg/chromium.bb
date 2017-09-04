// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "core/css/StyleEngine.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/LocalFrameView.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(DragUpdateTest, AffectedByDragUpdate) {
  // Check that when dragging the div in the document below, you only get a
  // single element style recalc.

  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.documentElement()->setInnerHTML(
      "<style>div {width:100px;height:100px} div:-webkit-drag { "
      "background-color: green }</style>"
      "<div id='div'>"
      "<span></span>"
      "<span></span>"
      "<span></span>"
      "<span></span>"
      "</div>");

  document.View()->UpdateAllLifecyclePhases();
  unsigned start_count = document.GetStyleEngine().StyleForElementCount();

  document.getElementById("div")->SetDragged(true);
  document.View()->UpdateAllLifecyclePhases();

  unsigned element_count =
      document.GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(1U, element_count);
}

TEST(DragUpdateTest, ChildAffectedByDragUpdate) {
  // Check that when dragging the div in the document below, you get a
  // single element style recalc.

  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.documentElement()->setInnerHTML(
      "<style>div {width:100px;height:100px} div:-webkit-drag .drag { "
      "background-color: green }</style>"
      "<div id='div'>"
      "<span></span>"
      "<span></span>"
      "<span class='drag'></span>"
      "<span></span>"
      "</div>");

  document.UpdateStyleAndLayout();
  unsigned start_count = document.GetStyleEngine().StyleForElementCount();

  document.getElementById("div")->SetDragged(true);
  document.UpdateStyleAndLayout();

  unsigned element_count =
      document.GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(1U, element_count);
}

TEST(DragUpdateTest, SiblingAffectedByDragUpdate) {
  // Check that when dragging the div in the document below, you get a
  // single element style recalc.

  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.documentElement()->setInnerHTML(
      "<style>div {width:100px;height:100px} div:-webkit-drag + .drag { "
      "background-color: green }</style>"
      "<div id='div'>"
      "<span></span>"
      "<span></span>"
      "<span></span>"
      "<span></span>"
      "</div>"
      "<span class='drag'></span>");

  document.UpdateStyleAndLayout();
  unsigned start_count = document.GetStyleEngine().StyleForElementCount();

  document.getElementById("div")->SetDragged(true);
  document.UpdateStyleAndLayout();

  unsigned element_count =
      document.GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(1U, element_count);
}

}  // namespace blink
