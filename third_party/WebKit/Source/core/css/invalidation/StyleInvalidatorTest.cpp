// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/invalidation/StyleInvalidator.h"

#include "core/css/StyleEngine.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class StyleInvalidatorTest : public ::testing::Test {
 protected:
  void SetUp() override;

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }
  StyleEngine& GetStyleEngine() { return GetDocument().GetStyleEngine(); }
  StyleInvalidator& GetStyleInvalidator() {
    return GetDocument().GetStyleEngine().GetStyleInvalidator();
  }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void StyleInvalidatorTest::SetUp() {
  dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
}

TEST_F(StyleInvalidatorTest, ScheduleOnDocumentNode) {
  GetDocument().body()->setInnerHTML(
      "<div id='d'></div><i id='i'></i><span></span>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  unsigned before_count = GetStyleEngine().StyleForElementCount();

  RefPtr<DescendantInvalidationSet> set = DescendantInvalidationSet::Create();
  set->AddTagName("div");
  set->AddTagName("span");

  InvalidationLists lists;
  lists.descendants.push_back(set);
  GetStyleInvalidator().ScheduleInvalidationSetsForNode(lists, GetDocument());

  EXPECT_TRUE(GetDocument().NeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());

  GetStyleInvalidator().Invalidate(GetDocument());

  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().NeedsStyleRecalc());
  EXPECT_TRUE(GetDocument().ChildNeedsStyleRecalc());

  GetDocument().View()->UpdateAllLifecyclePhases();
  unsigned after_count = GetStyleEngine().StyleForElementCount();
  EXPECT_EQ(2u, after_count - before_count);
}

}  // namespace blink
