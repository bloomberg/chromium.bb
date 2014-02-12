// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/js_sync_manager_observer.h"

#include "base/basictypes.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"
#include "sync/internal_api/public/util/sync_string_conversions.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/js/js_event_details.h"
#include "sync/js/js_test_util.h"
#include "sync/protocol/sync_protocol_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using ::testing::InSequence;
using ::testing::StrictMock;

class JsSyncManagerObserverTest : public testing::Test {
 protected:
  JsSyncManagerObserverTest() {
    js_sync_manager_observer_.SetJsEventHandler(
        mock_js_event_handler_.AsWeakHandle());
  }

 private:
  // This must be destroyed after the member variables below in order
  // for WeakHandles to be destroyed properly.
  base::MessageLoop message_loop_;

 protected:
  StrictMock<MockJsEventHandler> mock_js_event_handler_;
  JsSyncManagerObserver js_sync_manager_observer_;

  void PumpLoop() {
    message_loop_.RunUntilIdle();
  }
};

TEST_F(JsSyncManagerObserverTest, OnInitializationComplete) {
  base::DictionaryValue expected_details;
  syncer::ModelTypeSet restored_types;
  restored_types.Put(BOOKMARKS);
  restored_types.Put(NIGORI);
  expected_details.Set("restoredTypes", ModelTypeSetToValue(restored_types));

  EXPECT_CALL(mock_js_event_handler_,
              HandleJsEvent("onInitializationComplete",
                            HasDetailsAsDictionary(expected_details)));

  js_sync_manager_observer_.OnInitializationComplete(
      WeakHandle<JsBackend>(),
      WeakHandle<DataTypeDebugInfoListener>(),
      true,
      restored_types);
  PumpLoop();
}

TEST_F(JsSyncManagerObserverTest, OnSyncCycleCompleted) {
  sessions::SyncSessionSnapshot snapshot(
      sessions::ModelNeutralState(),
      ProgressMarkerMap(),
      false,
      5,
      2,
      7,
      false,
      0,
      base::Time::Now(),
      std::vector<int>(MODEL_TYPE_COUNT, 0),
      std::vector<int>(MODEL_TYPE_COUNT, 0),
      sync_pb::GetUpdatesCallerInfo::UNKNOWN);
  base::DictionaryValue expected_details;
  expected_details.Set("snapshot", snapshot.ToValue());

  EXPECT_CALL(mock_js_event_handler_,
              HandleJsEvent("onSyncCycleCompleted",
                            HasDetailsAsDictionary(expected_details)));

  js_sync_manager_observer_.OnSyncCycleCompleted(snapshot);
  PumpLoop();
}

TEST_F(JsSyncManagerObserverTest, OnActionableError) {
  SyncProtocolError sync_error;
  sync_error.action = CLEAR_USER_DATA_AND_RESYNC;
  sync_error.error_type = TRANSIENT_ERROR;
  base::DictionaryValue expected_details;
  expected_details.Set("syncError", sync_error.ToValue());

  EXPECT_CALL(mock_js_event_handler_,
              HandleJsEvent("onActionableError",
                           HasDetailsAsDictionary(expected_details)));

  js_sync_manager_observer_.OnActionableError(sync_error);
  PumpLoop();
}


TEST_F(JsSyncManagerObserverTest, OnConnectionStatusChange) {
  const ConnectionStatus kStatus = CONNECTION_AUTH_ERROR;
  base::DictionaryValue expected_details;
  expected_details.SetString("status",
                             ConnectionStatusToString(kStatus));

  EXPECT_CALL(mock_js_event_handler_,
              HandleJsEvent("onConnectionStatusChange",
                            HasDetailsAsDictionary(expected_details)));

  js_sync_manager_observer_.OnConnectionStatusChange(kStatus);
  PumpLoop();
}

}  // namespace
}  // namespace syncer
