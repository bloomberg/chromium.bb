// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/nudge_tracker.h"

#include "sync/internal_api/public/base/model_type_invalidation_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

ModelTypeSet ParamsMeaningAllEnabledTypes() {
  ModelTypeSet request_params(BOOKMARKS, AUTOFILL);
  return request_params;
}

ModelTypeSet ParamsMeaningJustOneEnabledType() {
  return ModelTypeSet(AUTOFILL);
}

}  // namespace

namespace sessions {

TEST(NudgeTrackerTest, CoalesceSources) {
  ModelTypeInvalidationMap one_type =
      ModelTypeSetToInvalidationMap(
          ParamsMeaningJustOneEnabledType(),
          std::string());
  ModelTypeInvalidationMap all_types =
      ModelTypeSetToInvalidationMap(
          ParamsMeaningAllEnabledTypes(),
          std::string());
  sessions::SyncSourceInfo source_one(
      sync_pb::GetUpdatesCallerInfo::NOTIFICATION, one_type);
  sessions::SyncSourceInfo source_two(
      sync_pb::GetUpdatesCallerInfo::LOCAL, all_types);

  NudgeTracker tracker;
  EXPECT_TRUE(tracker.IsEmpty());

  tracker.CoalesceSources(source_one);
  EXPECT_EQ(source_one.updates_source, tracker.source_info().updates_source);

  tracker.CoalesceSources(source_two);
  EXPECT_EQ(source_two.updates_source, tracker.source_info().updates_source);
}

TEST(NudgeTrackerTest, LocallyModifiedTypes_WithInvalidationFirst) {
  ModelTypeInvalidationMap one_type =
      ModelTypeSetToInvalidationMap(
          ParamsMeaningJustOneEnabledType(),
          std::string());
  ModelTypeInvalidationMap all_types =
      ModelTypeSetToInvalidationMap(
          ParamsMeaningAllEnabledTypes(),
          std::string());
  sessions::SyncSourceInfo source_one(
      sync_pb::GetUpdatesCallerInfo::NOTIFICATION, all_types);
  sessions::SyncSourceInfo source_two(
      sync_pb::GetUpdatesCallerInfo::LOCAL, one_type);

  NudgeTracker tracker;
  EXPECT_TRUE(tracker.IsEmpty());
  EXPECT_TRUE(tracker.GetLocallyModifiedTypes().Empty());

  tracker.CoalesceSources(source_one);
  EXPECT_TRUE(tracker.GetLocallyModifiedTypes().Empty());

  tracker.CoalesceSources(source_two);
  // TODO: This result is wrong, but that's how the code has always been.  A
  // local invalidation for a single type should mean that we have only one
  // locally modified source.  It should not "inherit" the list of data types
  // from the previous source.
  EXPECT_TRUE(tracker.GetLocallyModifiedTypes().Equals(
          ParamsMeaningAllEnabledTypes()));
}

TEST(NudgeTrackerTest, LocallyModifiedTypes_WithInvalidationSecond) {
  ModelTypeInvalidationMap one_type =
      ModelTypeSetToInvalidationMap(
          ParamsMeaningJustOneEnabledType(),
          std::string());
  ModelTypeInvalidationMap all_types =
      ModelTypeSetToInvalidationMap(
          ParamsMeaningAllEnabledTypes(),
          std::string());
  sessions::SyncSourceInfo source_one(
      sync_pb::GetUpdatesCallerInfo::LOCAL, one_type);
  sessions::SyncSourceInfo source_two(
      sync_pb::GetUpdatesCallerInfo::NOTIFICATION, all_types);

  NudgeTracker tracker;
  EXPECT_TRUE(tracker.IsEmpty());
  EXPECT_TRUE(tracker.GetLocallyModifiedTypes().Empty());

  tracker.CoalesceSources(source_one);
  EXPECT_TRUE(tracker.GetLocallyModifiedTypes().Equals(
          ParamsMeaningJustOneEnabledType()));

  tracker.CoalesceSources(source_two);

  // TODO: This result is wrong, but that's how the code has always been.
  // The receipt of an invalidation should have no effect on the set of
  // locally modified types.
  EXPECT_TRUE(tracker.GetLocallyModifiedTypes().Empty());
}

}  // namespace sessions
}  // namespace syncer
