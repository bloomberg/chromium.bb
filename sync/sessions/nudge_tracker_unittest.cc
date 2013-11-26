// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/model_type_test_util.h"
#include "sync/notifier/invalidation_util.h"
#include "sync/notifier/object_id_invalidation_map.h"
#include "sync/sessions/nudge_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

testing::AssertionResult ModelTypeSetEquals(ModelTypeSet a, ModelTypeSet b) {
  if (a.Equals(b)) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure()
        << "Left side " << ModelTypeSetToString(a)
        << ", does not match rigth side: " << ModelTypeSetToString(b);
  }
}

}  // namespace

namespace sessions {

class NudgeTrackerTest : public ::testing::Test {
 public:
  NudgeTrackerTest() {
    SetInvalidationsInSync();
  }

  static size_t GetHintBufferSize() {
    // Assumes that no test has adjusted this size.
    return NudgeTracker::kDefaultMaxPayloadsPerType;
  }

  bool InvalidationsOutOfSync() const {
    // We don't currently track invalidations out of sync on a per-type basis.
    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker_.FillProtoMessage(BOOKMARKS, &gu_trigger);
    return gu_trigger.invalidations_out_of_sync();
  }

  int ProtoLocallyModifiedCount(ModelType type) const {
    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker_.FillProtoMessage(type, &gu_trigger);
    return gu_trigger.local_modification_nudges();
  }

  int ProtoRefreshRequestedCount(ModelType type) const {
    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker_.FillProtoMessage(type, &gu_trigger);
    return gu_trigger.datatype_refresh_nudges();
  }

  void SetInvalidationsInSync() {
    nudge_tracker_.OnInvalidationsEnabled();
    nudge_tracker_.RecordSuccessfulSyncCycle();
  }

 protected:
  NudgeTracker nudge_tracker_;
};

// Exercise an empty NudgeTracker.
// Use with valgrind to detect uninitialized members.
TEST_F(NudgeTrackerTest, EmptyNudgeTracker) {
  // Now we're at the normal, "idle" state.
  EXPECT_FALSE(nudge_tracker_.IsSyncRequired());
  EXPECT_FALSE(nudge_tracker_.IsGetUpdatesRequired());
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::UNKNOWN,
            nudge_tracker_.updates_source());

  sync_pb::GetUpdateTriggers gu_trigger;
  nudge_tracker_.FillProtoMessage(BOOKMARKS, &gu_trigger);

  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::UNKNOWN,
            nudge_tracker_.updates_source());
}

// Verify that nudges override each other based on a priority order.
// LOCAL < DATATYPE_REFRESH < NOTIFICATION
TEST_F(NudgeTrackerTest, SourcePriorities) {
  // Track a local nudge.
  nudge_tracker_.RecordLocalChange(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::LOCAL,
            nudge_tracker_.updates_source());

  // A refresh request will override it.
  nudge_tracker_.RecordLocalRefreshRequest(ModelTypeSet(TYPED_URLS));
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::DATATYPE_REFRESH,
            nudge_tracker_.updates_source());

  // Another local nudge will not be enough to change it.
  nudge_tracker_.RecordLocalChange(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::DATATYPE_REFRESH,
            nudge_tracker_.updates_source());

  // An invalidation will override the refresh request source.
  ObjectIdInvalidationMap invalidation_map =
      BuildInvalidationMap(PREFERENCES, 1, "hint");
  nudge_tracker_.RecordRemoteInvalidation(invalidation_map);
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::NOTIFICATION,
            nudge_tracker_.updates_source());

  // Neither local nudges nor refresh requests will override it.
  nudge_tracker_.RecordLocalChange(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::NOTIFICATION,
            nudge_tracker_.updates_source());
  nudge_tracker_.RecordLocalRefreshRequest(ModelTypeSet(TYPED_URLS));
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::NOTIFICATION,
            nudge_tracker_.updates_source());
}

TEST_F(NudgeTrackerTest, HintCoalescing) {
  // Easy case: record one hint.
  {
    ObjectIdInvalidationMap invalidation_map =
        BuildInvalidationMap(BOOKMARKS, 1, "bm_hint_1");
    nudge_tracker_.RecordRemoteInvalidation(invalidation_map);

    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker_.FillProtoMessage(BOOKMARKS, &gu_trigger);
    ASSERT_EQ(1, gu_trigger.notification_hint_size());
    EXPECT_EQ("bm_hint_1", gu_trigger.notification_hint(0));
    EXPECT_FALSE(gu_trigger.client_dropped_hints());
  }

  // Record a second hint for the same type.
  {
    ObjectIdInvalidationMap invalidation_map =
        BuildInvalidationMap(BOOKMARKS, 2, "bm_hint_2");
    nudge_tracker_.RecordRemoteInvalidation(invalidation_map);

    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker_.FillProtoMessage(BOOKMARKS, &gu_trigger);
    ASSERT_EQ(2, gu_trigger.notification_hint_size());

    // Expect the most hint recent is last in the list.
    EXPECT_EQ("bm_hint_1", gu_trigger.notification_hint(0));
    EXPECT_EQ("bm_hint_2", gu_trigger.notification_hint(1));
    EXPECT_FALSE(gu_trigger.client_dropped_hints());
  }

  // Record a hint for a different type.
  {
    ObjectIdInvalidationMap invalidation_map =
        BuildInvalidationMap(PASSWORDS, 1, "pw_hint_1");
    nudge_tracker_.RecordRemoteInvalidation(invalidation_map);

    // Re-verify the bookmarks to make sure they're unaffected.
    sync_pb::GetUpdateTriggers bm_gu_trigger;
    nudge_tracker_.FillProtoMessage(BOOKMARKS, &bm_gu_trigger);
    ASSERT_EQ(2, bm_gu_trigger.notification_hint_size());
    EXPECT_EQ("bm_hint_1", bm_gu_trigger.notification_hint(0));
    EXPECT_EQ("bm_hint_2",
              bm_gu_trigger.notification_hint(1)); // most recent last.
    EXPECT_FALSE(bm_gu_trigger.client_dropped_hints());

    // Verify the new type, too.
    sync_pb::GetUpdateTriggers pw_gu_trigger;
    nudge_tracker_.FillProtoMessage(PASSWORDS, &pw_gu_trigger);
    ASSERT_EQ(1, pw_gu_trigger.notification_hint_size());
    EXPECT_EQ("pw_hint_1", pw_gu_trigger.notification_hint(0));
    EXPECT_FALSE(pw_gu_trigger.client_dropped_hints());
  }
}

TEST_F(NudgeTrackerTest, DropHintsLocally) {
  ObjectIdInvalidationMap invalidation_map =
      BuildInvalidationMap(BOOKMARKS, 1, "hint");

  for (size_t i = 0; i < GetHintBufferSize(); ++i) {
    nudge_tracker_.RecordRemoteInvalidation(invalidation_map);
  }
  {
    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker_.FillProtoMessage(BOOKMARKS, &gu_trigger);
    EXPECT_EQ(GetHintBufferSize(),
              static_cast<size_t>(gu_trigger.notification_hint_size()));
    EXPECT_FALSE(gu_trigger.client_dropped_hints());
  }

  // Force an overflow.
  ObjectIdInvalidationMap invalidation_map2 =
      BuildInvalidationMap(BOOKMARKS, 1000, "new_hint");
  nudge_tracker_.RecordRemoteInvalidation(invalidation_map2);

  {
    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker_.FillProtoMessage(BOOKMARKS, &gu_trigger);
    EXPECT_EQ(GetHintBufferSize(),
              static_cast<size_t>(gu_trigger.notification_hint_size()));
    EXPECT_TRUE(gu_trigger.client_dropped_hints());

    // Verify the newest hint was not dropped and is the last in the list.
    EXPECT_EQ("new_hint", gu_trigger.notification_hint(GetHintBufferSize()-1));

    // Verify the oldest hint, too.
    EXPECT_EQ("hint", gu_trigger.notification_hint(0));
  }
}

// TODO(rlarocque): Add trickles support.  See crbug.com/223437.
// TEST_F(NudgeTrackerTest, DropHintsAtServer);

// Checks the behaviour of the invalidations-out-of-sync flag.
TEST_F(NudgeTrackerTest, EnableDisableInvalidations) {
  // Start with invalidations offline.
  nudge_tracker_.OnInvalidationsDisabled();
  EXPECT_TRUE(InvalidationsOutOfSync());
  EXPECT_TRUE(nudge_tracker_.IsGetUpdatesRequired());

  // Simply enabling invalidations does not bring us back into sync.
  nudge_tracker_.OnInvalidationsEnabled();
  EXPECT_TRUE(InvalidationsOutOfSync());
  EXPECT_TRUE(nudge_tracker_.IsGetUpdatesRequired());

  // We must successfully complete a sync cycle while invalidations are enabled
  // to be sure that we're in sync.
  nudge_tracker_.RecordSuccessfulSyncCycle();
  EXPECT_FALSE(InvalidationsOutOfSync());
  EXPECT_FALSE(nudge_tracker_.IsGetUpdatesRequired());

  // If the invalidator malfunctions, we go become unsynced again.
  nudge_tracker_.OnInvalidationsDisabled();
  EXPECT_TRUE(InvalidationsOutOfSync());
  EXPECT_TRUE(nudge_tracker_.IsGetUpdatesRequired());

  // A sync cycle while invalidations are disabled won't reset the flag.
  nudge_tracker_.RecordSuccessfulSyncCycle();
  EXPECT_TRUE(InvalidationsOutOfSync());
  EXPECT_TRUE(nudge_tracker_.IsGetUpdatesRequired());

  // Nor will the re-enabling of invalidations be sufficient, even now that
  // we've had a successful sync cycle.
  nudge_tracker_.RecordSuccessfulSyncCycle();
  EXPECT_TRUE(InvalidationsOutOfSync());
  EXPECT_TRUE(nudge_tracker_.IsGetUpdatesRequired());
}

// Tests that locally modified types are correctly written out to the
// GetUpdateTriggers proto.
TEST_F(NudgeTrackerTest, WriteLocallyModifiedTypesToProto) {
  // Should not be locally modified by default.
  EXPECT_EQ(0, ProtoLocallyModifiedCount(PREFERENCES));

  // Record a local bookmark change.  Verify it was registered correctly.
  nudge_tracker_.RecordLocalChange(ModelTypeSet(PREFERENCES));
  EXPECT_EQ(1, ProtoLocallyModifiedCount(PREFERENCES));

  // Record a successful sync cycle.  Verify the count is cleared.
  nudge_tracker_.RecordSuccessfulSyncCycle();
  EXPECT_EQ(0, ProtoLocallyModifiedCount(PREFERENCES));
}

// Tests that refresh requested types are correctly written out to the
// GetUpdateTriggers proto.
TEST_F(NudgeTrackerTest, WriteRefreshRequestedTypesToProto) {
  // There should be no refresh requested by default.
  EXPECT_EQ(0, ProtoRefreshRequestedCount(SESSIONS));

  // Record a local refresh request.  Verify it was registered correctly.
  nudge_tracker_.RecordLocalRefreshRequest(ModelTypeSet(SESSIONS));
  EXPECT_EQ(1, ProtoRefreshRequestedCount(SESSIONS));

  // Record a successful sync cycle.  Verify the count is cleared.
  nudge_tracker_.RecordSuccessfulSyncCycle();
  EXPECT_EQ(0, ProtoRefreshRequestedCount(SESSIONS));
}

// Basic tests for the IsSyncRequired() flag.
TEST_F(NudgeTrackerTest, IsSyncRequired) {
  EXPECT_FALSE(nudge_tracker_.IsSyncRequired());

  // Local changes.
  nudge_tracker_.RecordLocalChange(ModelTypeSet(SESSIONS));
  EXPECT_TRUE(nudge_tracker_.IsSyncRequired());
  nudge_tracker_.RecordSuccessfulSyncCycle();
  EXPECT_FALSE(nudge_tracker_.IsSyncRequired());

  // Refresh requests.
  nudge_tracker_.RecordLocalRefreshRequest(ModelTypeSet(SESSIONS));
  EXPECT_TRUE(nudge_tracker_.IsSyncRequired());
  nudge_tracker_.RecordSuccessfulSyncCycle();
  EXPECT_FALSE(nudge_tracker_.IsSyncRequired());

  // Invalidations.
  ObjectIdInvalidationMap invalidation_map =
      BuildInvalidationMap(PREFERENCES, 1, "hint");
  nudge_tracker_.RecordRemoteInvalidation(invalidation_map);
  EXPECT_TRUE(nudge_tracker_.IsSyncRequired());
  nudge_tracker_.RecordSuccessfulSyncCycle();
  EXPECT_FALSE(nudge_tracker_.IsSyncRequired());
}

// Basic tests for the IsGetUpdatesRequired() flag.
TEST_F(NudgeTrackerTest, IsGetUpdatesRequired) {
  EXPECT_FALSE(nudge_tracker_.IsGetUpdatesRequired());

  // Local changes.
  nudge_tracker_.RecordLocalChange(ModelTypeSet(SESSIONS));
  EXPECT_FALSE(nudge_tracker_.IsGetUpdatesRequired());
  nudge_tracker_.RecordSuccessfulSyncCycle();
  EXPECT_FALSE(nudge_tracker_.IsGetUpdatesRequired());

  // Refresh requests.
  nudge_tracker_.RecordLocalRefreshRequest(ModelTypeSet(SESSIONS));
  EXPECT_TRUE(nudge_tracker_.IsGetUpdatesRequired());
  nudge_tracker_.RecordSuccessfulSyncCycle();
  EXPECT_FALSE(nudge_tracker_.IsGetUpdatesRequired());

  // Invalidations.
  ObjectIdInvalidationMap invalidation_map =
      BuildInvalidationMap(PREFERENCES, 1, "hint");
  nudge_tracker_.RecordRemoteInvalidation(invalidation_map);
  EXPECT_TRUE(nudge_tracker_.IsGetUpdatesRequired());
  nudge_tracker_.RecordSuccessfulSyncCycle();
  EXPECT_FALSE(nudge_tracker_.IsGetUpdatesRequired());
}

// Test IsSyncRequired() responds correctly to data type throttling.
TEST_F(NudgeTrackerTest, IsSyncRequired_Throttling) {
  const base::TimeTicks t0 = base::TimeTicks::FromInternalValue(1234);
  const base::TimeDelta throttle_length = base::TimeDelta::FromMinutes(10);
  const base::TimeTicks t1 = t0 + throttle_length;

  EXPECT_FALSE(nudge_tracker_.IsSyncRequired());

  // A local change to sessions enables the flag.
  nudge_tracker_.RecordLocalChange(ModelTypeSet(SESSIONS));
  EXPECT_TRUE(nudge_tracker_.IsSyncRequired());

  // But the throttling of sessions unsets it.
  nudge_tracker_.SetTypesThrottledUntil(ModelTypeSet(SESSIONS),
                                       throttle_length,
                                       t0);
  EXPECT_FALSE(nudge_tracker_.IsSyncRequired());

  // A refresh request for bookmarks means we have reason to sync again.
  nudge_tracker_.RecordLocalRefreshRequest(ModelTypeSet(BOOKMARKS));
  EXPECT_TRUE(nudge_tracker_.IsSyncRequired());

  // A successful sync cycle means we took care of bookmarks.
  nudge_tracker_.RecordSuccessfulSyncCycle();
  EXPECT_FALSE(nudge_tracker_.IsSyncRequired());

  // But we still haven't dealt with sessions.  We'll need to remember
  // that sessions are out of sync and re-enable the flag when their
  // throttling interval expires.
  nudge_tracker_.UpdateTypeThrottlingState(t1);
  EXPECT_FALSE(nudge_tracker_.IsTypeThrottled(SESSIONS));
  EXPECT_TRUE(nudge_tracker_.IsSyncRequired());
}

// Test IsGetUpdatesRequired() responds correctly to data type throttling.
TEST_F(NudgeTrackerTest, IsGetUpdatesRequired_Throttling) {
  const base::TimeTicks t0 = base::TimeTicks::FromInternalValue(1234);
  const base::TimeDelta throttle_length = base::TimeDelta::FromMinutes(10);
  const base::TimeTicks t1 = t0 + throttle_length;

  EXPECT_FALSE(nudge_tracker_.IsGetUpdatesRequired());

  // A refresh request to sessions enables the flag.
  nudge_tracker_.RecordLocalRefreshRequest(ModelTypeSet(SESSIONS));
  EXPECT_TRUE(nudge_tracker_.IsGetUpdatesRequired());

  // But the throttling of sessions unsets it.
  nudge_tracker_.SetTypesThrottledUntil(ModelTypeSet(SESSIONS),
                                       throttle_length,
                                       t0);
  EXPECT_FALSE(nudge_tracker_.IsGetUpdatesRequired());

  // A refresh request for bookmarks means we have reason to sync again.
  nudge_tracker_.RecordLocalRefreshRequest(ModelTypeSet(BOOKMARKS));
  EXPECT_TRUE(nudge_tracker_.IsGetUpdatesRequired());

  // A successful sync cycle means we took care of bookmarks.
  nudge_tracker_.RecordSuccessfulSyncCycle();
  EXPECT_FALSE(nudge_tracker_.IsGetUpdatesRequired());

  // But we still haven't dealt with sessions.  We'll need to remember
  // that sessions are out of sync and re-enable the flag when their
  // throttling interval expires.
  nudge_tracker_.UpdateTypeThrottlingState(t1);
  EXPECT_FALSE(nudge_tracker_.IsTypeThrottled(SESSIONS));
  EXPECT_TRUE(nudge_tracker_.IsGetUpdatesRequired());
}

// Tests throttling-related getter functions when no types are throttled.
TEST_F(NudgeTrackerTest, NoTypesThrottled) {
  EXPECT_FALSE(nudge_tracker_.IsAnyTypeThrottled());
  EXPECT_FALSE(nudge_tracker_.IsTypeThrottled(SESSIONS));
  EXPECT_TRUE(nudge_tracker_.GetThrottledTypes().Empty());
}

// Tests throttling-related getter functions when some types are throttled.
TEST_F(NudgeTrackerTest, ThrottleAndUnthrottle) {
  const base::TimeTicks t0 = base::TimeTicks::FromInternalValue(1234);
  const base::TimeDelta throttle_length = base::TimeDelta::FromMinutes(10);
  const base::TimeTicks t1 = t0 + throttle_length;

  nudge_tracker_.SetTypesThrottledUntil(ModelTypeSet(SESSIONS, PREFERENCES),
                                       throttle_length,
                                       t0);

  EXPECT_TRUE(nudge_tracker_.IsAnyTypeThrottled());
  EXPECT_TRUE(nudge_tracker_.IsTypeThrottled(SESSIONS));
  EXPECT_TRUE(nudge_tracker_.IsTypeThrottled(PREFERENCES));
  EXPECT_FALSE(nudge_tracker_.GetThrottledTypes().Empty());
  EXPECT_EQ(throttle_length, nudge_tracker_.GetTimeUntilNextUnthrottle(t0));

  nudge_tracker_.UpdateTypeThrottlingState(t1);

  EXPECT_FALSE(nudge_tracker_.IsAnyTypeThrottled());
  EXPECT_FALSE(nudge_tracker_.IsTypeThrottled(SESSIONS));
  EXPECT_TRUE(nudge_tracker_.GetThrottledTypes().Empty());
}

TEST_F(NudgeTrackerTest, OverlappingThrottleIntervals) {
  const base::TimeTicks t0 = base::TimeTicks::FromInternalValue(1234);
  const base::TimeDelta throttle1_length = base::TimeDelta::FromMinutes(10);
  const base::TimeDelta throttle2_length = base::TimeDelta::FromMinutes(20);
  const base::TimeTicks t1 = t0 + throttle1_length;
  const base::TimeTicks t2 = t0 + throttle2_length;

  // Setup the longer of two intervals.
  nudge_tracker_.SetTypesThrottledUntil(ModelTypeSet(SESSIONS, PREFERENCES),
                                       throttle2_length,
                                       t0);
  EXPECT_TRUE(ModelTypeSetEquals(
          ModelTypeSet(SESSIONS, PREFERENCES),
          nudge_tracker_.GetThrottledTypes()));
  EXPECT_EQ(throttle2_length,
            nudge_tracker_.GetTimeUntilNextUnthrottle(t0));

  // Setup the shorter interval.
  nudge_tracker_.SetTypesThrottledUntil(ModelTypeSet(SESSIONS, BOOKMARKS),
                                       throttle1_length,
                                       t0);
  EXPECT_TRUE(ModelTypeSetEquals(
          ModelTypeSet(SESSIONS, PREFERENCES, BOOKMARKS),
          nudge_tracker_.GetThrottledTypes()));
  EXPECT_EQ(throttle1_length,
            nudge_tracker_.GetTimeUntilNextUnthrottle(t0));

  // Expire the first interval.
  nudge_tracker_.UpdateTypeThrottlingState(t1);

  // SESSIONS appeared in both intervals.  We expect it will be throttled for
  // the longer of the two, so it's still throttled at time t1.
  EXPECT_TRUE(ModelTypeSetEquals(
          ModelTypeSet(SESSIONS, PREFERENCES),
          nudge_tracker_.GetThrottledTypes()));
  EXPECT_EQ(throttle2_length - throttle1_length,
            nudge_tracker_.GetTimeUntilNextUnthrottle(t1));

  // Expire the second interval.
  nudge_tracker_.UpdateTypeThrottlingState(t2);
  EXPECT_TRUE(nudge_tracker_.GetThrottledTypes().Empty());
}

}  // namespace sessions
}  // namespace syncer
