// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

using namespace HTMLNames;

class AffectedByFocusTest : public ::testing::Test {
 protected:
  struct ElementResult {
    const blink::HTMLQualifiedName tag;
    bool affected_by;
    bool children_or_siblings_affected_by;
  };

  void SetUp() override;

  Document& GetDocument() const { return *document_; }

  void SetHtmlInnerHTML(const char* html_content);

  void CheckElements(ElementResult expected[], unsigned expected_count) const;

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;

  Persistent<Document> document_;
};

void AffectedByFocusTest::SetUp() {
  dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
  document_ = &dummy_page_holder_->GetDocument();
  DCHECK(document_);
}

void AffectedByFocusTest::SetHtmlInnerHTML(const char* html_content) {
  GetDocument().documentElement()->setInnerHTML(String::FromUTF8(html_content));
  GetDocument().View()->UpdateAllLifecyclePhases();
}

void AffectedByFocusTest::CheckElements(ElementResult expected[],
                                        unsigned expected_count) const {
  unsigned i = 0;
  HTMLElement* element = GetDocument().body();

  for (; element && i < expected_count;
       element = Traversal<HTMLElement>::Next(*element), ++i) {
    ASSERT_TRUE(element->HasTagName(expected[i].tag));
    DCHECK(element->GetComputedStyle());
    ASSERT_EQ(expected[i].affected_by,
              element->GetComputedStyle()->AffectedByFocus());
    ASSERT_EQ(expected[i].children_or_siblings_affected_by,
              element->ChildrenOrSiblingsAffectedByFocus());
  }

  DCHECK(!element);
  DCHECK_EQ(i, expected_count);
}

// A global :focus rule in html.css currently causes every single element to be
// affectedByFocus. Check that all elements in a document with no :focus rules
// gets the affectedByFocus set on ComputedStyle and not
// childrenOrSiblingsAffectedByFocus.
TEST_F(AffectedByFocusTest, UAUniversalFocusRule) {
  ElementResult expected[] = {{bodyTag, true, false},
                              {divTag, true, false},
                              {divTag, true, false},
                              {divTag, true, false},
                              {spanTag, true, false}};

  SetHtmlInnerHTML(
      "<body>"
      "<div><div></div></div>"
      "<div><span></span></div>"
      "</body>");

  CheckElements(expected, sizeof(expected) / sizeof(ElementResult));
}

// ":focus div" will mark ascendants of all divs with
// childrenOrSiblingsAffectedByFocus.
TEST_F(AffectedByFocusTest, FocusedAscendant) {
  ElementResult expected[] = {{bodyTag, true, true},
                              {divTag, true, true},
                              {divTag, true, false},
                              {divTag, true, false},
                              {spanTag, true, false}};

  SetHtmlInnerHTML(
      "<head>"
      "<style>:focus div { background-color: pink }</style>"
      "</head>"
      "<body>"
      "<div><div></div></div>"
      "<div><span></span></div>"
      "</body>");

  CheckElements(expected, sizeof(expected) / sizeof(ElementResult));
}

// "body:focus div" will mark the body element with
// childrenOrSiblingsAffectedByFocus.
TEST_F(AffectedByFocusTest, FocusedAscendantWithType) {
  ElementResult expected[] = {{bodyTag, true, true},
                              {divTag, true, false},
                              {divTag, true, false},
                              {divTag, true, false},
                              {spanTag, true, false}};

  SetHtmlInnerHTML(
      "<head>"
      "<style>body:focus div { background-color: pink }</style>"
      "</head>"
      "<body>"
      "<div><div></div></div>"
      "<div><span></span></div>"
      "</body>");

  CheckElements(expected, sizeof(expected) / sizeof(ElementResult));
}

// ":not(body):focus div" should not mark the body element with
// childrenOrSiblingsAffectedByFocus.
// Note that currently ":focus:not(body)" does not do the same. Then the :focus
// is checked and the childrenOrSiblingsAffectedByFocus flag set before the
// negated type selector is found.
TEST_F(AffectedByFocusTest, FocusedAscendantWithNegatedType) {
  ElementResult expected[] = {{bodyTag, true, false},
                              {divTag, true, true},
                              {divTag, true, false},
                              {divTag, true, false},
                              {spanTag, true, false}};

  SetHtmlInnerHTML(
      "<head>"
      "<style>:not(body):focus div { background-color: pink }</style>"
      "</head>"
      "<body>"
      "<div><div></div></div>"
      "<div><span></span></div>"
      "</body>");

  CheckElements(expected, sizeof(expected) / sizeof(ElementResult));
}

// Checking current behavior for ":focus + div", but this is a BUG or at best
// sub-optimal. The focused element will also in this case get
// childrenOrSiblingsAffectedByFocus even if it's really a sibling. Effectively,
// the whole sub-tree of the focused element will have styles recalculated even
// though none of the children are affected. There are other mechanisms that
// makes sure the sibling also gets its styles recalculated.
TEST_F(AffectedByFocusTest, FocusedSibling) {
  ElementResult expected[] = {{bodyTag, true, false},
                              {divTag, true, true},
                              {spanTag, true, false},
                              {divTag, true, false}};

  SetHtmlInnerHTML(
      "<head>"
      "<style>:focus + div { background-color: pink }</style>"
      "</head>"
      "<body>"
      "<div>"
      "  <span></span>"
      "</div>"
      "<div></div>"
      "</body>");

  CheckElements(expected, sizeof(expected) / sizeof(ElementResult));
}

TEST_F(AffectedByFocusTest, AffectedByFocusUpdate) {
  // Check that when focussing the outer div in the document below, you only
  // get a single element style recalc.

  SetHtmlInnerHTML(
      "<style>:focus { border: 1px solid lime; }</style>"
      "<div id=d tabIndex=1>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "</div>");

  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned start_count = GetDocument().GetStyleEngine().StyleForElementCount();

  GetDocument().GetElementById("d")->focus();
  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned element_count =
      GetDocument().GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(1U, element_count);
}

TEST_F(AffectedByFocusTest, ChildrenOrSiblingsAffectedByFocusUpdate) {
  // Check that when focussing the outer div in the document below, you get a
  // style recalc for the whole subtree.

  SetHtmlInnerHTML(
      "<style>:focus div { border: 1px solid lime; }</style>"
      "<div id=d tabIndex=1>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "</div>");

  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned start_count = GetDocument().GetStyleEngine().StyleForElementCount();

  GetDocument().GetElementById("d")->focus();
  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned element_count =
      GetDocument().GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(11U, element_count);
}

TEST_F(AffectedByFocusTest, InvalidationSetFocusUpdate) {
  // Check that when focussing the outer div in the document below, you get a
  // style recalc for the outer div and the class=a div only.

  SetHtmlInnerHTML(
      "<style>:focus .a { border: 1px solid lime; }</style>"
      "<div id=d tabIndex=1>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div class='a'></div>"
      "</div>");

  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned start_count = GetDocument().GetStyleEngine().StyleForElementCount();

  GetDocument().GetElementById("d")->focus();
  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned element_count =
      GetDocument().GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(2U, element_count);
}

TEST_F(AffectedByFocusTest, NoInvalidationSetFocusUpdate) {
  // Check that when focussing the outer div in the document below, you get a
  // style recalc for the outer div only. The invalidation set for :focus will
  // include 'a', but the id=d div should be affectedByFocus, not
  // childrenOrSiblingsAffectedByFocus.

  SetHtmlInnerHTML(
      "<style>#nomatch:focus .a { border: 1px solid lime; }</style>"
      "<div id=d tabIndex=1>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div></div>"
      "<div class='a'></div>"
      "</div>");

  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned start_count = GetDocument().GetStyleEngine().StyleForElementCount();

  GetDocument().GetElementById("d")->focus();
  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned element_count =
      GetDocument().GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(1U, element_count);
}

}  // namespace blink
