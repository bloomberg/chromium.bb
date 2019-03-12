// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/memory_purge_manager.h"

#include "base/memory/memory_pressure_listener.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

class MemoryPurgeManagerTest : public testing::Test {
 public:
  MemoryPurgeManagerTest() = default;

  void SetUp() override {
    memory_pressure_listener_ =
        std::make_unique<base::MemoryPressureListener>(base::BindRepeating(
            &MemoryPurgeManagerTest::OnMemoryPressure, base::Unretained(this)));
    base::MemoryPressureListener::SetNotificationsSuppressed(false);
  }

  void TearDown() override { memory_pressure_listener_.reset(); }

 protected:
  void ExpectMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level) {
    EXPECT_CALL(*this, OnMemoryPressure(level)).Times(1);
    base::RunLoop().RunUntilIdle();
  }

  void ExpectNoMemoryPressure() {
    EXPECT_CALL(*this, OnMemoryPressure(testing::_)).Times(0);
    base::RunLoop().RunUntilIdle();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  MemoryPurgeManager memory_purge_manager_;

 private:
  MOCK_METHOD1(OnMemoryPressure,
               void(base::MemoryPressureListener::MemoryPressureLevel));
  DISALLOW_COPY_AND_ASSIGN(MemoryPurgeManagerTest);
};

// Verify that OnPageFrozen() triggers a memory pressure notification in a
// backgrounded renderer.
TEST_F(MemoryPurgeManagerTest, PageFrozenInBackgroundedRenderer) {
  memory_purge_manager_.OnPageCreated(false /* is_frozen */);
  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  ExpectMemoryPressure(base::MemoryPressureListener::MemoryPressureLevel::
                           MEMORY_PRESSURE_LEVEL_CRITICAL);
}

// Verify that OnPageFrozen() does not trigger a memory pressure notification in
// a foregrounded renderer.
TEST_F(MemoryPurgeManagerTest, PageFrozenInForegroundedRenderer) {
  memory_purge_manager_.OnPageCreated(false /* is_frozen */);
  memory_purge_manager_.SetRendererBackgrounded(false);
  memory_purge_manager_.OnPageFrozen();
  ExpectNoMemoryPressure();
}

TEST_F(MemoryPurgeManagerTest, PageUnfrozenUndoMemoryPressureSuppression) {
  memory_purge_manager_.OnPageCreated(false /* is_frozen */);

  memory_purge_manager_.SetRendererBackgrounded(true);
  memory_purge_manager_.OnPageFrozen();
  EXPECT_TRUE(base::MemoryPressureListener::AreNotificationsSuppressed());
  memory_purge_manager_.OnPageUnfrozen();
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageDestroyed(false /* was_frozen */);
}

TEST_F(MemoryPurgeManagerTest,
       PageFrozenMemorySuppressionOnlyWhenAllPagesFrozen) {
  memory_purge_manager_.SetRendererBackgrounded(true);

  memory_purge_manager_.OnPageCreated(false /* is_frozen */);
  memory_purge_manager_.OnPageCreated(false /* is_frozen */);
  memory_purge_manager_.OnPageCreated(false /* is_frozen */);

  memory_purge_manager_.OnPageFrozen();
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageFrozen();
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageFrozen();
  EXPECT_TRUE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageUnfrozen();
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageDestroyed(false /* was_frozen */);
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageCreated(false /* is_frozen */);
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());

  memory_purge_manager_.OnPageDestroyed(false /* was_frozen */);
  memory_purge_manager_.OnPageDestroyed(true /* was_frozen */);
  memory_purge_manager_.OnPageDestroyed(true /* was_frozen */);
}

}  // namespace blink
