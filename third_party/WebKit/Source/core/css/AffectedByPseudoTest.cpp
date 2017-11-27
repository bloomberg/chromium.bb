// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "core/css/StyleEngine.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLElement.h"
#include "core/html_names.h"
#include "core/testing/PageTestBase.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using namespace HTMLNames;

class AffectedByPseudoTest : public PageTestBase {
 protected:
  struct ElementResult {
    const blink::HTMLQualifiedName tag;
    bool children_or_siblings_affected_by;
  };

  void SetHtmlInnerHTML(const char* html_content);
  void CheckElementsForFocus(ElementResult expected[],
                             unsigned expected_count) const;
};

void AffectedByPseudoTest::SetHtmlInnerHTML(const char* html_content) {
  GetDocument().documentElement()->SetInnerHTMLFromString(
      String::FromUTF8(html_content));
  UpdateAllLifecyclePhases();
}

void AffectedByPseudoTest::CheckElementsForFocus(
    ElementResult expected[],
    unsigned expected_count) const {
  unsigned i = 0;
  HTMLElement* element = GetDocument().body();

  for (; element && i < expected_count;
       element = Traversal<HTMLElement>::Next(*element), ++i) {
    ASSERT_TRUE(element->HasTagName(expected[i].tag));
    DCHECK(element->GetComputedStyle());
    ASSERT_EQ(expected[i].children_or_siblings_affected_by,
              element->ChildrenOrSiblingsAffectedByFocus());
  }

  DCHECK(!element);
  DCHECK_EQ(i, expected_count);
}

// ":focus div" will mark ascendants of all divs with
// childrenOrSiblingsAffectedByFocus.
TEST_F(AffectedByPseudoTest, FocusedAscendant) {
  ElementResult expected[] = {{bodyTag, true},
                              {divTag, true},
                              {divTag, false},
                              {divTag, false},
                              {spanTag, false}};

  SetHtmlInnerHTML(R"HTML(
    <head>
    <style>:focus div { background-color: pink }</style>
    </head>
    <body>
    <div><div></div></div>
    <div><span></span></div>
    </body>
  )HTML");

  CheckElementsForFocus(expected, sizeof(expected) / sizeof(ElementResult));
}

// "body:focus div" will mark the body element with
// childrenOrSiblingsAffectedByFocus.
TEST_F(AffectedByPseudoTest, FocusedAscendantWithType) {
  ElementResult expected[] = {{bodyTag, true},
                              {divTag, false},
                              {divTag, false},
                              {divTag, false},
                              {spanTag, false}};

  SetHtmlInnerHTML(R"HTML(
    <head>
    <style>body:focus div { background-color: pink }</style>
    </head>
    <body>
    <div><div></div></div>
    <div><span></span></div>
    </body>
  )HTML");

  CheckElementsForFocus(expected, sizeof(expected) / sizeof(ElementResult));
}

// ":not(body):focus div" should not mark the body element with
// childrenOrSiblingsAffectedByFocus.
// Note that currently ":focus:not(body)" does not do the same. Then the :focus
// is checked and the childrenOrSiblingsAffectedByFocus flag set before the
// negated type selector is found.
TEST_F(AffectedByPseudoTest, FocusedAscendantWithNegatedType) {
  ElementResult expected[] = {{bodyTag, false},
                              {divTag, true},
                              {divTag, false},
                              {divTag, false},
                              {spanTag, false}};

  SetHtmlInnerHTML(R"HTML(
    <head>
    <style>:not(body):focus div { background-color: pink }</style>
    </head>
    <body>
    <div><div></div></div>
    <div><span></span></div>
    </body>
  )HTML");

  CheckElementsForFocus(expected, sizeof(expected) / sizeof(ElementResult));
}

// Checking current behavior for ":focus + div", but this is a BUG or at best
// sub-optimal. The focused element will also in this case get
// childrenOrSiblingsAffectedByFocus even if it's really a sibling. Effectively,
// the whole sub-tree of the focused element will have styles recalculated even
// though none of the children are affected. There are other mechanisms that
// makes sure the sibling also gets its styles recalculated.
TEST_F(AffectedByPseudoTest, FocusedSibling) {
  ElementResult expected[] = {
      {bodyTag, false}, {divTag, true}, {spanTag, false}, {divTag, false}};

  SetHtmlInnerHTML(R"HTML(
    <head>
    <style>:focus + div { background-color: pink }</style>
    </head>
    <body>
    <div>
      <span></span>
    </div>
    <div></div>
    </body>
  )HTML");

  CheckElementsForFocus(expected, sizeof(expected) / sizeof(ElementResult));
}

TEST_F(AffectedByPseudoTest, AffectedByFocusUpdate) {
  // Check that when focussing the outer div in the document below, you only
  // get a single element style recalc.

  SetHtmlInnerHTML(R"HTML(
    <style>:focus { border: 1px solid lime; }</style>
    <div id=d tabIndex=1>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  unsigned start_count = GetStyleEngine().StyleForElementCount();

  GetElementById("d")->focus();
  UpdateAllLifecyclePhases();

  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(1U, element_count);
}

TEST_F(AffectedByPseudoTest, ChildrenOrSiblingsAffectedByFocusUpdate) {
  // Check that when focussing the outer div in the document below, you get a
  // style recalc for the whole subtree.

  SetHtmlInnerHTML(R"HTML(
    <style>:focus div { border: 1px solid lime; }</style>
    <div id=d tabIndex=1>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  unsigned start_count = GetStyleEngine().StyleForElementCount();

  GetElementById("d")->focus();
  UpdateAllLifecyclePhases();

  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(11U, element_count);
}

TEST_F(AffectedByPseudoTest, InvalidationSetFocusUpdate) {
  // Check that when focussing the outer div in the document below, you get a
  // style recalc for the outer div and the class=a div only.

  SetHtmlInnerHTML(R"HTML(
    <style>:focus .a { border: 1px solid lime; }</style>
    <div id=d tabIndex=1>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div class='a'></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  unsigned start_count = GetStyleEngine().StyleForElementCount();

  GetElementById("d")->focus();
  UpdateAllLifecyclePhases();

  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(2U, element_count);
}

TEST_F(AffectedByPseudoTest, NoInvalidationSetFocusUpdate) {
  // Check that when focussing the outer div in the document below, you get a
  // style recalc for the outer div only. The invalidation set for :focus will
  // include 'a', but the id=d div should be affectedByFocus, not
  // childrenOrSiblingsAffectedByFocus.

  SetHtmlInnerHTML(R"HTML(
    <style>#nomatch:focus .a { border: 1px solid lime; }</style>
    <div id=d tabIndex=1>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div class='a'></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  unsigned start_count = GetStyleEngine().StyleForElementCount();

  GetElementById("d")->focus();
  UpdateAllLifecyclePhases();

  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(1U, element_count);
}

TEST_F(AffectedByPseudoTest, FocusWithinCommonAncestor) {
  // Check that when changing the focus between 2 elements we don't need a style
  // recalc for all the ancestors affected by ":focus-within".

  SetHtmlInnerHTML(R"HTML(
    <style>div:focus-within { background-color: lime; }</style>
    <div>
      <div>
        <div id=focusme1 tabIndex=1></div>
        <div id=focusme2 tabIndex=2></div>
      <div>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();

  unsigned start_count = GetStyleEngine().StyleForElementCount();

  GetElementById("focusme1")->focus();
  UpdateAllLifecyclePhases();

  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;

  EXPECT_EQ(3U, element_count);

  start_count += element_count;

  GetElementById("focusme2")->focus();
  UpdateAllLifecyclePhases();

  element_count = GetStyleEngine().StyleForElementCount() - start_count;

  // Only "focusme1" & "focusme2" elements need a recalc thanks to the common
  // ancestor strategy.
  EXPECT_EQ(2U, element_count);
}

TEST_F(AffectedByPseudoTest, HoverScrollbar) {
  SetHtmlInnerHTML(
      "<style>div::-webkit-scrollbar:hover { color: pink; }</style>"
      "<div id=div1></div>");

  UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetElementById("div1")->GetComputedStyle()->AffectedByHover());
}

}  // namespace blink
