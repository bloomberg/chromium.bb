// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/custom/CustomElementDefinition.h"

#include "core/dom/Node.h"  // CustomElementState
#include "core/html/custom/CEReactionsScope.h"
#include "core/html/custom/CustomElementDescriptor.h"
#include "core/html/custom/CustomElementReactionTestHelpers.h"
#include "core/html/custom/CustomElementTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class ConstructorFails : public TestCustomElementDefinition {
  WTF_MAKE_NONCOPYABLE(ConstructorFails);

 public:
  ConstructorFails(const CustomElementDescriptor& descriptor)
      : TestCustomElementDefinition(descriptor) {}
  ~ConstructorFails() override = default;
  bool RunConstructor(Element*) override { return false; }
};

}  // namespace

TEST(CustomElementDefinitionTest, upgrade_clearsReactionQueueOnFailure) {
  Element* element = CreateElement("a-a");
  EXPECT_EQ(CustomElementState::kUndefined, element->GetCustomElementState())
      << "sanity check: this element should be ready to upgrade";
  {
    CEReactionsScope reactions;
    reactions.EnqueueToCurrentQueue(
        element, new TestReaction({new Unreached(
                     "upgrade failure should clear the reaction queue")}));
    ConstructorFails definition(CustomElementDescriptor("a-a", "a-a"));
    definition.Upgrade(element);
  }
  EXPECT_EQ(CustomElementState::kFailed, element->GetCustomElementState())
      << "failing to construct should have set the 'failed' element state";
}

TEST(CustomElementDefinitionTest,
     upgrade_clearsReactionQueueOnFailure_backupStack) {
  Element* element = CreateElement("a-a");
  EXPECT_EQ(CustomElementState::kUndefined, element->GetCustomElementState())
      << "sanity check: this element should be ready to upgrade";
  ResetCustomElementReactionStackForTest reset_reaction_stack;
  reset_reaction_stack.Stack().EnqueueToBackupQueue(
      element, new TestReaction({new Unreached(
                   "upgrade failure should clear the reaction queue")}));
  ConstructorFails definition(CustomElementDescriptor("a-a", "a-a"));
  definition.Upgrade(element);
  EXPECT_EQ(CustomElementState::kFailed, element->GetCustomElementState())
      << "failing to construct should have set the 'failed' element state";
}

}  // namespace blink
