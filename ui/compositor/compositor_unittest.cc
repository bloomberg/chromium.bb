// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_simple_task_runner.h"
#include "cc/output/begin_frame_args.h"
#include "cc/test/begin_frame_args_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/context_factories_for_test.h"

using testing::Mock;
using testing::_;

namespace ui {
namespace {

class MockCompositorBeginFrameObserver : public CompositorBeginFrameObserver {
 public:
  MOCK_METHOD1(OnSendBeginFrame, void(const cc::BeginFrameArgs&));
};

// Test fixture for tests that require a ui::Compositor with a real task
// runner.
class CompositorTest : public testing::Test {
 public:
  CompositorTest() {}
  ~CompositorTest() override {}

  void SetUp() override {
    task_runner_ = new base::TestSimpleTaskRunner;

    ui::ContextFactory* context_factory =
        ui::InitializeContextFactoryForTests(false);

    compositor_.reset(new ui::Compositor(gfx::kNullAcceleratedWidget,
                                         context_factory, task_runner_));
  }
  void TearDown() override {
    compositor_.reset();
    ui::TerminateContextFactoryForTests();
  }

 protected:
  base::TestSimpleTaskRunner* task_runner() { return task_runner_.get(); }
  ui::Compositor* compositor() { return compositor_.get(); }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_ptr<ui::Compositor> compositor_;

  DISALLOW_COPY_AND_ASSIGN(CompositorTest);
};

}  // namespace

TEST_F(CompositorTest, LocksTimeOut) {
  scoped_refptr<ui::CompositorLock> lock;

  // Ensure that the lock times out by default.
  lock = compositor()->GetCompositorLock();
  EXPECT_TRUE(compositor()->IsLocked());
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(compositor()->IsLocked());

  // Ensure that the lock does not time out when set.
  compositor()->SetLocksWillTimeOut(false);
  lock = compositor()->GetCompositorLock();
  EXPECT_TRUE(compositor()->IsLocked());
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(compositor()->IsLocked());
}

TEST_F(CompositorTest, AddAndRemoveBeginFrameObserver) {
  cc::BeginFrameArgs args =
    cc::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE,
                                       base::TimeTicks::FromInternalValue(33));

  // Simulate to trigger new BeginFrame by using |args|.
  compositor()->SendBeginFramesToChildren(args);

  // When |missed_begin_frame_args_| is sent, its type is set to MISSED.
  cc::BeginFrameArgs expected_args(args);
  expected_args.type = cc::BeginFrameArgs::MISSED;

  MockCompositorBeginFrameObserver test_observer;
  MockCompositorBeginFrameObserver test_observer2;
  EXPECT_CALL(test_observer, OnSendBeginFrame(expected_args));
  EXPECT_CALL(test_observer2, OnSendBeginFrame(expected_args));

  // When new observer is added, Compositor immediately calls OnSendBeginFrame
  // with |missed_begin_frame_args_|.
  compositor()->AddBeginFrameObserver(&test_observer);
  compositor()->AddBeginFrameObserver(&test_observer2);
  Mock::VerifyAndClearExpectations(&test_observer);
  Mock::VerifyAndClearExpectations(&test_observer2);

  // When |test_observer2| is removed and added again, it will be called again.
  EXPECT_CALL(test_observer2, OnSendBeginFrame(expected_args));
  compositor()->RemoveBeginFrameObserver(&test_observer2);
  compositor()->AddBeginFrameObserver(&test_observer2);
  Mock::VerifyAndClearExpectations(&test_observer2);

  // When all observer is removed, |missed_begin_frame_args_| is invalidated.
  // So, it is not used for newly added observer.
  EXPECT_CALL(test_observer2, OnSendBeginFrame(_)).Times(0);
  compositor()->RemoveBeginFrameObserver(&test_observer);
  compositor()->RemoveBeginFrameObserver(&test_observer2);
  compositor()->AddBeginFrameObserver(&test_observer2);
  Mock::VerifyAndClearExpectations(&test_observer2);

  compositor()->RemoveBeginFrameObserver(&test_observer2);
}

}  // namespace ui
