// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_child_iterator.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/ng/ng_block_break_token.h"
#include "core/layout/ng/ng_block_node.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class NGBlockChildIteratorTest : public RenderingTest {
 public:
  NGBlockChildIteratorTest() {
    RuntimeEnabledFeatures::SetLayoutNGEnabled(true);
  }
  ~NGBlockChildIteratorTest() {
    RuntimeEnabledFeatures::SetLayoutNGEnabled(false);
  }
};

TEST_F(NGBlockChildIteratorTest, NullFirstChild) {
  NGBlockChildIterator iterator(nullptr, nullptr);
  ASSERT_EQ(NGBlockChildIterator::Entry(nullptr, nullptr),
            iterator.NextChild());
}

TEST_F(NGBlockChildIteratorTest, NoBreakToken) {
  SetBodyInnerHTML(R"HTML(
      <div id='child1'></div>
      <div id='child2'></div>
      <div id='child3'></div>
    )HTML");
  NGLayoutInputNode node1 =
      NGBlockNode(ToLayoutBox(GetLayoutObjectByElementId("child1")));
  NGLayoutInputNode node2 = node1.NextSibling();
  NGLayoutInputNode node3 = node2.NextSibling();

  // The iterator should loop through three children.
  NGBlockChildIterator iterator(node1, nullptr);
  ASSERT_EQ(NGBlockChildIterator::Entry(node1, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node2, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node3, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(nullptr, nullptr),
            iterator.NextChild());
}

TEST_F(NGBlockChildIteratorTest, BreakTokenWithFinishedChild) {
  SetBodyInnerHTML(R"HTML(
      <div id='container'>
        <div id='child1'></div>
        <div id='child2'></div>
        <div id='child3'></div>
      </div>
    )HTML");
  NGBlockNode container =
      NGBlockNode(ToLayoutBox(GetLayoutObjectByElementId("container")));
  NGLayoutInputNode node1 = container.FirstChild();
  NGLayoutInputNode node2 = node1.NextSibling();
  NGLayoutInputNode node3 = node2.NextSibling();

  Vector<RefPtr<NGBreakToken>> child_break_tokens;
  child_break_tokens.push_back(NGBlockBreakToken::Create(node1, LayoutUnit()));
  RefPtr<NGBlockBreakToken> parent_token =
      NGBlockBreakToken::Create(container, LayoutUnit(50), child_break_tokens);

  // The iterator should loop through two children.
  NGBlockChildIterator iterator(node1, parent_token.Get());
  ASSERT_EQ(NGBlockChildIterator::Entry(node2, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node3, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(nullptr, nullptr),
            iterator.NextChild());

  child_break_tokens.push_back(NGBlockBreakToken::Create(node2, LayoutUnit()));
  parent_token =
      NGBlockBreakToken::Create(container, LayoutUnit(50), child_break_tokens);

  // The first break token is for the #child2. That means that everything
  // preceding it (i.e. #child1) has already been finished. The break token for
  // #child2 is marked as finished, so all we need to visit is #child3.
  NGBlockChildIterator iterator2(node1, parent_token.Get());
  ASSERT_EQ(NGBlockChildIterator::Entry(node3, nullptr), iterator2.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(nullptr, nullptr),
            iterator2.NextChild());
}

TEST_F(NGBlockChildIteratorTest, BreakTokenWithUnFinishedChild) {
  SetBodyInnerHTML(R"HTML(
      <div id='container'>
        <div id='child1'></div>
        <div id='child2'></div>
        <div id='child3'></div>
      </div>
    )HTML");
  NGBlockNode container =
      NGBlockNode(ToLayoutBox(GetLayoutObjectByElementId("container")));
  NGLayoutInputNode node1 = container.FirstChild();
  NGLayoutInputNode node2 = node1.NextSibling();
  NGLayoutInputNode node3 = node2.NextSibling();

  Vector<RefPtr<NGBreakToken>> child_break_tokens;
  RefPtr<NGBreakToken> child_token =
      NGBlockBreakToken::Create(node1, LayoutUnit(), child_break_tokens);
  child_break_tokens.push_back(child_token);
  RefPtr<NGBlockBreakToken> parent_token =
      NGBlockBreakToken::Create(container, LayoutUnit(50), child_break_tokens);

  // The iterator should loop through three children, one with a break token.
  NGBlockChildIterator iterator(node1, parent_token.Get());
  ASSERT_EQ(NGBlockChildIterator::Entry(node1, child_token.Get()),
            iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node2, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node3, nullptr), iterator.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(nullptr, nullptr),
            iterator.NextChild());

  child_token =
      NGBlockBreakToken::Create(node2, LayoutUnit(), child_break_tokens);
  child_break_tokens.push_back(child_token);
  parent_token =
      NGBlockBreakToken::Create(container, LayoutUnit(50), child_break_tokens);

  // The first break token is for the #child2. That means that everything
  // preceding it (i.e. #child1) has already been finished. The break token for
  // #child2 is not marked as finished, so we need to visit that child, and
  // #child3.
  NGBlockChildIterator iterator2(node1, parent_token.Get());
  ASSERT_EQ(NGBlockChildIterator::Entry(node2, child_token.Get()),
            iterator2.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(node3, nullptr), iterator2.NextChild());
  ASSERT_EQ(NGBlockChildIterator::Entry(nullptr, nullptr),
            iterator2.NextChild());
}

}  // namespace
}  // namespace blink
