// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/context_factories_for_test.h"

namespace ui {
namespace {

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

}  // namespace ui
