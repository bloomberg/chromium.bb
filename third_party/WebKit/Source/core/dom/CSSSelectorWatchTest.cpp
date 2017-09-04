// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/CSSSelectorWatch.h"

#include <memory>
#include "core/css/StyleEngine.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class CSSSelectorWatchTest : public ::testing::Test {
 protected:
  void SetUp() override;

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }
  StyleEngine& GetStyleEngine() { return GetDocument().GetStyleEngine(); }

  static const HashSet<String> AddedSelectors(const CSSSelectorWatch& watch) {
    return watch.added_selectors_;
  }
  static const HashSet<String> RemovedSelectors(const CSSSelectorWatch& watch) {
    return watch.removed_selectors_;
  }
  static void ClearAddedRemoved(CSSSelectorWatch&);

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void CSSSelectorWatchTest::SetUp() {
  dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
}

void CSSSelectorWatchTest::ClearAddedRemoved(CSSSelectorWatch& watch) {
  watch.added_selectors_.clear();
  watch.removed_selectors_.clear();
}

TEST_F(CSSSelectorWatchTest, RecalcOnDocumentChange) {
  GetDocument().body()->setInnerHTML(
      "<div>"
      "  <span id='x' class='a'></span>"
      "  <span id='y' class='b'><span></span></span>"
      "  <span id='z'><span></span></span>"
      "</div>");

  CSSSelectorWatch& watch = CSSSelectorWatch::From(GetDocument());

  Vector<String> selectors;
  selectors.push_back(".a");
  watch.WatchCSSSelectors(selectors);

  GetDocument().View()->UpdateAllLifecyclePhases();

  selectors.clear();
  selectors.push_back(".b");
  selectors.push_back(".c");
  selectors.push_back("#nomatch");
  watch.WatchCSSSelectors(selectors);

  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* x = GetDocument().getElementById("x");
  Element* y = GetDocument().getElementById("y");
  Element* z = GetDocument().getElementById("z");
  ASSERT_TRUE(x);
  ASSERT_TRUE(y);
  ASSERT_TRUE(z);

  x->removeAttribute(HTMLNames::classAttr);
  y->removeAttribute(HTMLNames::classAttr);
  z->setAttribute(HTMLNames::classAttr, "c");

  ClearAddedRemoved(watch);

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  GetDocument().View()->UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();

  EXPECT_EQ(2u, after_count - before_count);

  EXPECT_EQ(1u, AddedSelectors(watch).size());
  EXPECT_TRUE(AddedSelectors(watch).Contains(".c"));

  EXPECT_EQ(1u, RemovedSelectors(watch).size());
  EXPECT_TRUE(RemovedSelectors(watch).Contains(".b"));
}

}  // namespace blink
