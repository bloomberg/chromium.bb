// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementReactionTestHelpers_h
#define CustomElementReactionTestHelpers_h

#include "core/dom/custom/CustomElementReaction.h"

#include <initializer_list>
#include <memory>
#include <vector>
#include "core/dom/custom/CustomElementReactionQueue.h"
#include "core/dom/custom/CustomElementReactionStack.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/Noncopyable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class Element;

class Command : public GarbageCollectedFinalized<Command> {
  WTF_MAKE_NONCOPYABLE(Command);

 public:
  Command() = default;
  virtual ~Command() = default;
  DEFINE_INLINE_VIRTUAL_TRACE() {}
  virtual void Run(Element*) = 0;
};

class Call : public Command {
  WTF_MAKE_NONCOPYABLE(Call);

 public:
  using Callback = WTF::Function<void(Element*)>;
  Call(std::unique_ptr<Callback> callback) : callback_(std::move(callback)) {}
  ~Call() override = default;
  void Run(Element* element) override { callback_->operator()(element); }

 private:
  std::unique_ptr<Callback> callback_;
};

class Unreached : public Command {
  WTF_MAKE_NONCOPYABLE(Unreached);

 public:
  Unreached(const char* message) : message_(message) {}
  ~Unreached() override = default;
  void Run(Element*) override { EXPECT_TRUE(false) << message_; }

 private:
  const char* message_;
};

class Log : public Command {
  WTF_MAKE_NONCOPYABLE(Log);

 public:
  Log(char what, std::vector<char>& where) : what_(what), where_(where) {}
  ~Log() override = default;
  void Run(Element*) override { where_.push_back(what_); }

 private:
  char what_;
  std::vector<char>& where_;
};

class Recurse : public Command {
  WTF_MAKE_NONCOPYABLE(Recurse);

 public:
  Recurse(CustomElementReactionQueue* queue) : queue_(queue) {}
  ~Recurse() override = default;
  DEFINE_INLINE_VIRTUAL_TRACE() {
    Command::Trace(visitor);
    visitor->Trace(queue_);
  }
  void Run(Element* element) override { queue_->InvokeReactions(element); }

 private:
  Member<CustomElementReactionQueue> queue_;
};

class Enqueue : public Command {
  WTF_MAKE_NONCOPYABLE(Enqueue);

 public:
  Enqueue(CustomElementReactionQueue* queue, CustomElementReaction* reaction)
      : queue_(queue), reaction_(reaction) {}
  ~Enqueue() override = default;
  DEFINE_INLINE_VIRTUAL_TRACE() {
    Command::Trace(visitor);
    visitor->Trace(queue_);
    visitor->Trace(reaction_);
  }
  void Run(Element*) override { queue_->Add(reaction_); }

 private:
  Member<CustomElementReactionQueue> queue_;
  Member<CustomElementReaction> reaction_;
};

class TestReaction : public CustomElementReaction {
  WTF_MAKE_NONCOPYABLE(TestReaction);

 public:
  TestReaction(std::initializer_list<Command*> commands)
      : CustomElementReaction(nullptr) {
    // TODO(dominicc): Simply pass the initializer list when
    // HeapVector supports initializer lists like Vector.
    for (auto& command : commands)
      commands_.push_back(command);
  }
  ~TestReaction() override = default;
  DEFINE_INLINE_VIRTUAL_TRACE() {
    CustomElementReaction::Trace(visitor);
    visitor->Trace(commands_);
  }
  void Invoke(Element* element) override {
    for (auto& command : commands_)
      command->Run(element);
  }

 private:
  HeapVector<Member<Command>> commands_;
};

class ResetCustomElementReactionStackForTest final {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(ResetCustomElementReactionStackForTest);

 public:
  ResetCustomElementReactionStackForTest()
      : stack_(new CustomElementReactionStack),
        old_stack_(
            CustomElementReactionStackTestSupport::SetCurrentForTest(stack_)) {}

  ~ResetCustomElementReactionStackForTest() {
    CustomElementReactionStackTestSupport::SetCurrentForTest(old_stack_);
  }

  CustomElementReactionStack& Stack() { return *stack_; }

 private:
  Member<CustomElementReactionStack> stack_;
  Member<CustomElementReactionStack> old_stack_;
};

}  // namespace blink

#endif  // CustomElementReactionTestHelpers_h
