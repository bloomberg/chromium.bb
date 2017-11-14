// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/test/begin_frame_args_test.h"
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

class CompositorTest : public testing::Test {
 public:
  CompositorTest() {}
  ~CompositorTest() override {}

  void SetUp() override {
    ui::ContextFactory* context_factory = nullptr;
    ui::ContextFactoryPrivate* context_factory_private = nullptr;
    ui::InitializeContextFactoryForTests(false, &context_factory,
                                         &context_factory_private);

    compositor_.reset(new ui::Compositor(
        context_factory_private->AllocateFrameSinkId(), context_factory,
        context_factory_private, CreateTaskRunner(),
        false /* enable_surface_synchronization */,
        false /* enable_pixel_canvas */));
    compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
  }

  void TearDown() override {
    compositor_.reset();
    ui::TerminateContextFactoryForTests();
  }

  void DestroyCompositor() { compositor_.reset(); }

 protected:
  virtual scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner() = 0;

  ui::Compositor* compositor() { return compositor_.get(); }

 private:
  std::unique_ptr<ui::Compositor> compositor_;

  DISALLOW_COPY_AND_ASSIGN(CompositorTest);
};

// For tests that control time.
class CompositorTestWithMockedTime : public CompositorTest {
 protected:
  scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner() override {
    task_runner_ = new base::TestMockTimeTaskRunner;
    return task_runner_;
  }

  base::TestMockTimeTaskRunner* task_runner() { return task_runner_.get(); }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
};

// For tests that run on a real MessageLoop with real time.
class CompositorTestWithMessageLoop : public CompositorTest {
 protected:
  scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner() override {
    task_runner_ = base::ThreadTaskRunnerHandle::Get();
    return task_runner_;
  }

  base::SequencedTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

class CompositorObserverForLocks : public CompositorObserver {
 public:
  CompositorObserverForLocks() = default;

  void OnCompositingDidCommit(Compositor* compositor) override {}
  void OnCompositingStarted(Compositor* compositor,
                            base::TimeTicks start_time) override {}
  void OnCompositingEnded(Compositor* compositor) override {}
  void OnCompositingLockStateChanged(Compositor* compositor) override {
    changed_ = true;
    locked_ = compositor->IsLocked();
  }
  void OnCompositingChildResizing(Compositor* compositor) override {}

  void OnCompositingShuttingDown(Compositor* compositor) override {}

  bool changed() const { return changed_; }
  bool locked() const { return locked_; }

  void Reset() { changed_ = false; }

 private:
  bool changed_ = false;
  bool locked_ = false;
};

class MockCompositorLockClient : public ui::CompositorLockClient {
 public:
  MOCK_METHOD0(CompositorLockTimedOut, void());
};

}  // namespace

TEST_F(CompositorTestWithMockedTime, LocksAreObserved) {
  std::unique_ptr<CompositorLock> lock;

  CompositorObserverForLocks observer;
  compositor()->AddObserver(&observer);

  EXPECT_FALSE(observer.changed());

  lock = compositor()->GetCompositorLock(nullptr, base::TimeDelta());
  // The observer see that locks changed and that the compositor is locked
  // at the time.
  EXPECT_TRUE(observer.changed());
  EXPECT_TRUE(observer.locked());

  observer.Reset();
  EXPECT_FALSE(observer.changed());

  lock = nullptr;
  // The observer see that locks changed and that the compositor is not locked
  // at the time.
  EXPECT_TRUE(observer.changed());
  EXPECT_FALSE(observer.locked());

  compositor()->RemoveObserver(&observer);
}

TEST_F(CompositorTestWithMockedTime, LocksTimeOut) {
  std::unique_ptr<CompositorLock> lock;

  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(100);

  {
    testing::StrictMock<MockCompositorLockClient> lock_client;
    // This lock has a timeout.
    lock = compositor()->GetCompositorLock(&lock_client, timeout);
    EXPECT_TRUE(compositor()->IsLocked());
    EXPECT_CALL(lock_client, CompositorLockTimedOut()).Times(1);
    task_runner()->FastForwardBy(timeout);
    task_runner()->RunUntilIdle();
    EXPECT_FALSE(compositor()->IsLocked());
  }

  {
    testing::StrictMock<MockCompositorLockClient> lock_client;
    // This lock has no timeout.
    lock = compositor()->GetCompositorLock(&lock_client, base::TimeDelta());
    EXPECT_TRUE(compositor()->IsLocked());
    EXPECT_CALL(lock_client, CompositorLockTimedOut()).Times(0);
    task_runner()->FastForwardBy(timeout);
    task_runner()->RunUntilIdle();
    EXPECT_TRUE(compositor()->IsLocked());
  }
}

TEST_F(CompositorTestWithMockedTime, MultipleLockClients) {
  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;
  testing::StrictMock<MockCompositorLockClient> lock_client2;
  std::unique_ptr<CompositorLock> lock2;

  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(1);
  // Both locks are grabbed from the Compositor with a separate client.
  lock1 = compositor()->GetCompositorLock(&lock_client1, timeout);
  lock2 = compositor()->GetCompositorLock(&lock_client2, timeout);
  EXPECT_TRUE(compositor()->IsLocked());
  // Both clients get notified of timeout.
  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(1);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(1);
  task_runner()->FastForwardBy(timeout);
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(compositor()->IsLocked());
}

TEST_F(CompositorTestWithMockedTime, ExtendingLifeOfLockDoesntUseDeadClient) {
  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;
  testing::StrictMock<MockCompositorLockClient> lock_client2;
  std::unique_ptr<CompositorLock> lock2;

  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(1);

  // One lock is grabbed from the compositor with a client. The other
  // extends its lifetime past that of the first.
  lock1 = compositor()->GetCompositorLock(&lock_client1, timeout);
  EXPECT_TRUE(compositor()->IsLocked());

  // This also locks the compositor and will do so past |lock1| ending.
  lock2 = compositor()->GetCompositorLock(&lock_client2, timeout);
  // |lock1| is destroyed, so it won't timeout but |lock2| will.
  lock1 = nullptr;

  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(0);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(1);
  task_runner()->FastForwardBy(timeout);
  task_runner()->RunUntilIdle();

  EXPECT_FALSE(compositor()->IsLocked());
}

TEST_F(CompositorTestWithMockedTime, AddingLocksDoesNotExtendTimeout) {
  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;
  testing::StrictMock<MockCompositorLockClient> lock_client2;
  std::unique_ptr<CompositorLock> lock2;

  base::TimeDelta timeout1 = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta timeout2 = base::TimeDelta::FromMilliseconds(10);

  // The first lock has a short timeout.
  lock1 = compositor()->GetCompositorLock(&lock_client1, timeout1);
  EXPECT_TRUE(compositor()->IsLocked());

  // The second lock has a longer timeout, but since a lock is active,
  // the first one is used for both.
  lock2 = compositor()->GetCompositorLock(&lock_client2, timeout2);

  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(1);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(1);
  task_runner()->FastForwardBy(timeout1);
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(compositor()->IsLocked());
}

TEST_F(CompositorTestWithMockedTime, AllowAndExtendTimeout) {
  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;
  testing::StrictMock<MockCompositorLockClient> lock_client2;
  std::unique_ptr<CompositorLock> lock2;

  base::TimeDelta timeout1 = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta timeout2 = base::TimeDelta::FromMilliseconds(10);

  // The first lock has a short timeout.
  lock1 = compositor()->GetCompositorLock(&lock_client1, timeout1);
  EXPECT_TRUE(compositor()->IsLocked());

  // Allow locks to extend timeout.
  compositor()->set_allow_locks_to_extend_timeout(true);
  // The second lock has a longer timeout, so the second one is used for both.
  lock2 = compositor()->GetCompositorLock(&lock_client2, timeout2);
  compositor()->set_allow_locks_to_extend_timeout(false);

  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(0);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(0);
  task_runner()->FastForwardBy(timeout1);
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(compositor()->IsLocked());

  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(1);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(1);
  task_runner()->FastForwardBy(timeout2 - timeout1);
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(compositor()->IsLocked());
}

TEST_F(CompositorTestWithMockedTime, ExtendingTimeoutStartingCreatedTime) {
  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;
  testing::StrictMock<MockCompositorLockClient> lock_client2;
  std::unique_ptr<CompositorLock> lock2;

  base::TimeDelta timeout1 = base::TimeDelta::FromMilliseconds(5);
  base::TimeDelta timeout2 = base::TimeDelta::FromMilliseconds(10);

  // The first lock has a short timeout.
  lock1 = compositor()->GetCompositorLock(&lock_client1, timeout1);
  EXPECT_TRUE(compositor()->IsLocked());

  base::TimeDelta time_elapse = base::TimeDelta::FromMilliseconds(1);
  task_runner()->FastForwardBy(time_elapse);
  task_runner()->RunUntilIdle();

  // Allow locks to extend timeout.
  compositor()->set_allow_locks_to_extend_timeout(true);
  // The second lock has a longer timeout, so the second one is used for both
  // and start from the time second lock created.
  lock2 = compositor()->GetCompositorLock(&lock_client2, timeout2);
  compositor()->set_allow_locks_to_extend_timeout(false);

  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(0);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(0);
  task_runner()->FastForwardBy(timeout1 - time_elapse);
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(compositor()->IsLocked());

  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(1);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(1);
  task_runner()->FastForwardBy(timeout2 - (timeout1 - time_elapse));
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(compositor()->IsLocked());
}

TEST_F(CompositorTestWithMockedTime, AllowButNotExtendTimeout) {
  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;
  testing::StrictMock<MockCompositorLockClient> lock_client2;
  std::unique_ptr<CompositorLock> lock2;

  base::TimeDelta timeout1 = base::TimeDelta::FromMilliseconds(10);
  base::TimeDelta timeout2 = base::TimeDelta::FromMilliseconds(1);

  // The first lock has a longer timeout.
  lock1 = compositor()->GetCompositorLock(&lock_client1, timeout1);
  EXPECT_TRUE(compositor()->IsLocked());

  // Allow locks to extend timeout.
  compositor()->set_allow_locks_to_extend_timeout(true);
  // The second lock has a short timeout, so the first one is used for both.
  lock2 = compositor()->GetCompositorLock(&lock_client2, timeout2);
  compositor()->set_allow_locks_to_extend_timeout(false);

  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(0);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(0);
  task_runner()->FastForwardBy(timeout2);
  task_runner()->RunUntilIdle();
  EXPECT_TRUE(compositor()->IsLocked());

  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(1);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(1);
  task_runner()->FastForwardBy(timeout1 - timeout2);
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(compositor()->IsLocked());
}

TEST_F(CompositorTestWithMockedTime, AllowingExtendDoesNotUseDeadClient) {
  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;
  testing::StrictMock<MockCompositorLockClient> lock_client2;
  std::unique_ptr<CompositorLock> lock2;

  base::TimeDelta timeout1 = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta timeout2 = base::TimeDelta::FromMilliseconds(10);

  lock1 = compositor()->GetCompositorLock(&lock_client1, timeout1);
  EXPECT_TRUE(compositor()->IsLocked());
  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(1);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(0);
  task_runner()->FastForwardBy(timeout1);
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(compositor()->IsLocked());

  // Allow locks to extend timeout.
  compositor()->set_allow_locks_to_extend_timeout(true);
  // |lock1| is timed out already. The second lock can timeout on its own.
  lock2 = compositor()->GetCompositorLock(&lock_client2, timeout2);
  compositor()->set_allow_locks_to_extend_timeout(false);
  EXPECT_TRUE(compositor()->IsLocked());
  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(0);
  EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(1);
  task_runner()->FastForwardBy(timeout2);
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(compositor()->IsLocked());
}

TEST_F(CompositorTestWithMockedTime, LockIsDestroyedDoesntTimeout) {
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(1);

  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;
  lock1 = compositor()->GetCompositorLock(&lock_client1, timeout);
  EXPECT_TRUE(compositor()->IsLocked());
  // The CompositorLockClient is destroyed when |lock1| is released.
  lock1 = nullptr;
  // The client isn't called as a result.
  EXPECT_CALL(lock_client1, CompositorLockTimedOut()).Times(0);
  task_runner()->FastForwardBy(timeout);
  task_runner()->RunUntilIdle();
  EXPECT_FALSE(compositor()->IsLocked());
}

TEST_F(CompositorTestWithMockedTime, TimeoutEndsWhenLockEnds) {
  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;
  testing::StrictMock<MockCompositorLockClient> lock_client2;
  std::unique_ptr<CompositorLock> lock2;

  base::TimeDelta timeout1 = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta timeout2 = base::TimeDelta::FromMilliseconds(10);

  // The first lock has a short timeout.
  lock1 = compositor()->GetCompositorLock(&lock_client1, timeout1);
  EXPECT_TRUE(compositor()->IsLocked());
  // But the first lock is ended before timeout.
  lock1 = nullptr;
  EXPECT_FALSE(compositor()->IsLocked());

  // The second lock has a longer timeout, and it should use that timeout,
  // since the first lock is done.
  lock2 = compositor()->GetCompositorLock(&lock_client2, timeout2);
  EXPECT_TRUE(compositor()->IsLocked());

  {
    // The second lock doesn't timeout from the first lock which has ended.
    EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(0);
    task_runner()->FastForwardBy(timeout1);
    task_runner()->RunUntilIdle();
  }

  {
    // The second lock can still timeout on its own though.
    EXPECT_CALL(lock_client2, CompositorLockTimedOut()).Times(1);
    task_runner()->FastForwardBy(timeout2 - timeout1);
    task_runner()->RunUntilIdle();
  }

  EXPECT_FALSE(compositor()->IsLocked());
}

TEST_F(CompositorTestWithMockedTime, CompositorLockOutlivesCompositor) {
  testing::StrictMock<MockCompositorLockClient> lock_client1;
  std::unique_ptr<CompositorLock> lock1;

  lock1 = compositor()->GetCompositorLock(&lock_client1, base::TimeDelta());
  // The compositor is destroyed before the lock.
  DestroyCompositor();
  // This doesn't crash.
  lock1 = nullptr;
}

TEST_F(CompositorTestWithMockedTime,
       ReleaseWidgetWithOutputSurfaceNeverCreated) {
  compositor()->SetVisible(false);
  EXPECT_EQ(gfx::kNullAcceleratedWidget,
            compositor()->ReleaseAcceleratedWidget());
  compositor()->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
  compositor()->SetVisible(true);
}

#if defined(OS_WIN)
// TODO(crbug.com/608436): Flaky on windows trybots
#define MAYBE_CreateAndReleaseOutputSurface \
  DISABLED_CreateAndReleaseOutputSurface
#else
#define MAYBE_CreateAndReleaseOutputSurface CreateAndReleaseOutputSurface
#endif
TEST_F(CompositorTestWithMessageLoop, MAYBE_CreateAndReleaseOutputSurface) {
  std::unique_ptr<Layer> root_layer(new Layer(ui::LAYER_SOLID_COLOR));
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
