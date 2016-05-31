// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementReactionStack.h"

#include "core/dom/custom/CustomElementReaction.h"
#include "core/dom/custom/CustomElementReactionTestHelpers.h"
#include "core/dom/custom/CustomElementTestHelpers.h"
#include "core/html/HTMLDocument.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/AtomicString.h"
#include <initializer_list>
#include <vector>

namespace blink {

TEST(CustomElementReactionStackTest, one)
{
    std::vector<char> log;

    CustomElementReactionStack* stack = new CustomElementReactionStack();
    stack->push();
    stack->enqueue(CreateElement("a"), new TestReaction({new Log('a', log)}));
    stack->popInvokingReactions();

    EXPECT_EQ(log, std::vector<char>({'a'}))
        << "popping the reaction stack should run reactions";
}

TEST(CustomElementReactionStackTest, multipleElements)
{
    std::vector<char> log;

    CustomElementReactionStack* stack = new CustomElementReactionStack();
    stack->push();
    stack->enqueue(CreateElement("a"), new TestReaction({new Log('a', log)}));
    stack->enqueue(CreateElement("a"), new TestReaction({new Log('b', log)}));
    stack->popInvokingReactions();

    EXPECT_EQ(log, std::vector<char>({'a', 'b'}))
        << "reactions should run in the order the elements queued";
}

TEST(CustomElementReactionStackTest, popTopEmpty)
{
    std::vector<char> log;

    CustomElementReactionStack* stack = new CustomElementReactionStack();
    stack->push();
    stack->enqueue(CreateElement("a"), new TestReaction({new Log('a', log)}));
    stack->push();
    stack->popInvokingReactions();

    EXPECT_EQ(log, std::vector<char>())
        << "popping the empty top-of-stack should not run any reactions";
}

TEST(CustomElementReactionStackTest, popTop)
{
    std::vector<char> log;

    CustomElementReactionStack* stack = new CustomElementReactionStack();
    stack->push();
    stack->enqueue(CreateElement("a"), new TestReaction({new Log('a', log)}));
    stack->push();
    stack->enqueue(CreateElement("a"), new TestReaction({new Log('b', log)}));
    stack->popInvokingReactions();

    EXPECT_EQ(log, std::vector<char>({'b'}))
        << "popping the top-of-stack should only run top-of-stack reactions";
}

TEST(CustomElementReactionStackTest, requeueingDoesNotReorderElements)
{
    std::vector<char> log;

    Element* element = CreateElement("a");

    CustomElementReactionStack* stack = new CustomElementReactionStack();
    stack->push();
    stack->enqueue(element, new TestReaction({new Log('a', log)}));
    stack->enqueue(CreateElement("a"), new TestReaction({new Log('z', log)}));
    stack->enqueue(element, new TestReaction({new Log('b', log)}));
    stack->popInvokingReactions();

    EXPECT_EQ(log, std::vector<char>({'a', 'b', 'z'}))
        << "reactions should run together in the order elements were queued";
}

TEST(CustomElementReactionStackTest, oneReactionQueuePerElement)
{
    std::vector<char> log;

    Element* element = CreateElement("a");

    CustomElementReactionStack* stack = new CustomElementReactionStack();
    stack->push();
    stack->enqueue(element, new TestReaction({new Log('a', log)}));
    stack->enqueue(CreateElement("a"), new TestReaction({new Log('z', log)}));
    stack->push();
    stack->enqueue(CreateElement("a"), new TestReaction({new Log('y', log)}));
    stack->enqueue(element, new TestReaction({new Log('b', log)}));
    stack->popInvokingReactions();

    EXPECT_EQ(log, std::vector<char>({'y', 'a', 'b'}))
        << "reactions should run together in the order elements were queued";

    log.clear();
    stack->popInvokingReactions();
    EXPECT_EQ(log, std::vector<char>({'z'})) << "reactions should be run once";
}

} // namespace blink
