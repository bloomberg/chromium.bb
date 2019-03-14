// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/memory_purge_manager.h"

#include "base/memory/memory_pressure_listener.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_lifecycle_state.h"

namespace blink {

namespace {

constexpr base::TimeDelta kDelayForPurgeAfterFreeze =
    base::TimeDelta::FromMinutes(1);

class MemoryPurgeManagerTest : public testing::Test {
 public:
  MemoryPurgeManagerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI_MOCK_TIME,
            base::test::ScopedTaskEnvironment::NowSource::
                MAIN_THREAD_MOCK_TIME),
        observed_memory_pressure_(false) {}

  void SetUp() override {
    memory_pressure_listener_ =
        std::make_unique<base::MemoryPressureListener>(base::BindRepeating(
            &MemoryPurgeManagerTest::OnMemoryPressure, base::Unretained(this)));
    base::MemoryPressureListener::SetNotificationsSuppressed(false);

    // Set an initial delay to ensure that the first call to TimeTicks::Now()
    // before incrementing the counter does not return a null value.
    FastForwardBy(base::TimeDelta::FromSeconds(1));
  }

  void TearDown() override {
    memory_pressure_listener_.reset();
    scoped_task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  void SetupDelayedPurgeAfterFreezeExperiment() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kFreezePurgeMemoryAllPagesFrozen,
        {{"delay-in-minutes",
          base::IntToString(kDelayForPurgeAfterFreeze.InMinutes())}});
  }

  void ExpectMemoryPressure(
      base::TimeDelta delay = base::TimeDelta::FromMinutes(0)) {
    FastForwardBy(delay);
    EXPECT_TRUE(observed_memory_pressure_);
    observed_memory_pressure_ = false;
  }

  void ExpectNoMemoryPressure(
      base::TimeDelta delay = base::TimeDelta::FromMinutes(0)) {
    FastForwardBy(delay);
    EXPECT_FALSE(observed_memory_pressure_);
  }

  void FastForwardBy(base::TimeDelta delta) {
    scoped_task_environment_.FastForwardBy(delta);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  bool observed_memory_pressure_;

  MemoryPurgeManager memory_purge_manager_;

 private:
  void OnMemoryPressure(base::MemoryPressureListener::MemoryPressureLevel) {
    observed_memory_pressure_ = true;
  }

  DISALLOW_COPY_AND_ASSIGN(MemoryPurgeManagerTest);
};

// Verify that OnPageFrozen() triggers a memory pressure notification in a
// backgrounded renderer.
TEST_F(MemoryPurgeManagerTest, PageFrozenInBackgroundedRenderer) {
  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);
  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  ExpectMemoryPressure();
}

// Verify that OnPageFrozen() does not trigger a memory pressure notification in
// a foregrounded renderer.
TEST_F(MemoryPurgeManagerTest, PageFrozenInForegroundedRenderer) {
  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);
  memory_purge_manager_.SetRendererBackgrounded(false);
  memory_purge_manager_.OnPageFrozen();
  ExpectNoMemoryPressure();
}

TEST_F(MemoryPurgeManagerTest, PageResumedUndoMemoryPressureSuppression) {
  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  ExpectMemoryPressure();
  EXPECT_TRUE(base::MemoryPressureListener::AreNotificationsSuppressed());
  memory_purge_manager_.OnPageResumed();
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kActive);
}

TEST_F(MemoryPurgeManagerTest, PageFrozenPurgeMemoryAllPagesFrozenDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {} /* enabled */,
      {features::kFreezePurgeMemoryAllPagesFrozen} /* disabled */);

  memory_purge_manager_.SetRendererBackgrounded(true);

  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);
  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);
  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);

  memory_purge_manager_.OnPageFrozen();
  ExpectMemoryPressure();
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageFrozen();
  ExpectMemoryPressure();
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageFrozen();
  ExpectMemoryPressure();
  EXPECT_TRUE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageResumed();
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kActive);
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kActive);
  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kFrozen);
  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kFrozen);
}

TEST_F(MemoryPurgeManagerTest, PageFrozenPurgeMemoryAllPagesFrozenEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kFreezePurgeMemoryAllPagesFrozen} /* enabled */,
      {} /* disabled */);

  memory_purge_manager_.SetRendererBackgrounded(true);

  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);
  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);
  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);

  memory_purge_manager_.OnPageFrozen();
  ExpectNoMemoryPressure();
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageFrozen();
  ExpectNoMemoryPressure();
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageFrozen();
  ExpectMemoryPressure();
  EXPECT_TRUE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageResumed();
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kActive);
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kActive);
  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kFrozen);
  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kFrozen);
}

TEST_F(MemoryPurgeManagerTest, MemoryPurgeWithDelay) {
  SetupDelayedPurgeAfterFreezeExperiment();

  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();

  // The memory pressure notification should not occur immediately
  ExpectNoMemoryPressure();

  // The memory pressure notification should occur after 1 minute
  ExpectMemoryPressure(kDelayForPurgeAfterFreeze);

  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kFrozen);
}

TEST_F(MemoryPurgeManagerTest, CancelMemoryPurgeWithDelay) {
  SetupDelayedPurgeAfterFreezeExperiment();

  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(base::TimeDelta::FromSeconds(40));
  ExpectNoMemoryPressure();

  // If the page is resumed before the memory purge timer expires, the purge
  // should be cancelled.
  memory_purge_manager_.OnPageResumed();
  ExpectNoMemoryPressure(kDelayForPurgeAfterFreeze);

  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kActive);
}

TEST_F(MemoryPurgeManagerTest, MemoryPurgeWithDelayNewActivePageCreated) {
  SetupDelayedPurgeAfterFreezeExperiment();

  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(base::TimeDelta::FromSeconds(40));
  ExpectNoMemoryPressure();

  // All pages are no longer frozen, the memory purge should be cancelled.
  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);
  ExpectNoMemoryPressure(kDelayForPurgeAfterFreeze);

  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kFrozen);
  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kActive);
}

TEST_F(MemoryPurgeManagerTest, MemoryPurgeWithDelayNewFrozenPageCreated) {
  SetupDelayedPurgeAfterFreezeExperiment();

  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(base::TimeDelta::FromSeconds(40));
  ExpectNoMemoryPressure();

  // All pages are still frozen and the memory purge should occur.
  memory_purge_manager_.OnPageCreated(PageLifecycleState::kFrozen);
  ExpectMemoryPressure(kDelayForPurgeAfterFreeze);

  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kFrozen);
  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kFrozen);
}

}  // namespace

}  // namespace blink
