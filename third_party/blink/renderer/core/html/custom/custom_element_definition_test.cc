// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/node.h"  // CustomElementState
#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_descriptor.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_test_helpers.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_test_helpers.h"

namespace blink {

namespace {

class ConstructorFails : public TestCustomElementDefinition {
 public:
  ConstructorFails(const CustomElementDescriptor& descriptor)
      : TestCustomElementDefinition(descriptor) {}
  ~ConstructorFails() override = default;
  bool RunConstructor(Element&) override { return false; }

  DISALLOW_COPY_AND_ASSIGN(ConstructorFails);
};

}  // namespace

TEST(CustomElementDefinitionTest, upgrade_clearsReactionQueueOnFailure) {
  Element& element = *CreateElement("a-a");
  EXPECT_EQ(CustomElementState::kUndefined, element.GetCustomElementState())
      << "sanity check: this element should be ready to upgrade";
  {
    CEReactionsScope reactions;
    HeapVector<Member<Command>>* commands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    commands->push_back(MakeGarbageCollected<Unreached>(
        "upgrade failure should clear the reaction queue"));
    reactions.EnqueueToCurrentQueue(
        element, *MakeGarbageCollected<TestReaction>(commands));
    ConstructorFails definition(CustomElementDescriptor("a-a", "a-a"));
    definition.Upgrade(element);
  }
  EXPECT_EQ(CustomElementState::kFailed, element.GetCustomElementState())
      << "failing to construct should have set the 'failed' element state";
}

TEST(CustomElementDefinitionTest,
     upgrade_clearsReactionQueueOnFailure_backupStack) {
  Element& element = *CreateElement("a-a");
  EXPECT_EQ(CustomElementState::kUndefined, element.GetCustomElementState())
      << "sanity check: this element should be ready to upgrade";
  ResetCustomElementReactionStackForTest reset_reaction_stack;
  HeapVector<Member<Command>>* commands =
      MakeGarbageCollected<HeapVector<Member<Command>>>();
  commands->push_back(MakeGarbageCollected<Unreached>(
      "upgrade failure should clear the reaction queue"));
  reset_reaction_stack.Stack().EnqueueToBackupQueue(
      element, *MakeGarbageCollected<TestReaction>(commands));
  ConstructorFails definition(CustomElementDescriptor("a-a", "a-a"));
  definition.Upgrade(element);
  EXPECT_EQ(CustomElementState::kFailed, element.GetCustomElementState())
      << "failing to construct should have set the 'failed' element state";
}

}  // namespace blink
