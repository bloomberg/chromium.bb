// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/debug_info_event_listener.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

typedef testing::Test DebugInfoEventListenerTest;

TEST_F(DebugInfoEventListenerTest, VerifyEventsAdded) {
  DebugInfoEventListener debug_info_event_listener;
  debug_info_event_listener.CreateAndAddEvent(
      sync_pb::DebugEventInfo::ENCRYPTION_COMPLETE);
  ASSERT_EQ(debug_info_event_listener.events_.size(), 1U);
  const sync_pb::DebugEventInfo& debug_info =
      debug_info_event_listener.events_.back();
  ASSERT_TRUE(debug_info.has_singleton_event());
  ASSERT_EQ(debug_info.singleton_event(),
            sync_pb::DebugEventInfo::ENCRYPTION_COMPLETE);
}

TEST_F(DebugInfoEventListenerTest, VerifyQueueSize) {
  DebugInfoEventListener debug_info_event_listener;
  for (unsigned int i = 0; i < 2*kMaxEntries; ++i) {
    debug_info_event_listener.CreateAndAddEvent(
        sync_pb::DebugEventInfo::ENCRYPTION_COMPLETE);
  }
  sync_pb::DebugInfo debug_info;
  debug_info_event_listener.GetAndClearDebugInfo(&debug_info);
  ASSERT_TRUE(debug_info.events_dropped());
  ASSERT_EQ(static_cast<int>(kMaxEntries), debug_info.events_size());
}

TEST_F(DebugInfoEventListenerTest, VerifyGetAndClearEvents) {
  DebugInfoEventListener debug_info_event_listener;
  debug_info_event_listener.CreateAndAddEvent(
      sync_pb::DebugEventInfo::ENCRYPTION_COMPLETE);
  ASSERT_EQ(debug_info_event_listener.events_.size(), 1U);
  sync_pb::DebugInfo debug_info;
  debug_info_event_listener.GetAndClearDebugInfo(&debug_info);
  ASSERT_EQ(debug_info_event_listener.events_.size(), 0U);
  ASSERT_EQ(debug_info.events_size(), 1);
  ASSERT_TRUE(debug_info.events(0).has_singleton_event());
  ASSERT_EQ(debug_info.events(0).singleton_event(),
            sync_pb::DebugEventInfo::ENCRYPTION_COMPLETE);
}

}  // namespace syncer
