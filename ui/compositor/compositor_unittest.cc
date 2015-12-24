// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "cc/output/begin_frame_args.h"
#include "cc/test/begin_frame_args_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/compositor/test/draw_waiter_for_test.h"

using testing::Mock;
using testing::_;

namespace ui {
namespace {

ACTION_P2(RemoveObserver, compositor, observer) {
  compositor->RemoveBeginFrameObserver(observer);
}

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
    task_runner_ = base::ThreadTaskRunnerHandle::Get();

    ui::ContextFactory* context_factory =
        ui::InitializeContextFactoryForTests(false);

    compositor_.reset(new ui::Compositor(context_factory, task_runner_));
    compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
  }
  void TearDown() override {
    compositor_.reset();
    ui::TerminateContextFactoryForTests();
  }

 protected:
  base::SingleThreadTaskRunner* task_runner() { return task_runner_.get(); }
  ui::Compositor* compositor() { return compositor_.get(); }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_ptr<ui::Compositor> compositor_;

  DISALLOW_COPY_AND_ASSIGN(CompositorTest);
};

}  // namespace

TEST_F(CompositorTest, LocksTimeOut) {
  scoped_refptr<ui::CompositorLock> lock;
  {
    base::RunLoop run_loop;
    // Ensure that the lock times out by default.
    lock = compositor()->GetCompositorLock();
    EXPECT_TRUE(compositor()->IsLocked());
    task_runner()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(kCompositorLockTimeoutMs));
    run_loop.Run();
    EXPECT_FALSE(compositor()->IsLocked());
  }

  {
    base::RunLoop run_loop;
    // Ensure that the lock does not time out when set.
    compositor()->SetLocksWillTimeOut(false);
    lock = compositor()->GetCompositorLock();
    EXPECT_TRUE(compositor()->IsLocked());
    task_runner()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(kCompositorLockTimeoutMs));
    run_loop.Run();
    EXPECT_TRUE(compositor()->IsLocked());
  }
}

TEST_F(CompositorTest, AddAndRemoveBeginFrameObserver) {
  cc::BeginFrameArgs args =
    cc::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE,
                                       base::TimeTicks::FromInternalValue(33));

  MockCompositorBeginFrameObserver test_observer;
  MockCompositorBeginFrameObserver test_observer2;

  // Add a single observer.
  compositor()->AddBeginFrameObserver(&test_observer);
  Mock::VerifyAndClearExpectations(&test_observer);

  // When |missed_begin_frame_args_| is sent, its type is set to MISSED.
  cc::BeginFrameArgs expected_args(args);
  cc::BeginFrameArgs expected_missed_args(args);
  expected_missed_args.type = cc::BeginFrameArgs::MISSED;

  // Simulate to trigger new BeginFrame by using |args|.
  EXPECT_CALL(test_observer, OnSendBeginFrame(expected_args));
  compositor()->SendBeginFramesToChildren(args);
  Mock::VerifyAndClearExpectations(&test_observer);

  // When new observer is added, Compositor immediately calls OnSendBeginFrame
  // with |missed_begin_frame_args_|.
  EXPECT_CALL(test_observer2, OnSendBeginFrame(expected_missed_args));
  compositor()->AddBeginFrameObserver(&test_observer2);
  Mock::VerifyAndClearExpectations(&test_observer);
  Mock::VerifyAndClearExpectations(&test_observer2);

  // When |test_observer2| is removed and added again, it will be called again.
  EXPECT_CALL(test_observer2, OnSendBeginFrame(expected_missed_args));
  compositor()->RemoveBeginFrameObserver(&test_observer2);
  compositor()->AddBeginFrameObserver(&test_observer2);
  Mock::VerifyAndClearExpectations(&test_observer2);

  // When all observer is removed, |missed_begin_frame_args_| is invalidated.
  // So, it is not used for newly added observer.
  EXPECT_CALL(test_observer2, OnSendBeginFrame(_)).Times(0);
  compositor()->RemoveBeginFrameObserver(&test_observer);
  compositor()->RemoveBeginFrameObserver(&test_observer2);
  compositor()->SendBeginFramesToChildren(args);
  compositor()->AddBeginFrameObserver(&test_observer2);
  Mock::VerifyAndClearExpectations(&test_observer2);

  compositor()->RemoveBeginFrameObserver(&test_observer2);
}

TEST_F(CompositorTest, RemoveBeginFrameObserverWhileSendingBeginFrame) {
  cc::BeginFrameArgs args = cc::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, base::TimeTicks::FromInternalValue(33));

  cc::BeginFrameArgs expected_args(args);
  cc::BeginFrameArgs expected_missed_args(args);
  expected_missed_args.type = cc::BeginFrameArgs::MISSED;

  // Add both observers, and simulate removal of |test_observer2| during
  // BeginFrame dispatch (implicitly triggered when the observer is added).
  MockCompositorBeginFrameObserver test_observer;
  MockCompositorBeginFrameObserver test_observer2;
  EXPECT_CALL(test_observer, OnSendBeginFrame(expected_args));
  EXPECT_CALL(test_observer2, OnSendBeginFrame(expected_missed_args))
      .WillOnce(RemoveObserver(compositor(), &test_observer2));

  // When a new observer is added, Compositor immediately calls OnSendBeginFrame
  // with |missed_begin_frame_args_|.
  compositor()->AddBeginFrameObserver(&test_observer);
  compositor()->SendBeginFramesToChildren(args);
  compositor()->AddBeginFrameObserver(&test_observer2);
  Mock::VerifyAndClearExpectations(&test_observer);
  Mock::VerifyAndClearExpectations(&test_observer2);

  // |test_observer2| was removed during the previous implicit BeginFrame
  // dispatch, and should not get the new frame.
  expected_args.type = cc::BeginFrameArgs::NORMAL;
  EXPECT_CALL(test_observer, OnSendBeginFrame(expected_args));
  EXPECT_CALL(test_observer2, OnSendBeginFrame(_)).Times(0);
  compositor()->SendBeginFramesToChildren(args);
  Mock::VerifyAndClearExpectations(&test_observer);
  Mock::VerifyAndClearExpectations(&test_observer2);

  // Now remove |test_observer| during explicit BeginFrame dispatch.
  EXPECT_CALL(test_observer, OnSendBeginFrame(expected_args))
      .WillOnce(RemoveObserver(compositor(), &test_observer));
  EXPECT_CALL(test_observer2, OnSendBeginFrame(_)).Times(0);
  compositor()->SendBeginFramesToChildren(args);
  Mock::VerifyAndClearExpectations(&test_observer);
  Mock::VerifyAndClearExpectations(&test_observer2);

  // No observers should get the new frame.
  EXPECT_CALL(test_observer, OnSendBeginFrame(_)).Times(0);
  EXPECT_CALL(test_observer2, OnSendBeginFrame(_)).Times(0);
  compositor()->SendBeginFramesToChildren(args);
  Mock::VerifyAndClearExpectations(&test_observer);
  Mock::VerifyAndClearExpectations(&test_observer2);

  // Adding a new observer should not trigger a missed frame, as the
  // previous frame had no observers.
  EXPECT_CALL(test_observer, OnSendBeginFrame(_)).Times(0);
  compositor()->AddBeginFrameObserver(&test_observer);
  compositor()->RemoveBeginFrameObserver(&test_observer);
  Mock::VerifyAndClearExpectations(&test_observer);
}

TEST_F(CompositorTest, ReleaseWidgetWithOutputSurfaceNeverCreated) {
  compositor()->SetVisible(false);
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            compositor()->ReleaseAcceleratedWidget());
  compositor()->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
  compositor()->SetVisible(true);
}

TEST_F(CompositorTest, CreateAndReleaseOutputSurface) {
  scoped_ptr<Layer> root_layer(new Layer(ui::LAYER_SOLID_COLOR));
  root_layer->SetBounds(gfx::Rect(10, 10));
  compositor()->SetRootLayer(root_layer.get());
  compositor()->SetScaleAndSize(1.0f, gfx::Size(10, 10));
  DCHECK(compositor()->IsVisible());
  compositor()->ScheduleDraw();
  DrawWaiterForTest::WaitForCompositingEnded(compositor());
  compositor()->SetVisible(false);
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            compositor()->ReleaseAcceleratedWidget());
  compositor()->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
  compositor()->SetVisible(true);
  compositor()->ScheduleDraw();
  DrawWaiterForTest::WaitForCompositingEnded(compositor());
  compositor()->SetRootLayer(nullptr);
}

}  // namespace ui
