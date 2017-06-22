// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CompositorMutatorClient.h"

#include <memory>
#include "base/callback.h"
#include "platform/graphics/CompositorMutation.h"
#include "platform/graphics/CompositorMutationsTarget.h"
#include "platform/graphics/CompositorMutator.h"
#include "platform/wtf/PtrUtil.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace blink {
namespace {

class StubCompositorMutator : public CompositorMutator {
 public:
  StubCompositorMutator() {}

  bool Mutate(double monotonic_time_now) override { return false; }
};

class MockCompositoMutationsTarget : public CompositorMutationsTarget {
 public:
  MOCK_METHOD1(ApplyMutations, void(CompositorMutations*));
};

TEST(CompositorMutatorClient, CallbackForNonNullMutationsShouldApply) {
  MockCompositoMutationsTarget target;

  CompositorMutatorClient client(new StubCompositorMutator, &target);
  std::unique_ptr<CompositorMutations> mutations =
      WTF::MakeUnique<CompositorMutations>();
  client.SetMutationsForTesting(std::move(mutations));

  EXPECT_CALL(target, ApplyMutations(_));
  client.TakeMutations().Run();
}

TEST(CompositorMutatorClient, CallbackForNullMutationsShouldBeNoop) {
  MockCompositoMutationsTarget target;
  CompositorMutatorClient client(new StubCompositorMutator, &target);

  EXPECT_CALL(target, ApplyMutations(_)).Times(0);
  EXPECT_TRUE(client.TakeMutations().is_null());
}

}  // namespace
}  // namespace blink
