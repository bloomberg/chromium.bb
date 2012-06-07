// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/session_state.h"

#include <string>

#include "base/base64.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/values_test_util.h"
#include "base/time.h"
#include "base/values.h"
#include "sync/internal_api/public/sessions/error_counters.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"
#include "sync/internal_api/public/sessions/sync_source_info.h"
#include "sync/internal_api/public/sessions/syncer_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace sessions {
namespace {

using base::ExpectDictBooleanValue;
using base::ExpectDictDictionaryValue;
using base::ExpectDictIntegerValue;
using base::ExpectDictListValue;
using base::ExpectDictStringValue;

class SessionStateTest : public testing::Test {};

TEST_F(SessionStateTest, SyncSourceInfoToValue) {
  sync_pb::GetUpdatesCallerInfo::GetUpdatesSource updates_source =
      sync_pb::GetUpdatesCallerInfo::PERIODIC;
  syncable::ModelTypePayloadMap types;
  types[syncable::PREFERENCES] = "preferencespayload";
  types[syncable::EXTENSIONS] = "";
  scoped_ptr<DictionaryValue> expected_types_value(
      syncable::ModelTypePayloadMapToValue(types));

  SyncSourceInfo source_info(updates_source, types);

  scoped_ptr<DictionaryValue> value(source_info.ToValue());
  EXPECT_EQ(2u, value->size());
  ExpectDictStringValue("PERIODIC", *value, "updatesSource");
  ExpectDictDictionaryValue(*expected_types_value, *value, "types");
}

TEST_F(SessionStateTest, SyncerStatusToValue) {
  SyncerStatus status;
  status.invalid_store = true;
  status.num_successful_commits = 5;
  status.num_successful_bookmark_commits = 10;
  status.num_updates_downloaded_total = 100;
  status.num_tombstone_updates_downloaded_total = 200;
  status.num_reflected_updates_downloaded_total = 50;
  status.num_local_overwrites = 15;
  status.num_server_overwrites = 18;

  scoped_ptr<DictionaryValue> value(status.ToValue());
  EXPECT_EQ(8u, value->size());
  ExpectDictBooleanValue(status.invalid_store, *value, "invalidStore");
  ExpectDictIntegerValue(status.num_successful_commits,
                         *value, "numSuccessfulCommits");
  ExpectDictIntegerValue(status.num_successful_bookmark_commits,
                         *value, "numSuccessfulBookmarkCommits");
  ExpectDictIntegerValue(status.num_updates_downloaded_total,
                         *value, "numUpdatesDownloadedTotal");
  ExpectDictIntegerValue(status.num_tombstone_updates_downloaded_total,
                         *value, "numTombstoneUpdatesDownloadedTotal");
  ExpectDictIntegerValue(status.num_reflected_updates_downloaded_total,
                         *value, "numReflectedUpdatesDownloadedTotal");
  ExpectDictIntegerValue(status.num_local_overwrites,
                         *value, "numLocalOverwrites");
  ExpectDictIntegerValue(status.num_server_overwrites,
                         *value, "numServerOverwrites");
}

TEST_F(SessionStateTest, SyncSessionSnapshotToValue) {
  SyncerStatus syncer_status;
  syncer_status.num_successful_commits = 500;
  scoped_ptr<DictionaryValue> expected_syncer_status_value(
      syncer_status.ToValue());

  ErrorCounters errors;

  const int kNumServerChangesRemaining = 105;
  const bool kIsShareUsable = true;

  const syncable::ModelTypeSet initial_sync_ended(
      syncable::BOOKMARKS, syncable::PREFERENCES);
  scoped_ptr<ListValue> expected_initial_sync_ended_value(
      syncable::ModelTypeSetToValue(initial_sync_ended));

  syncable::ModelTypePayloadMap download_progress_markers;
  download_progress_markers[syncable::BOOKMARKS] = "test";
  download_progress_markers[syncable::APPS] = "apps";
  scoped_ptr<DictionaryValue> expected_download_progress_markers_value(
      syncable::ModelTypePayloadMapToValue(download_progress_markers));

  const bool kHasMoreToSync = false;
  const bool kIsSilenced = true;
  const int kNumEncryptionConflicts = 1054;
  const int kNumHierarchyConflicts = 1055;
  const int kNumSimpleConflicts = 1056;
  const int kNumServerConflicts = 1057;

  SyncSourceInfo source;
  scoped_ptr<DictionaryValue> expected_source_value(source.ToValue());

  SyncSessionSnapshot snapshot(syncer_status,
                               errors,
                               kNumServerChangesRemaining,
                               kIsShareUsable,
                               initial_sync_ended,
                               download_progress_markers,
                               kHasMoreToSync,
                               kIsSilenced,
                               kNumEncryptionConflicts,
                               kNumHierarchyConflicts,
                               kNumSimpleConflicts,
                               kNumServerConflicts,
                               source,
                               false,
                               0,
                               base::Time::Now(),
                               false);
  scoped_ptr<DictionaryValue> value(snapshot.ToValue());
  EXPECT_EQ(14u, value->size());
  ExpectDictDictionaryValue(*expected_syncer_status_value, *value,
                            "syncerStatus");
  ExpectDictIntegerValue(kNumServerChangesRemaining, *value,
                         "numServerChangesRemaining");
  ExpectDictBooleanValue(kIsShareUsable, *value, "isShareUsable");
  ExpectDictListValue(*expected_initial_sync_ended_value, *value,
                      "initialSyncEnded");
  ExpectDictDictionaryValue(*expected_download_progress_markers_value,
                            *value, "downloadProgressMarkers");
  ExpectDictBooleanValue(kHasMoreToSync, *value, "hasMoreToSync");
  ExpectDictBooleanValue(kIsSilenced, *value, "isSilenced");
  ExpectDictIntegerValue(kNumEncryptionConflicts, *value,
                         "numEncryptionConflicts");
  ExpectDictIntegerValue(kNumHierarchyConflicts, *value,
                         "numHierarchyConflicts");
  ExpectDictIntegerValue(kNumSimpleConflicts, *value,
                         "numSimpleConflicts");
  ExpectDictIntegerValue(kNumServerConflicts, *value,
                         "numServerConflicts");
  ExpectDictDictionaryValue(*expected_source_value, *value, "source");
  ExpectDictBooleanValue(false, *value, "notificationsEnabled");
}

}  // namespace
}  // namespace sessions
}  // namespace browser_sync
