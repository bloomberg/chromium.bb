// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/custom/CustomElementReactionStack.h"

#include <initializer_list>
#include <vector>
#include "core/html/custom/CustomElementReaction.h"
#include "core/html/custom/CustomElementReactionTestHelpers.h"
#include "core/html/custom/CustomElementTestHelpers.h"
#include "platform/wtf/text/AtomicString.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CustomElementReactionStackTest, one) {
  std::vector<char> log;

  CustomElementReactionStack* stack = new CustomElementReactionStack();
  stack->Push();
  stack->EnqueueToCurrentQueue(CreateElement("a"),
                               new TestReaction({new Log('a', log)}));
  stack->PopInvokingReactions();

  EXPECT_EQ(log, std::vector<char>({'a'}))
      << "popping the reaction stack should run reactions";
}

TEST(CustomElementReactionStackTest, multipleElements) {
  std::vector<char> log;

  CustomElementReactionStack* stack = new CustomElementReactionStack();
  stack->Push();
  stack->EnqueueToCurrentQueue(CreateElement("a"),
                               new TestReaction({new Log('a', log)}));
  stack->EnqueueToCurrentQueue(CreateElement("a"),
                               new TestReaction({new Log('b', log)}));
  stack->PopInvokingReactions();

  EXPECT_EQ(log, std::vector<char>({'a', 'b'}))
      << "reactions should run in the order the elements queued";
}

TEST(CustomElementReactionStackTest, popTopEmpty) {
  std::vector<char> log;

  CustomElementReactionStack* stack = new CustomElementReactionStack();
  stack->Push();
  stack->EnqueueToCurrentQueue(CreateElement("a"),
                               new TestReaction({new Log('a', log)}));
  stack->Push();
  stack->PopInvokingReactions();

  EXPECT_EQ(log, std::vector<char>())
      << "popping the empty top-of-stack should not run any reactions";
}

TEST(CustomElementReactionStackTest, popTop) {
  std::vector<char> log;

  CustomElementReactionStack* stack = new CustomElementReactionStack();
  stack->Push();
  stack->EnqueueToCurrentQueue(CreateElement("a"),
                               new TestReaction({new Log('a', log)}));
  stack->Push();
  stack->EnqueueToCurrentQueue(CreateElement("a"),
                               new TestReaction({new Log('b', log)}));
  stack->PopInvokingReactions();

  EXPECT_EQ(log, std::vector<char>({'b'}))
      << "popping the top-of-stack should only run top-of-stack reactions";
}

TEST(CustomElementReactionStackTest, requeueingDoesNotReorderElements) {
  std::vector<char> log;

  Element* element = CreateElement("a");

  CustomElementReactionStack* stack = new CustomElementReactionStack();
  stack->Push();
  stack->EnqueueToCurrentQueue(element, new TestReaction({new Log('a', log)}));
  stack->EnqueueToCurrentQueue(CreateElement("a"),
                               new TestReaction({new Log('z', log)}));
  stack->EnqueueToCurrentQueue(element, new TestReaction({new Log('b', log)}));
  stack->PopInvokingReactions();

  EXPECT_EQ(log, std::vector<char>({'a', 'b', 'z'}))
      << "reactions should run together in the order elements were queued";
}

TEST(CustomElementReactionStackTest, oneReactionQueuePerElement) {
  std::vector<char> log;

  Element* element = CreateElement("a");

  CustomElementReactionStack* stack = new CustomElementReactionStack();
  stack->Push();
  stack->EnqueueToCurrentQueue(element, new TestReaction({new Log('a', log)}));
  stack->EnqueueToCurrentQueue(CreateElement("a"),
                               new TestReaction({new Log('z', log)}));
  stack->Push();
  stack->EnqueueToCurrentQueue(CreateElement("a"),
                               new TestReaction({new Log('y', log)}));
  stack->EnqueueToCurrentQueue(element, new TestReaction({new Log('b', log)}));
  stack->PopInvokingReactions();

  EXPECT_EQ(log, std::vector<char>({'y', 'a', 'b'}))
      << "reactions should run together in the order elements were queued";

  log.clear();
  stack->PopInvokingReactions();
  EXPECT_EQ(log, std::vector<char>({'z'})) << "reactions should be run once";
}

class EnqueueToStack : public Command {
  WTF_MAKE_NONCOPYABLE(EnqueueToStack);

 public:
  EnqueueToStack(CustomElementReactionStack* stack,
                 Element* element,
                 CustomElementReaction* reaction)
      : stack_(stack), element_(element), reaction_(reaction) {}
  ~EnqueueToStack() override = default;
  virtual void Trace(blink::Visitor* visitor) {
    Command::Trace(visitor);
    visitor->Trace(stack_);
    visitor->Trace(element_);
    visitor->Trace(reaction_);
  }
  void Run(Element*) override {
    stack_->EnqueueToCurrentQueue(element_, reaction_);
  }

 private:
  Member<CustomElementReactionStack> stack_;
  Member<Element> element_;
  Member<CustomElementReaction> reaction_;
};

TEST(CustomElementReactionStackTest, enqueueFromReaction) {
  std::vector<char> log;

  Element* element = CreateElement("a");

  CustomElementReactionStack* stack = new CustomElementReactionStack();
  stack->Push();
  stack->EnqueueToCurrentQueue(
      element, new TestReaction({new EnqueueToStack(
                   stack, element, new TestReaction({new Log('a', log)}))}));
  stack->PopInvokingReactions();

  EXPECT_EQ(log, std::vector<char>({'a'})) << "enqueued reaction from another "
                                              "reaction should run in the same "
                                              "invoke";
}

}  // namespace blink
