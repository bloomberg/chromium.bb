// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/pending_user_input.h"
#include "third_party/blink/renderer/platform/scheduler/public/pending_user_input_type.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

class PendingUserInputMonitorTest : public testing::Test {
 public:
  PendingUserInput::Monitor monitor_;
};

// Tests that a single event type is tracked.
TEST_F(PendingUserInputMonitorTest, TestCounterBasic) {
  EXPECT_FALSE(
      monitor_.Info().HasPendingInputType(PendingUserInputType::kMouseDown));
  EXPECT_FALSE(monitor_.Info().HasPendingInputType(PendingUserInputType::kAny));

  monitor_.OnEnqueue(WebInputEvent::kMouseDown);
  EXPECT_TRUE(
      monitor_.Info().HasPendingInputType(PendingUserInputType::kMouseDown));
  EXPECT_TRUE(monitor_.Info().HasPendingInputType(PendingUserInputType::kAny));

  monitor_.OnDequeue(WebInputEvent::kMouseDown);
  EXPECT_FALSE(
      monitor_.Info().HasPendingInputType(PendingUserInputType::kMouseDown));
  EXPECT_FALSE(monitor_.Info().HasPendingInputType(PendingUserInputType::kAny));
}

// Tests that enqueuing multiple identical event types is tracked correctly.
TEST_F(PendingUserInputMonitorTest, TestCounterNested) {
  EXPECT_FALSE(
      monitor_.Info().HasPendingInputType(PendingUserInputType::kMouseDown));
  EXPECT_FALSE(monitor_.Info().HasPendingInputType(PendingUserInputType::kAny));

  monitor_.OnEnqueue(WebInputEvent::kMouseDown);
  EXPECT_TRUE(
      monitor_.Info().HasPendingInputType(PendingUserInputType::kMouseDown));
  EXPECT_TRUE(monitor_.Info().HasPendingInputType(PendingUserInputType::kAny));

  monitor_.OnEnqueue(WebInputEvent::kMouseDown);
  EXPECT_TRUE(
      monitor_.Info().HasPendingInputType(PendingUserInputType::kMouseDown));
  EXPECT_TRUE(monitor_.Info().HasPendingInputType(PendingUserInputType::kAny));

  monitor_.OnDequeue(WebInputEvent::kMouseDown);
  EXPECT_TRUE(
      monitor_.Info().HasPendingInputType(PendingUserInputType::kMouseDown));
  EXPECT_TRUE(monitor_.Info().HasPendingInputType(PendingUserInputType::kAny));

  monitor_.OnDequeue(WebInputEvent::kMouseDown);
  EXPECT_FALSE(
      monitor_.Info().HasPendingInputType(PendingUserInputType::kMouseDown));
  EXPECT_FALSE(monitor_.Info().HasPendingInputType(PendingUserInputType::kAny));
}

// Tests that non-overlapping input types are tracked independently.
TEST_F(PendingUserInputMonitorTest, TestCounterDisjoint) {
  EXPECT_FALSE(
      monitor_.Info().HasPendingInputType(PendingUserInputType::kMouseDown));
  EXPECT_FALSE(
      monitor_.Info().HasPendingInputType(PendingUserInputType::kMouseUp));
  EXPECT_FALSE(monitor_.Info().HasPendingInputType(PendingUserInputType::kAny));

  monitor_.OnEnqueue(WebInputEvent::kMouseDown);
  EXPECT_TRUE(
      monitor_.Info().HasPendingInputType(PendingUserInputType::kMouseDown));
  EXPECT_FALSE(
      monitor_.Info().HasPendingInputType(PendingUserInputType::kMouseUp));
  EXPECT_TRUE(monitor_.Info().HasPendingInputType(PendingUserInputType::kAny));

  monitor_.OnEnqueue(WebInputEvent::kMouseUp);
  EXPECT_TRUE(
      monitor_.Info().HasPendingInputType(PendingUserInputType::kMouseDown));
  EXPECT_TRUE(
      monitor_.Info().HasPendingInputType(PendingUserInputType::kMouseUp));
  EXPECT_TRUE(monitor_.Info().HasPendingInputType(PendingUserInputType::kAny));

  monitor_.OnDequeue(WebInputEvent::kMouseDown);
  EXPECT_FALSE(
      monitor_.Info().HasPendingInputType(PendingUserInputType::kMouseDown));
  EXPECT_TRUE(
      monitor_.Info().HasPendingInputType(PendingUserInputType::kMouseUp));
  EXPECT_TRUE(monitor_.Info().HasPendingInputType(PendingUserInputType::kAny));

  monitor_.OnDequeue(WebInputEvent::kMouseUp);
  EXPECT_FALSE(
      monitor_.Info().HasPendingInputType(PendingUserInputType::kMouseDown));
  EXPECT_FALSE(
      monitor_.Info().HasPendingInputType(PendingUserInputType::kMouseUp));
  EXPECT_FALSE(monitor_.Info().HasPendingInputType(PendingUserInputType::kAny));
}

}  // namespace scheduler
}  // namespace blink
