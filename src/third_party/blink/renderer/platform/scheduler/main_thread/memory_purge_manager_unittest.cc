// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/memory_purge_manager.h"

#include "base/memory/memory_pressure_listener.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
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
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        memory_purge_manager_(task_environment_.GetMainThreadTaskRunner()) {}

  void SetUp() override {
    memory_pressure_listener_ =
        std::make_unique<base::MemoryPressureListener>(base::BindRepeating(
            &MemoryPurgeManagerTest::OnMemoryPressure, base::Unretained(this)));
    base::MemoryPressureListener::SetNotificationsSuppressed(false);
  }

  void TearDown() override {
    memory_pressure_listener_.reset();
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  void SetupDelayedPurgeAfterFreezeExperiment() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kFreezePurgeMemoryAllPagesFrozen,
          {{"delay-in-minutes",
            base::NumberToString(kDelayForPurgeAfterFreeze.InMinutes())}}}},
        {features::kPurgeRendererMemoryWhenBackgrounded});
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  unsigned MemoryPressureCount() const { return memory_pressure_count_; }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  MemoryPurgeManager memory_purge_manager_;

  unsigned memory_pressure_count_ = 0;

 private:
  void OnMemoryPressure(base::MemoryPressureListener::MemoryPressureLevel) {
    memory_pressure_count_++;
  }

  DISALLOW_COPY_AND_ASSIGN(MemoryPurgeManagerTest);
};

// Verify that OnPageFrozen() triggers a memory pressure notification in a
// backgrounded renderer.
TEST_F(MemoryPurgeManagerTest, PageFrozenInBackgroundedRenderer) {
  scoped_feature_list_.InitWithFeatures(
      {} /* enabled */,
      {features::kPurgeRendererMemoryWhenBackgrounded} /* disabled */);

  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);
  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(base::TimeDelta::FromMinutes(0));
  EXPECT_EQ(1U, MemoryPressureCount());
}

// Verify that OnPageFrozen() does not trigger a memory pressure notification in
// a foregrounded renderer.
TEST_F(MemoryPurgeManagerTest, PageFrozenInForegroundedRenderer) {
  scoped_feature_list_.InitWithFeatures(
      {} /* enabled */,
      {features::kPurgeRendererMemoryWhenBackgrounded} /* disabled */);

  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);
  memory_purge_manager_.SetRendererBackgrounded(false);
  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(base::TimeDelta::FromMinutes(0));
  EXPECT_EQ(0U, MemoryPressureCount());
}

TEST_F(MemoryPurgeManagerTest, PageResumedUndoMemoryPressureSuppression) {
  scoped_feature_list_.InitWithFeatures(
      {} /* enabled */,
      {features::kPurgeRendererMemoryWhenBackgrounded} /* disabled */);

  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(base::TimeDelta::FromMinutes(0));
  EXPECT_EQ(1U, MemoryPressureCount());

  EXPECT_TRUE(base::MemoryPressureListener::AreNotificationsSuppressed());
  memory_purge_manager_.OnPageResumed();
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kActive);
}

TEST_F(MemoryPurgeManagerTest, PageFrozenPurgeMemoryAllPagesFrozenDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {} /* enabled */,
      {features::kFreezePurgeMemoryAllPagesFrozen,
       features::kPurgeRendererMemoryWhenBackgrounded} /* disabled */);

  memory_purge_manager_.SetRendererBackgrounded(true);

  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);
  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);
  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);

  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(base::TimeDelta::FromMinutes(0));
  EXPECT_EQ(1U, MemoryPressureCount());
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(base::TimeDelta::FromMinutes(0));
  EXPECT_EQ(2U, MemoryPressureCount());
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(base::TimeDelta::FromMinutes(0));
  EXPECT_EQ(3U, MemoryPressureCount());
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
      {features::kPurgeRendererMemoryWhenBackgrounded} /* disabled */);

  memory_purge_manager_.SetRendererBackgrounded(true);

  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);
  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);
  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);

  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(base::TimeDelta::FromMinutes(0));
  EXPECT_EQ(0U, MemoryPressureCount());
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(base::TimeDelta::FromMinutes(0));
  EXPECT_EQ(0U, MemoryPressureCount());
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(base::TimeDelta::FromMinutes(0));
  EXPECT_EQ(1U, MemoryPressureCount());
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
  FastForwardBy(base::TimeDelta::FromMinutes(0));
  EXPECT_EQ(0U, MemoryPressureCount());

  // The memory pressure notification should occur after 1 minute
  FastForwardBy(kDelayForPurgeAfterFreeze);
  EXPECT_EQ(1U, MemoryPressureCount());

  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kFrozen);
}

TEST_F(MemoryPurgeManagerTest, CancelMemoryPurgeWithDelay) {
  SetupDelayedPurgeAfterFreezeExperiment();

  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(base::TimeDelta::FromSeconds(40));
  EXPECT_EQ(0U, MemoryPressureCount());

  // If the page is resumed before the memory purge timer expires, the purge
  // should be cancelled.
  memory_purge_manager_.OnPageResumed();
  FastForwardBy(base::TimeDelta::FromMinutes(0));
  EXPECT_EQ(0U, MemoryPressureCount());

  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kActive);
}

TEST_F(MemoryPurgeManagerTest, MemoryPurgeWithDelayNewActivePageCreated) {
  SetupDelayedPurgeAfterFreezeExperiment();

  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(base::TimeDelta::FromSeconds(40));
  EXPECT_EQ(0U, MemoryPressureCount());

  // All pages are no longer frozen, the memory purge should be cancelled.
  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);
  FastForwardBy(kDelayForPurgeAfterFreeze);
  EXPECT_EQ(0U, MemoryPressureCount());

  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kFrozen);
  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kActive);
}

TEST_F(MemoryPurgeManagerTest, MemoryPurgeWithDelayNewFrozenPageCreated) {
  SetupDelayedPurgeAfterFreezeExperiment();

  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(base::TimeDelta::FromSeconds(40));
  EXPECT_EQ(0U, MemoryPressureCount());

  // All pages are still frozen and the memory purge should occur.
  memory_purge_manager_.OnPageCreated(PageLifecycleState::kFrozen);
  FastForwardBy(kDelayForPurgeAfterFreeze);
  EXPECT_EQ(1U, MemoryPressureCount());

  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kFrozen);
  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kFrozen);
}

TEST_F(MemoryPurgeManagerTest, PurgeRendererMemoryWhenBackgroundedEnabled) {
  scoped_feature_list_.InitWithFeatures(
      {features::kPurgeRendererMemoryWhenBackgrounded} /* enabled */,
      {} /* disabled */);

  memory_purge_manager_.SetRendererBackgrounded(true);
  FastForwardBy(base::TimeDelta::FromMinutes(
      MemoryPurgeManager::kDefaultMaxTimeToPurgeAfterBackgrounded));
  // No page, no memory pressure.
  EXPECT_EQ(0U, MemoryPressureCount());
}

TEST_F(MemoryPurgeManagerTest, PurgeRendererMemoryWhenBackgroundedDisabled) {
  scoped_feature_list_.InitWithFeatures(
      {} /* enabled */,
      {features::kPurgeRendererMemoryWhenBackgrounded} /* disabled */);

  memory_purge_manager_.SetRendererBackgrounded(true);
  FastForwardBy(base::TimeDelta::Max());
  EXPECT_EQ(0U, MemoryPressureCount());
}

TEST_F(MemoryPurgeManagerTest,
       PurgeRendererMemoryWhenBackgroundedEnabledForegroundedBeforePurge) {
  scoped_feature_list_.InitWithFeatures(
      {features::kPurgeRendererMemoryWhenBackgrounded} /* enabled */,
      {} /* disabled */);

  memory_purge_manager_.SetRendererBackgrounded(true);
  FastForwardBy(base::TimeDelta::FromSeconds(30));
  EXPECT_EQ(0U, MemoryPressureCount());

  memory_purge_manager_.SetRendererBackgrounded(false);
  FastForwardBy(base::TimeDelta::Max());
  EXPECT_EQ(0U, MemoryPressureCount());
}

TEST_F(MemoryPurgeManagerTest, PageFrozenAndResumedWhileBackgrounded) {
  constexpr base::TimeDelta kFreezePurgeDelay =
      base::TimeDelta::FromMinutes(10);
  constexpr base::TimeDelta kBeforeBackgroundPurgeDelay =
      base::TimeDelta::FromMinutes(
          MemoryPurgeManager::kDefaultMinTimeToPurgeAfterBackgrounded) /
      2;

  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{features::kFreezePurgeMemoryAllPagesFrozen,
        {{"delay-in-minutes",
          base::NumberToString(kFreezePurgeDelay.InMinutes())}}},
       {features::kPurgeRendererMemoryWhenBackgrounded, {}}},
      {});

  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(kBeforeBackgroundPurgeDelay);
  EXPECT_EQ(0U, MemoryPressureCount());

  memory_purge_manager_.OnPageResumed();
  FastForwardBy(
      base::TimeDelta::FromMinutes(
          MemoryPurgeManager::kDefaultMaxTimeToPurgeAfterBackgrounded) -
      kBeforeBackgroundPurgeDelay);
  // Since the renderer is still backgrounded, the memory purge should happen
  // even though there are no frozen pages.
  EXPECT_EQ(1U, MemoryPressureCount());

  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kActive);
}

TEST_F(MemoryPurgeManagerTest,
       PageFrozenAndRendererBackgroundedShorterBackgroundedDelay) {
  constexpr base::TimeDelta kFreezePurgeDelay =
      base::TimeDelta::FromMinutes(10);
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{features::kFreezePurgeMemoryAllPagesFrozen,
        {{"delay-in-minutes",
          base::NumberToString(kFreezePurgeDelay.InMinutes())}}},
       {features::kPurgeRendererMemoryWhenBackgrounded, {}}},
      {});

  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(base::TimeDelta::FromMinutes(
      MemoryPurgeManager::kDefaultMaxTimeToPurgeAfterBackgrounded));
  EXPECT_EQ(1U, MemoryPressureCount());

  FastForwardBy(kFreezePurgeDelay);
  EXPECT_EQ(1U, MemoryPressureCount());

  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kFrozen);
}

TEST_F(MemoryPurgeManagerTest,
       PageFrozenAndRendererBackgroundedShorterFreezeDelay) {
  constexpr base::TimeDelta kFreezePurgeDelay = base::TimeDelta::FromMinutes(
      MemoryPurgeManager::kDefaultMinTimeToPurgeAfterBackgrounded);
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{features::kFreezePurgeMemoryAllPagesFrozen,
        {{"delay-in-minutes",
          base::NumberToString(kFreezePurgeDelay.InMinutes())}}},
       {features::kPurgeRendererMemoryWhenBackgrounded, {}}},
      {});

  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  FastForwardBy(kFreezePurgeDelay);
  EXPECT_EQ(1U, MemoryPressureCount());

  FastForwardBy(base::TimeDelta::Max());
  EXPECT_EQ(1U, MemoryPressureCount());

  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kFrozen);
}

TEST_F(MemoryPurgeManagerTest, NoMemoryPurgeIfNoPage) {
  scoped_feature_list_.InitWithFeatures(
      {features::kFreezePurgeMemoryAllPagesFrozen} /* enabled */,
      {features::kPurgeRendererMemoryWhenBackgrounded} /* disabled */);

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageCreated(PageLifecycleState::kActive);

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  memory_purge_manager_.OnPageDestroyed(PageLifecycleState::kFrozen);

  FastForwardBy(base::TimeDelta::FromMinutes(0));
  EXPECT_EQ(0U, MemoryPressureCount());
}

}  // namespace

}  // namespace blink
