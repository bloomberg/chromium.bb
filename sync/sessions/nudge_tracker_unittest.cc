// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/nudge_tracker.h"

#include "sync/internal_api/public/base/model_type_invalidation_map.h"
#include "sync/internal_api/public/sessions/sync_source_info.h"
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
  static size_t GetHintBufferSize() {
    // Assumes that no test has adjusted this size.
    return NudgeTracker::kDefaultMaxPayloadsPerType;
  }

  bool InvalidationsOutOfSync(const NudgeTracker& nudge_tracker) {
    // We don't currently track invalidations out of sync on a per-type basis.
    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker.FillProtoMessage(BOOKMARKS, &gu_trigger);
    return gu_trigger.invalidations_out_of_sync();
  }

  int ProtoLocallyModifiedCount(const NudgeTracker& nudge_tracker,
                                ModelType type) {
    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker.FillProtoMessage(type, &gu_trigger);
    return gu_trigger.local_modification_nudges();
  }

  int ProtoRefreshRequestedCount(const NudgeTracker& nudge_tracker,
                                 ModelType type) {
    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker.FillProtoMessage(type, &gu_trigger);
    return gu_trigger.datatype_refresh_nudges();
  }
};

// Exercise an empty NudgeTracker.
// Use with valgrind to detect uninitialized members.
TEST_F(NudgeTrackerTest, EmptyNudgeTracker) {
  NudgeTracker nudge_tracker;

  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::UNKNOWN,
            nudge_tracker.updates_source());
  EXPECT_TRUE(nudge_tracker.GetLocallyModifiedTypes().Empty());

  sync_pb::GetUpdateTriggers gu_trigger;
  nudge_tracker.FillProtoMessage(BOOKMARKS, &gu_trigger);

  SyncSourceInfo source_info = nudge_tracker.GetSourceInfo();
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::UNKNOWN,
            source_info.updates_source);
}

// Verify that nudges override each other based on a priority order.
// LOCAL < DATATYPE_REFRESH < NOTIFICATION
TEST_F(NudgeTrackerTest, SourcePriorities) {
  NudgeTracker nudge_tracker;

  // Track a local nudge.
  nudge_tracker.RecordLocalChange(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::LOCAL,
            nudge_tracker.updates_source());

  // A refresh request will override it.
  nudge_tracker.RecordLocalRefreshRequest(ModelTypeSet(TYPED_URLS));
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::DATATYPE_REFRESH,
            nudge_tracker.updates_source());

  // Another local nudge will not be enough to change it.
  nudge_tracker.RecordLocalChange(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::DATATYPE_REFRESH,
            nudge_tracker.updates_source());

  // An invalidation will override the refresh request source.
  ModelTypeInvalidationMap invalidation_map =
      ModelTypeSetToInvalidationMap(ModelTypeSet(PREFERENCES),
                                    std::string("hint"));
  nudge_tracker.RecordRemoteInvalidation(invalidation_map);
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::NOTIFICATION,
            nudge_tracker.updates_source());

  // Neither local nudges nor refresh requests will override it.
  nudge_tracker.RecordLocalChange(ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::NOTIFICATION,
            nudge_tracker.updates_source());
  nudge_tracker.RecordLocalRefreshRequest(ModelTypeSet(TYPED_URLS));
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::NOTIFICATION,
            nudge_tracker.updates_source());
}

// Verify locally modified type coalescing and independence from other nudges.
TEST_F(NudgeTrackerTest, LocallyModifiedTypes) {
  NudgeTracker nudge_tracker;

  // Start with a notification.  Verify it has no effect.
  ModelTypeInvalidationMap invalidation_map1 =
      ModelTypeSetToInvalidationMap(ModelTypeSet(PREFERENCES),
                                    std::string("hint"));
  nudge_tracker.RecordRemoteInvalidation(invalidation_map1);
  EXPECT_TRUE(nudge_tracker.GetLocallyModifiedTypes().Empty());

  // Record a local bookmark change.  Verify it was registered correctly.
  nudge_tracker.RecordLocalChange(ModelTypeSet(BOOKMARKS));
  EXPECT_TRUE(ModelTypeSetEquals(
          ModelTypeSet(BOOKMARKS),
          nudge_tracker.GetLocallyModifiedTypes()));

  // Record a notification and a refresh request.  Verify they have no effect.
  ModelTypeInvalidationMap invalidation_map2 =
      ModelTypeSetToInvalidationMap(ModelTypeSet(PASSWORDS),
                                    std::string("hint"));
  nudge_tracker.RecordRemoteInvalidation(invalidation_map2);
  EXPECT_TRUE(ModelTypeSetEquals(
          ModelTypeSet(BOOKMARKS),
          nudge_tracker.GetLocallyModifiedTypes()));

  nudge_tracker.RecordLocalRefreshRequest(ModelTypeSet(AUTOFILL));
  EXPECT_TRUE(ModelTypeSetEquals(
          ModelTypeSet(BOOKMARKS),
          nudge_tracker.GetLocallyModifiedTypes()));

  // Record another local nudge.  Verify it was coalesced correctly.
  nudge_tracker.RecordLocalChange(ModelTypeSet(THEMES));
  EXPECT_TRUE(ModelTypeSetEquals(
          ModelTypeSet(THEMES, BOOKMARKS),
          nudge_tracker.GetLocallyModifiedTypes()));
}

TEST_F(NudgeTrackerTest, HintCoalescing) {
  NudgeTracker nudge_tracker;

  // Easy case: record one hint.
  {
    ModelTypeInvalidationMap invalidation_map =
        ModelTypeSetToInvalidationMap(ModelTypeSet(BOOKMARKS),
                                      std::string("bm_hint_1"));
    nudge_tracker.RecordRemoteInvalidation(invalidation_map);

    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker.FillProtoMessage(BOOKMARKS, &gu_trigger);
    ASSERT_EQ(1, gu_trigger.notification_hint_size());
    EXPECT_EQ("bm_hint_1", gu_trigger.notification_hint(0));
    EXPECT_FALSE(gu_trigger.client_dropped_hints());
  }

  // Record a second hint for the same type.
  {
    ModelTypeInvalidationMap invalidation_map =
        ModelTypeSetToInvalidationMap(ModelTypeSet(BOOKMARKS),
                                      std::string("bm_hint_2"));
    nudge_tracker.RecordRemoteInvalidation(invalidation_map);

    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker.FillProtoMessage(BOOKMARKS, &gu_trigger);
    ASSERT_EQ(2, gu_trigger.notification_hint_size());

    // Expect the most hint recent is last in the list.
    EXPECT_EQ("bm_hint_1", gu_trigger.notification_hint(0));
    EXPECT_EQ("bm_hint_2", gu_trigger.notification_hint(1));
    EXPECT_FALSE(gu_trigger.client_dropped_hints());
  }

  // Record a hint for a different type.
  {
    ModelTypeInvalidationMap invalidation_map =
        ModelTypeSetToInvalidationMap(ModelTypeSet(PASSWORDS),
                                      std::string("pw_hint_1"));
    nudge_tracker.RecordRemoteInvalidation(invalidation_map);

    // Re-verify the bookmarks to make sure they're unaffected.
    sync_pb::GetUpdateTriggers bm_gu_trigger;
    nudge_tracker.FillProtoMessage(BOOKMARKS, &bm_gu_trigger);
    ASSERT_EQ(2, bm_gu_trigger.notification_hint_size());
    EXPECT_EQ("bm_hint_1", bm_gu_trigger.notification_hint(0));
    EXPECT_EQ("bm_hint_2",
              bm_gu_trigger.notification_hint(1)); // most recent last.
    EXPECT_FALSE(bm_gu_trigger.client_dropped_hints());

    // Verify the new type, too.
    sync_pb::GetUpdateTriggers pw_gu_trigger;
    nudge_tracker.FillProtoMessage(PASSWORDS, &pw_gu_trigger);
    ASSERT_EQ(1, pw_gu_trigger.notification_hint_size());
    EXPECT_EQ("pw_hint_1", pw_gu_trigger.notification_hint(0));
    EXPECT_FALSE(pw_gu_trigger.client_dropped_hints());
  }
}

TEST_F(NudgeTrackerTest, DropHintsLocally) {
  NudgeTracker nudge_tracker;
  ModelTypeInvalidationMap invalidation_map =
      ModelTypeSetToInvalidationMap(ModelTypeSet(BOOKMARKS),
                                    std::string("hint"));

  for (size_t i = 0; i < GetHintBufferSize(); ++i) {
    nudge_tracker.RecordRemoteInvalidation(invalidation_map);
  }
  {
    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker.FillProtoMessage(BOOKMARKS, &gu_trigger);
    EXPECT_EQ(GetHintBufferSize(),
              static_cast<size_t>(gu_trigger.notification_hint_size()));
    EXPECT_FALSE(gu_trigger.client_dropped_hints());
  }

  // Force an overflow.
  ModelTypeInvalidationMap invalidation_map2 =
      ModelTypeSetToInvalidationMap(ModelTypeSet(BOOKMARKS),
                                    std::string("new_hint"));
  nudge_tracker.RecordRemoteInvalidation(invalidation_map2);

  {
    sync_pb::GetUpdateTriggers gu_trigger;
    nudge_tracker.FillProtoMessage(BOOKMARKS, &gu_trigger);
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
  NudgeTracker nudge_tracker;

  // By default, assume we're out of sync with the invalidation server.
  EXPECT_TRUE(InvalidationsOutOfSync(nudge_tracker));

  // Simply enabling invalidations does not bring us back into sync.
  nudge_tracker.OnInvalidationsEnabled();
  EXPECT_TRUE(InvalidationsOutOfSync(nudge_tracker));

  // We must successfully complete a sync cycle while invalidations are enabled
  // to be sure that we're in sync.
  nudge_tracker.RecordSuccessfulSyncCycle();
  EXPECT_FALSE(InvalidationsOutOfSync(nudge_tracker));

  // If the invalidator malfunctions, we go become unsynced again.
  nudge_tracker.OnInvalidationsDisabled();
  EXPECT_TRUE(InvalidationsOutOfSync(nudge_tracker));

  // A sync cycle while invalidations are disabled won't reset the flag.
  nudge_tracker.RecordSuccessfulSyncCycle();
  EXPECT_TRUE(InvalidationsOutOfSync(nudge_tracker));

  // Nor will the re-enabling of invalidations be sufficient, even now that
  // we've had a successful sync cycle.
  nudge_tracker.RecordSuccessfulSyncCycle();
  EXPECT_TRUE(InvalidationsOutOfSync(nudge_tracker));
}

// Tests that locally modified types are correctly written out to the
// GetUpdateTriggers proto.
TEST_F(NudgeTrackerTest, WriteLocallyModifiedTypesToProto) {
  NudgeTracker nudge_tracker;

  // Should not be locally modified by default.
  EXPECT_EQ(0, ProtoLocallyModifiedCount(nudge_tracker, PREFERENCES));

  // Record a local bookmark change.  Verify it was registered correctly.
  nudge_tracker.RecordLocalChange(ModelTypeSet(PREFERENCES));
  EXPECT_EQ(1, ProtoLocallyModifiedCount(nudge_tracker, PREFERENCES));

  // Record a successful sync cycle.  Verify the count is cleared.
  nudge_tracker.RecordSuccessfulSyncCycle();
  EXPECT_EQ(0, ProtoLocallyModifiedCount(nudge_tracker, PREFERENCES));
}

// Tests that refresh requested types are correctly written out to the
// GetUpdateTriggers proto.
TEST_F(NudgeTrackerTest, WriteRefreshRequestedTypesToProto) {
  NudgeTracker nudge_tracker;

  // There should be no refresh requested by default.
  EXPECT_EQ(0, ProtoRefreshRequestedCount(nudge_tracker, SESSIONS));

  // Record a local refresh request.  Verify it was registered correctly.
  nudge_tracker.RecordLocalRefreshRequest(ModelTypeSet(SESSIONS));
  EXPECT_EQ(1, ProtoRefreshRequestedCount(nudge_tracker, SESSIONS));

  // Record a successful sync cycle.  Verify the count is cleared.
  nudge_tracker.RecordSuccessfulSyncCycle();
  EXPECT_EQ(0, ProtoRefreshRequestedCount(nudge_tracker, SESSIONS));
}

}  // namespace sessions
}  // namespace syncer
