// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "sync/internal_api/public/base/model_type_test_util.h"
#include "sync/notifier/invalidation_util.h"
#include "sync/notifier/mock_ack_handler.h"
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

syncer::Invalidation BuildUnknownVersionInvalidation(ModelType type) {
  invalidation::ObjectId id;
  bool result = RealModelTypeToObjectId(type, &id);
  DCHECK(result);
  return Invalidation::InitUnknownVersion(id);
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
            nudge_tracker_.GetLegacySource());

  sync_pb::GetUpdateTriggers gu_trigger;
  nudge_tracker_.FillProtoMessage(BOOKMARKS, &gu_trigger);

  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::UNKNOWN,
            nudge_tracker_.GetLegacySource());
}

// Verify that nudges override each other based on a priority order.
// RETRY < LOCAL < DATATYPE_REFRESH < NOTIFICATION
TEST_F(NudgeTrackerTest, SourcePriorities) {
  // Start with a retry request.
  const base::TimeTicks t0 = base::TimeTicks::FromInternalValue(1234);
  const base::TimeTicks t1 = t0 + base::TimeDelta::FromSeconds(10);
  nudge_tracker_.SetNextRetryTime(t0);
  nudge_tracker_.SetSyncCycleStartTime(t1);
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::RETRY,
            nudge_tracker_.GetLegacySource());

  // Track a local nudge.
  nudge_tracker_.RecordLocalChange(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::LOCAL,
            nudge_tracker_.GetLegacySource());

  // A refresh request will override it.
  nudge_tracker_.RecordLocalRefreshRequest(ModelTypeSet(TYPED_URLS));
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::DATATYPE_REFRESH,
            nudge_tracker_.GetLegacySource());

  // Another local nudge will not be enough to change it.
  nudge_tracker_.RecordLocalChange(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::DATATYPE_REFRESH,
            nudge_tracker_.GetLegacySource());

  // An invalidation will override the refresh request source.
  ObjectIdInvalidationMap invalidation_map =
      BuildInvalidationMap(PREFERENCES, 1, "hint");
  nudge_tracker_.RecordRemoteInvalidation(invalidation_map);
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::NOTIFICATION,
            nudge_tracker_.GetLegacySource());

  // Neither local nudges nor refresh requests will override it.
  nudge_tracker_.RecordLocalChange(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::NOTIFICATION,
            nudge_tracker_.GetLegacySource());
  nudge_tracker_.RecordLocalRefreshRequest(ModelTypeSet(TYPED_URLS));
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::NOTIFICATION,
            nudge_tracker_.GetLegacySource());
}

// Verifies the management of invalidation hints and GU trigger fields.
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

// Test the dropping of invalidation hints.  Receives invalidations one by one.
TEST_F(NudgeTrackerTest, DropHintsLocally_OneAtATime) {
  for (size_t i = 0; i < GetHintBufferSize(); ++i) {
    ObjectIdInvalidationMap invalidation_map =
        BuildInvalidationMap(BOOKMARKS, i, "hint");
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
    EXPECT_TRUE(gu_trigger.client_dropped_hints());
    ASSERT_EQ(GetHintBufferSize(),
              static_cast<size_t>(gu_trigger.notification_hint_size()));

    // Verify the newest hint was not dropped and is the last in the list.
    EXPECT_EQ("new_hint", gu_trigger.notification_hint(GetHintBufferSize()-1));

    // Verify the oldest hint, too.
    EXPECT_EQ("hint", gu_trigger.notification_hint(0));
  }
}

// Test the dropping of invalidation hints.
// Receives invalidations in large batches.
TEST_F(NudgeTrackerTest, DropHintsLocally_ManyHints) {
  ObjectIdInvalidationMap invalidation_map;
  for (size_t i = 0; i < GetHintBufferSize(); ++i) {
    invalidation_map.Insert(BuildInvalidation(BOOKMARKS, i, "hint"));
  }
  nudge_tracker_.RecordRemoteInvalidation(invalidation_map);
  {
    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker_.FillProtoMessage(BOOKMARKS, &gu_trigger);
    EXPECT_EQ(GetHintBufferSize(),
              static_cast<size_t>(gu_trigger.notification_hint_size()));
    EXPECT_FALSE(gu_trigger.client_dropped_hints());
  }

  // Force an overflow.
  ObjectIdInvalidationMap invalidation_map2;
  invalidation_map2.Insert(BuildInvalidation(BOOKMARKS, 1000, "new_hint"));
  invalidation_map2.Insert(BuildInvalidation(BOOKMARKS, 1001, "newer_hint"));
  nudge_tracker_.RecordRemoteInvalidation(invalidation_map2);

  {
    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker_.FillProtoMessage(BOOKMARKS, &gu_trigger);
    EXPECT_TRUE(gu_trigger.client_dropped_hints());
    ASSERT_EQ(GetHintBufferSize(),
              static_cast<size_t>(gu_trigger.notification_hint_size()));

    // Verify the newest hints were not dropped and are the last in the list.
    EXPECT_EQ("newer_hint",
              gu_trigger.notification_hint(GetHintBufferSize()-1));
    EXPECT_EQ("new_hint", gu_trigger.notification_hint(GetHintBufferSize()-2));

    // Verify the oldest hint, too.
    EXPECT_EQ("hint", gu_trigger.notification_hint(0));
  }
}

// Tests the receipt of 'unknown version' invalidations.
TEST_F(NudgeTrackerTest, DropHintsAtServer_Alone) {
  ObjectIdInvalidationMap invalidation_map;
  invalidation_map.Insert(BuildUnknownVersionInvalidation(BOOKMARKS));

  // Record the unknown version invalidation.
  nudge_tracker_.RecordRemoteInvalidation(invalidation_map);
  {
    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker_.FillProtoMessage(BOOKMARKS, &gu_trigger);
    EXPECT_TRUE(gu_trigger.server_dropped_hints());
    EXPECT_FALSE(gu_trigger.client_dropped_hints());
    ASSERT_EQ(0, gu_trigger.notification_hint_size());
  }

  // Clear status then verify.
  nudge_tracker_.RecordSuccessfulSyncCycle();
  {
    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker_.FillProtoMessage(BOOKMARKS, &gu_trigger);
    EXPECT_FALSE(gu_trigger.client_dropped_hints());
    EXPECT_FALSE(gu_trigger.server_dropped_hints());
    ASSERT_EQ(0, gu_trigger.notification_hint_size());
  }
}

// Tests the receipt of 'unknown version' invalidations.  This test also
// includes a known version invalidation to mix things up a bit.
TEST_F(NudgeTrackerTest, DropHintsAtServer_WithOtherInvalidations) {
  ObjectIdInvalidationMap invalidation_map;
  invalidation_map.Insert(BuildUnknownVersionInvalidation(BOOKMARKS));
  invalidation_map.Insert(BuildInvalidation(BOOKMARKS, 10, "hint"));

  // Record the two invalidations, one with unknown version, the other unknown.
  nudge_tracker_.RecordRemoteInvalidation(invalidation_map);
  {
    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker_.FillProtoMessage(BOOKMARKS, &gu_trigger);
    EXPECT_TRUE(gu_trigger.server_dropped_hints());
    EXPECT_FALSE(gu_trigger.client_dropped_hints());
    ASSERT_EQ(1, gu_trigger.notification_hint_size());
    EXPECT_EQ("hint", gu_trigger.notification_hint(0));
  }

  // Clear status then verify.
  nudge_tracker_.RecordSuccessfulSyncCycle();
  {
    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker_.FillProtoMessage(BOOKMARKS, &gu_trigger);
    EXPECT_FALSE(gu_trigger.client_dropped_hints());
    EXPECT_FALSE(gu_trigger.server_dropped_hints());
    ASSERT_EQ(0, gu_trigger.notification_hint_size());
  }
}

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

TEST_F(NudgeTrackerTest, Retry) {
  const base::TimeTicks t0 = base::TimeTicks::FromInternalValue(12345);
  const base::TimeTicks t3 = t0 + base::TimeDelta::FromSeconds(3);
  const base::TimeTicks t4 = t0 + base::TimeDelta::FromSeconds(4);

  // Set retry for t3.
  nudge_tracker_.SetNextRetryTime(t3);

  // Not due yet at t0.
  nudge_tracker_.SetSyncCycleStartTime(t0);
  EXPECT_FALSE(nudge_tracker_.IsRetryRequired());
  EXPECT_FALSE(nudge_tracker_.IsGetUpdatesRequired());

  // Successful sync cycle at t0 changes nothing.
  nudge_tracker_.RecordSuccessfulSyncCycle();
  EXPECT_FALSE(nudge_tracker_.IsRetryRequired());
  EXPECT_FALSE(nudge_tracker_.IsGetUpdatesRequired());

  // At t4, the retry becomes due.
  nudge_tracker_.SetSyncCycleStartTime(t4);
  EXPECT_TRUE(nudge_tracker_.IsRetryRequired());
  EXPECT_TRUE(nudge_tracker_.IsGetUpdatesRequired());

  // A sync cycle unsets the flag.
  nudge_tracker_.RecordSuccessfulSyncCycle();
  EXPECT_FALSE(nudge_tracker_.IsRetryRequired());

  // It's still unset at the start of the next sync cycle.
  nudge_tracker_.SetSyncCycleStartTime(t4);
  EXPECT_FALSE(nudge_tracker_.IsRetryRequired());
}

// Test a mid-cycle update when IsRetryRequired() was true before the cycle
// began.
TEST_F(NudgeTrackerTest, IsRetryRequired_MidCycleUpdate1) {
  const base::TimeTicks t0 = base::TimeTicks::FromInternalValue(12345);
  const base::TimeTicks t1 = t0 + base::TimeDelta::FromSeconds(1);
  const base::TimeTicks t2 = t0 + base::TimeDelta::FromSeconds(2);
  const base::TimeTicks t5 = t0 + base::TimeDelta::FromSeconds(5);
  const base::TimeTicks t6 = t0 + base::TimeDelta::FromSeconds(6);

  nudge_tracker_.SetNextRetryTime(t0);
  nudge_tracker_.SetSyncCycleStartTime(t1);

  EXPECT_TRUE(nudge_tracker_.IsRetryRequired());

  // Pretend that we were updated mid-cycle.  SetSyncCycleStartTime is
  // called only at the start of the sync cycle, so don't call it here.
  // The update should have no effect on IsRetryRequired().
  nudge_tracker_.SetNextRetryTime(t5);

  EXPECT_TRUE(nudge_tracker_.IsRetryRequired());

  // Verify that the successful sync cycle clears the flag.
  nudge_tracker_.RecordSuccessfulSyncCycle();
  EXPECT_FALSE(nudge_tracker_.IsRetryRequired());

  // Verify expecations around the new retry time.
  nudge_tracker_.SetSyncCycleStartTime(t2);
  EXPECT_FALSE(nudge_tracker_.IsRetryRequired());

  nudge_tracker_.SetSyncCycleStartTime(t6);
  EXPECT_TRUE(nudge_tracker_.IsRetryRequired());
}

// Test a mid-cycle update when IsRetryRequired() was false before the cycle
// began.
TEST_F(NudgeTrackerTest, IsRetryRequired_MidCycleUpdate2) {
  const base::TimeTicks t0 = base::TimeTicks::FromInternalValue(12345);
  const base::TimeTicks t1 = t0 + base::TimeDelta::FromSeconds(1);
  const base::TimeTicks t3 = t0 + base::TimeDelta::FromSeconds(3);
  const base::TimeTicks t5 = t0 + base::TimeDelta::FromSeconds(5);
  const base::TimeTicks t6 = t0 + base::TimeDelta::FromSeconds(6);

  // Schedule a future retry, and a nudge unrelated to it.
  nudge_tracker_.RecordLocalChange(ModelTypeSet(BOOKMARKS));
  nudge_tracker_.SetNextRetryTime(t1);
  nudge_tracker_.SetSyncCycleStartTime(t0);
  EXPECT_FALSE(nudge_tracker_.IsRetryRequired());

  // Pretend this happened in mid-cycle.  This should have no effect on
  // IsRetryRequired().
  nudge_tracker_.SetNextRetryTime(t5);
  EXPECT_FALSE(nudge_tracker_.IsRetryRequired());

  // The cycle succeeded.
  nudge_tracker_.RecordSuccessfulSyncCycle();

  // The time t3 is greater than the GU retry time scheduled at the beginning of
  // the test, but later than the retry time that overwrote it during the
  // pretend 'sync cycle'.
  nudge_tracker_.SetSyncCycleStartTime(t3);
  EXPECT_FALSE(nudge_tracker_.IsRetryRequired());

  // Finally, the retry established during the sync cycle becomes due.
  nudge_tracker_.SetSyncCycleStartTime(t6);
  EXPECT_TRUE(nudge_tracker_.IsRetryRequired());
}

// Simulate the case where a sync cycle fails.
TEST_F(NudgeTrackerTest, IsRetryRequired_FailedCycle) {
  const base::TimeTicks t0 = base::TimeTicks::FromInternalValue(12345);
  const base::TimeTicks t1 = t0 + base::TimeDelta::FromSeconds(1);
  const base::TimeTicks t2 = t0 + base::TimeDelta::FromSeconds(2);

  nudge_tracker_.SetNextRetryTime(t0);
  nudge_tracker_.SetSyncCycleStartTime(t1);
  EXPECT_TRUE(nudge_tracker_.IsRetryRequired());

  // The nudge tracker receives no notifications for a failed sync cycle.
  // Pretend one happened here.
  EXPECT_TRUE(nudge_tracker_.IsRetryRequired());

  // Think of this as the retry cycle.
  nudge_tracker_.SetSyncCycleStartTime(t2);
  EXPECT_TRUE(nudge_tracker_.IsRetryRequired());

  // The second cycle is a success.
  nudge_tracker_.RecordSuccessfulSyncCycle();
  EXPECT_FALSE(nudge_tracker_.IsRetryRequired());
}

// Simulate a partially failed sync cycle.  The callback to update the GU retry
// was invoked, but the sync cycle did not complete successfully.
TEST_F(NudgeTrackerTest, IsRetryRequired_FailedCycleIncludesUpdate) {
  const base::TimeTicks t0 = base::TimeTicks::FromInternalValue(12345);
  const base::TimeTicks t1 = t0 + base::TimeDelta::FromSeconds(1);
  const base::TimeTicks t3 = t0 + base::TimeDelta::FromSeconds(3);
  const base::TimeTicks t4 = t0 + base::TimeDelta::FromSeconds(4);
  const base::TimeTicks t5 = t0 + base::TimeDelta::FromSeconds(5);
  const base::TimeTicks t6 = t0 + base::TimeDelta::FromSeconds(6);

  nudge_tracker_.SetNextRetryTime(t0);
  nudge_tracker_.SetSyncCycleStartTime(t1);
  EXPECT_TRUE(nudge_tracker_.IsRetryRequired());

  // The cycle is in progress.  A new GU Retry time is received.
  // The flag is not because this cycle is still in progress.
  nudge_tracker_.SetNextRetryTime(t5);
  EXPECT_TRUE(nudge_tracker_.IsRetryRequired());

  // The nudge tracker receives no notifications for a failed sync cycle.
  // Pretend the cycle failed here.

  // The next sync cycle starts.  The new GU time has not taken effect by this
  // time, but the NudgeTracker hasn't forgotten that we have not yet serviced
  // the retry from the previous cycle.
  nudge_tracker_.SetSyncCycleStartTime(t3);
  EXPECT_TRUE(nudge_tracker_.IsRetryRequired());

  // It succeeds.  The retry time is not updated, so it should remain at t5.
  nudge_tracker_.RecordSuccessfulSyncCycle();

  // Another sync cycle.  This one is still before the scheduled retry.  It does
  // not change the scheduled retry time.
  nudge_tracker_.SetSyncCycleStartTime(t4);
  EXPECT_FALSE(nudge_tracker_.IsRetryRequired());
  nudge_tracker_.RecordSuccessfulSyncCycle();

  // The retry scheduled way back during the first cycle of this test finally
  // becomes due.  Perform a successful sync cycle to service it.
  nudge_tracker_.SetSyncCycleStartTime(t6);
  EXPECT_TRUE(nudge_tracker_.IsRetryRequired());
  nudge_tracker_.RecordSuccessfulSyncCycle();
}

class NudgeTrackerAckTrackingTest : public NudgeTrackerTest {
 public:
  NudgeTrackerAckTrackingTest() {}

  bool IsInvalidationUnacknowledged(const syncer::Invalidation& invalidation) {
    // Run pending tasks before checking with the MockAckHandler.
    // The WeakHandle may have posted some tasks for it.
    base::RunLoop().RunUntilIdle();
    return mock_ack_handler_.IsUnacked(invalidation);
  }

  bool IsInvalidationAcknowledged(const syncer::Invalidation& invalidation) {
    // Run pending tasks before checking with the MockAckHandler.
    // The WeakHandle may have posted some tasks for it.
    base::RunLoop().RunUntilIdle();
    return mock_ack_handler_.IsAcknowledged(invalidation);
  }

  bool IsInvalidationDropped(const syncer::Invalidation& invalidation) {
    // Run pending tasks before checking with the MockAckHandler.
    // The WeakHandle may have posted some tasks for it.
    base::RunLoop().RunUntilIdle();
    return mock_ack_handler_.IsDropped(invalidation);
  }

  bool AllInvalidationsAccountedFor() {
    return mock_ack_handler_.AllInvalidationsAccountedFor();
  }

  Invalidation SendInvalidation(
      ModelType type,
      int64 version,
      const std::string& hint) {
    // Build and register the invalidation.
    syncer::Invalidation invalidation = BuildInvalidation(type, version, hint);
    mock_ack_handler_.RegisterInvalidation(&invalidation);

    // Send it to the NudgeTracker.
    ObjectIdInvalidationMap invalidation_map;
    invalidation_map.Insert(invalidation);
    nudge_tracker_.RecordRemoteInvalidation(invalidation_map);

    // Return it to the test framework for use in assertions.
    return invalidation;
  }

  Invalidation SendUnknownVersionInvalidation(ModelType type) {
    // Build and register the invalidation.
    syncer::Invalidation invalidation = BuildUnknownVersionInvalidation(type);
    mock_ack_handler_.RegisterInvalidation(&invalidation);

    // Send it to the NudgeTracker.
    ObjectIdInvalidationMap invalidation_map;
    invalidation_map.Insert(invalidation);
    nudge_tracker_.RecordRemoteInvalidation(invalidation_map);

    // Return it to the test framework for use in assertions.
    return invalidation;
  }

  void RecordSuccessfulSyncCycle() {
    nudge_tracker_.RecordSuccessfulSyncCycle();
  }

 private:
  syncer::MockAckHandler mock_ack_handler_;
  base::MessageLoop loop_;
};

// Test the acknowledgement of a single invalidation.
TEST_F(NudgeTrackerAckTrackingTest, SimpleAcknowledgement) {
  Invalidation inv = SendInvalidation(BOOKMARKS, 10, "hint");

  EXPECT_TRUE(IsInvalidationUnacknowledged(inv));

  RecordSuccessfulSyncCycle();
  EXPECT_TRUE(IsInvalidationAcknowledged(inv));

  EXPECT_TRUE(AllInvalidationsAccountedFor());
}

// Test the acknowledgement of many invalidations.
TEST_F(NudgeTrackerAckTrackingTest, ManyAcknowledgements) {
  Invalidation inv1 = SendInvalidation(BOOKMARKS, 10, "hint");
  Invalidation inv2 = SendInvalidation(BOOKMARKS, 14, "hint2");
  Invalidation inv3 = SendInvalidation(PREFERENCES, 8, "hint3");

  EXPECT_TRUE(IsInvalidationUnacknowledged(inv1));
  EXPECT_TRUE(IsInvalidationUnacknowledged(inv2));
  EXPECT_TRUE(IsInvalidationUnacknowledged(inv3));

  RecordSuccessfulSyncCycle();
  EXPECT_TRUE(IsInvalidationAcknowledged(inv1));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv2));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv3));

  EXPECT_TRUE(AllInvalidationsAccountedFor());
}

// Test dropping when the buffer overflows and subsequent drop recovery.
TEST_F(NudgeTrackerAckTrackingTest, OverflowAndRecover) {
  std::vector<Invalidation> invalidations;

  Invalidation inv10 = SendInvalidation(BOOKMARKS, 10, "hint");
  for (size_t i = 1; i < GetHintBufferSize(); ++i) {
    invalidations.push_back(SendInvalidation(BOOKMARKS, i+10, "hint"));
  }

  for (std::vector<Invalidation>::iterator it = invalidations.begin();
       it != invalidations.end(); ++it) {
    EXPECT_TRUE(IsInvalidationUnacknowledged(*it));
  }

  // This invalidation, though arriving the most recently, has the oldest
  // version number so it should be dropped first.
  Invalidation inv5 = SendInvalidation(BOOKMARKS, 5, "old_hint");
  EXPECT_TRUE(IsInvalidationDropped(inv5));

  // This invalidation has a larger version number, so it will force a
  // previously delivered invalidation to be dropped.
  Invalidation inv100 = SendInvalidation(BOOKMARKS, 100, "new_hint");
  EXPECT_TRUE(IsInvalidationDropped(inv10));

  // This should recover from the drop and bring us back into sync.
  RecordSuccessfulSyncCycle();

  for (std::vector<Invalidation>::iterator it = invalidations.begin();
       it != invalidations.end(); ++it) {
    EXPECT_TRUE(IsInvalidationAcknowledged(*it));
  }
  EXPECT_TRUE(IsInvalidationAcknowledged(inv100));

  EXPECT_TRUE(AllInvalidationsAccountedFor());
}

// Test receipt of an unknown version invalidation from the server.
TEST_F(NudgeTrackerAckTrackingTest, UnknownVersionFromServer_Simple) {
  Invalidation inv = SendUnknownVersionInvalidation(BOOKMARKS);
  EXPECT_TRUE(IsInvalidationUnacknowledged(inv));
  RecordSuccessfulSyncCycle();
  EXPECT_TRUE(IsInvalidationAcknowledged(inv));
  EXPECT_TRUE(AllInvalidationsAccountedFor());
}

// Test receipt of multiple unknown version invalidations from the server.
TEST_F(NudgeTrackerAckTrackingTest, UnknownVersionFromServer_Complex) {
  Invalidation inv1 = SendUnknownVersionInvalidation(BOOKMARKS);
  Invalidation inv2 = SendInvalidation(BOOKMARKS, 10, "hint");
  Invalidation inv3 = SendUnknownVersionInvalidation(BOOKMARKS);
  Invalidation inv4 = SendUnknownVersionInvalidation(BOOKMARKS);
  Invalidation inv5 = SendInvalidation(BOOKMARKS, 20, "hint2");

  // These invalidations have been overridden, so they got acked early.
  EXPECT_TRUE(IsInvalidationAcknowledged(inv1));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv3));

  // These invalidations are still waiting to be used.
  EXPECT_TRUE(IsInvalidationUnacknowledged(inv2));
  EXPECT_TRUE(IsInvalidationUnacknowledged(inv4));
  EXPECT_TRUE(IsInvalidationUnacknowledged(inv5));

  // Finish the sync cycle and expect all remaining invalidations to be acked.
  RecordSuccessfulSyncCycle();
  EXPECT_TRUE(IsInvalidationAcknowledged(inv1));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv2));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv3));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv4));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv5));

  EXPECT_TRUE(AllInvalidationsAccountedFor());
}

}  // namespace sessions
}  // namespace syncer
