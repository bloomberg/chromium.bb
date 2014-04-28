// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/sessions/sync_session_snapshot.h"

#include "base/memory/scoped_ptr.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace sessions {
namespace {

using base::ExpectDictBooleanValue;
using base::ExpectDictDictionaryValue;
using base::ExpectDictIntegerValue;
using base::ExpectDictListValue;
using base::ExpectDictStringValue;

class SyncSessionSnapshotTest : public testing::Test {};

TEST_F(SyncSessionSnapshotTest, SyncSessionSnapshotToValue) {
  ModelNeutralState model_neutral;
  model_neutral.num_successful_commits = 5;
  model_neutral.num_successful_bookmark_commits = 10;
  model_neutral.num_updates_downloaded_total = 100;
  model_neutral.num_tombstone_updates_downloaded_total = 200;
  model_neutral.num_reflected_updates_downloaded_total = 50;
  model_neutral.num_local_overwrites = 15;
  model_neutral.num_server_overwrites = 18;

  ProgressMarkerMap download_progress_markers;
  download_progress_markers[BOOKMARKS] = "test";
  download_progress_markers[APPS] = "apps";
  scoped_ptr<base::DictionaryValue> expected_download_progress_markers_value(
      ProgressMarkerMapToValue(download_progress_markers));

  const bool kIsSilenced = true;
  const int kNumEncryptionConflicts = 1054;
  const int kNumHierarchyConflicts = 1055;
  const int kNumServerConflicts = 1057;

  SyncSessionSnapshot snapshot(model_neutral,
                               download_progress_markers,
                               kIsSilenced,
                               kNumEncryptionConflicts,
                               kNumHierarchyConflicts,
                               kNumServerConflicts,
                               false,
                               0,
                               base::Time::Now(),
                               std::vector<int>(MODEL_TYPE_COUNT,0),
                               std::vector<int>(MODEL_TYPE_COUNT, 0),
                               sync_pb::GetUpdatesCallerInfo::UNKNOWN);
  scoped_ptr<base::DictionaryValue> value(snapshot.ToValue());
  EXPECT_EQ(16u, value->size());
  ExpectDictIntegerValue(model_neutral.num_successful_commits,
                         *value, "numSuccessfulCommits");
  ExpectDictIntegerValue(model_neutral.num_successful_bookmark_commits,
                         *value, "numSuccessfulBookmarkCommits");
  ExpectDictIntegerValue(model_neutral.num_updates_downloaded_total,
                         *value, "numUpdatesDownloadedTotal");
  ExpectDictIntegerValue(model_neutral.num_tombstone_updates_downloaded_total,
                         *value, "numTombstoneUpdatesDownloadedTotal");
  ExpectDictIntegerValue(model_neutral.num_reflected_updates_downloaded_total,
                         *value, "numReflectedUpdatesDownloadedTotal");
  ExpectDictIntegerValue(model_neutral.num_local_overwrites,
                         *value, "numLocalOverwrites");
  ExpectDictIntegerValue(model_neutral.num_server_overwrites,
                         *value, "numServerOverwrites");
  ExpectDictDictionaryValue(*expected_download_progress_markers_value,
                            *value, "downloadProgressMarkers");
  ExpectDictBooleanValue(kIsSilenced, *value, "isSilenced");
  ExpectDictIntegerValue(kNumEncryptionConflicts, *value,
                         "numEncryptionConflicts");
  ExpectDictIntegerValue(kNumHierarchyConflicts, *value,
                         "numHierarchyConflicts");
  ExpectDictIntegerValue(kNumServerConflicts, *value,
                         "numServerConflicts");
  ExpectDictBooleanValue(false, *value, "notificationsEnabled");
}

}  // namespace
}  // namespace sessions
}  // namespace syncer
