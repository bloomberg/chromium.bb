// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "sync/internal_api/debug_info_event_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test DebugInfoEventListenerTest;

namespace syncer {
TEST_F(DebugInfoEventListenerTest, VerifyEventsAdded) {
  syncer::DebugInfoEventListener debug_info_event_listener;
  debug_info_event_listener.CreateAndAddEvent(
      sync_pb::DebugEventInfo::ENCRYPTION_COMPLETE);
  ASSERT_EQ(debug_info_event_listener.events_.size(), 1U);
  const sync_pb::DebugEventInfo& debug_info =
      debug_info_event_listener.events_.back();
  ASSERT_TRUE(debug_info.has_type());
  ASSERT_EQ(debug_info.type(), sync_pb::DebugEventInfo::ENCRYPTION_COMPLETE);
}

TEST_F(DebugInfoEventListenerTest, VerifyQueueSize) {
  syncer::DebugInfoEventListener debug_info_event_listener;
  for (int i = 0; i < 10; ++i) {
    debug_info_event_listener.CreateAndAddEvent(
        sync_pb::DebugEventInfo::ENCRYPTION_COMPLETE);
  }
  ASSERT_EQ(debug_info_event_listener.events_.size(),
      syncer::kMaxEntries);
}

TEST_F(DebugInfoEventListenerTest, VerifyGetAndClearEvents) {
  syncer::DebugInfoEventListener debug_info_event_listener;
  debug_info_event_listener.CreateAndAddEvent(
      sync_pb::DebugEventInfo::ENCRYPTION_COMPLETE);
  ASSERT_EQ(debug_info_event_listener.events_.size(), 1U);
  sync_pb::DebugInfo debug_info;
  debug_info_event_listener.GetAndClearDebugInfo(&debug_info);
  ASSERT_EQ(debug_info_event_listener.events_.size(), 0U);
  ASSERT_EQ(debug_info.events_size(), 1);
  ASSERT_TRUE(debug_info.events(0).has_type());
  ASSERT_EQ(debug_info.events(0).type(),
      sync_pb::DebugEventInfo::ENCRYPTION_COMPLETE);
}

}  // namespace syncer
