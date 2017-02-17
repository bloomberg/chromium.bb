// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/invalidation/StyleInvalidator.h"

#include "core/dom/StyleEngine.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class StyleInvalidatorTest : public ::testing::Test {
 protected:
  void SetUp() override;

  Document& document() { return m_dummyPageHolder->document(); }
  StyleEngine& styleEngine() { return document().styleEngine(); }
  StyleInvalidator& styleInvalidator() {
    return document().styleEngine().styleInvalidator();
  }

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

void StyleInvalidatorTest::SetUp() {
  m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
}

TEST_F(StyleInvalidatorTest, ScheduleOnDocumentNode) {
  document().body()->setInnerHTML(
      "<div id='d'></div><i id='i'></i><span></span>");
  document().view()->updateAllLifecyclePhases();

  unsigned beforeCount = styleEngine().styleForElementCount();

  RefPtr<DescendantInvalidationSet> set = DescendantInvalidationSet::create();
  set->addTagName("div");
  set->addTagName("span");

  InvalidationLists lists;
  lists.descendants.push_back(set);
  styleInvalidator().scheduleInvalidationSetsForNode(lists, document());

  EXPECT_TRUE(document().needsStyleInvalidation());
  EXPECT_FALSE(document().childNeedsStyleInvalidation());

  styleInvalidator().invalidate(document());

  EXPECT_FALSE(document().needsStyleInvalidation());
  EXPECT_FALSE(document().childNeedsStyleInvalidation());
  EXPECT_FALSE(document().needsStyleRecalc());
  EXPECT_TRUE(document().childNeedsStyleRecalc());

  document().view()->updateAllLifecyclePhases();
  unsigned afterCount = styleEngine().styleForElementCount();
  EXPECT_EQ(2u, afterCount - beforeCount);
}

}  // namespace blink
