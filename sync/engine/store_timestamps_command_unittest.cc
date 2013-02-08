// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "sync/engine/store_timestamps_command.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"
#include "sync/test/engine/syncer_command_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

// Adds a progress marker to |response| for the given field number and
// token.
void AddProgressMarkerForFieldNumber(
    sync_pb::GetUpdatesResponse* response,
    int field_number, const std::string& token) {
  sync_pb::DataTypeProgressMarker* marker =
      response->add_new_progress_marker();
  marker->set_data_type_id(field_number);
  marker->set_token(token);
}

// Adds a progress marker to |response| for the given model type and
// token.
void AddProgressMarkerForModelType(
    sync_pb::GetUpdatesResponse* response,
    ModelType model_type, const std::string& token) {
  AddProgressMarkerForFieldNumber(
      response, GetSpecificsFieldNumberFromModelType(model_type), token);
}

class StoreTimestampsCommandTest : public SyncerCommandTest {
 protected:
  // Gets the directory's progress marker's token for the given model
  // type.
  std::string GetProgessMarkerToken(ModelType model_type) {
    sync_pb::DataTypeProgressMarker progress_marker;
    session()->context()->directory()->GetDownloadProgress(
        model_type, &progress_marker);
    EXPECT_EQ(
        GetSpecificsFieldNumberFromModelType(model_type),
        progress_marker.data_type_id());
    return progress_marker.token();
  }
};

// Builds a GetUpdatesResponse with some progress markers, including
// invalid ones.  ProcessNewProgressMarkers() should return the model
// types for the valid progress markers and fill in the progress
// markers in the directory.
TEST_F(StoreTimestampsCommandTest, ProcessNewProgressMarkers) {
  sync_pb::GetUpdatesResponse response;
  AddProgressMarkerForModelType(&response, BOOKMARKS, "token1");
  AddProgressMarkerForModelType(&response,
                                HISTORY_DELETE_DIRECTIVES, "token2");
  AddProgressMarkerForFieldNumber(&response, -1, "bad token");

  ModelTypeSet forward_progress_types =
      ProcessNewProgressMarkers(
          response, session()->context()->directory());

  EXPECT_TRUE(
      forward_progress_types.Equals(
          ModelTypeSet(BOOKMARKS, HISTORY_DELETE_DIRECTIVES)));

  EXPECT_EQ("token1", GetProgessMarkerToken(BOOKMARKS));
  EXPECT_EQ("token2", GetProgessMarkerToken(HISTORY_DELETE_DIRECTIVES));

  ModelTypeSet non_forward_progress_types =
      Difference(ProtocolTypes(), forward_progress_types);
  for (ModelTypeSet::Iterator it = non_forward_progress_types.First();
       it.Good(); it.Inc()) {
    EXPECT_TRUE(GetProgessMarkerToken(it.Get()).empty());
  }
}

}  // namespace

}  // namespace syncer
