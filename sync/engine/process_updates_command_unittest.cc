// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "sync/engine/process_updates_command.h"
#include "sync/internal_api/public/syncable/model_type.h"
#include "sync/sessions/session_state.h"
#include "sync/sessions/sync_session.h"
#include "sync/syncable/syncable_id.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/syncer_command_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class ProcessUpdatesCommandTest : public SyncerCommandTest {
 protected:
  ProcessUpdatesCommandTest() {}
  virtual ~ProcessUpdatesCommandTest() {}

  virtual void SetUp() {
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_UI)));
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_DB)));
    (*mutable_routing_info())[syncable::BOOKMARKS] = GROUP_UI;
    (*mutable_routing_info())[syncable::AUTOFILL] = GROUP_DB;
    SyncerCommandTest::SetUp();
  }

  ProcessUpdatesCommand command_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessUpdatesCommandTest);
};

TEST_F(ProcessUpdatesCommandTest, GetGroupsToChange) {
  ExpectNoGroupsToChange(command_);
  // Add a verified update for GROUP_DB.
  session()->mutable_status_controller()->
      GetUnrestrictedMutableUpdateProgressForTest(GROUP_DB)->
      AddVerifyResult(VerifyResult(), sync_pb::SyncEntity());
  ExpectGroupToChange(command_, GROUP_DB);
}

}  // namespace

}  // namespace syncer
