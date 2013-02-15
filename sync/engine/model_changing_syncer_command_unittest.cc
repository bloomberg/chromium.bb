// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "sync/engine/model_changing_syncer_command.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/sessions/sync_session.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/syncer_command_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class FakeModelChangingSyncerCommand : public ModelChangingSyncerCommand {
 public:
  FakeModelChangingSyncerCommand() {}
  virtual ~FakeModelChangingSyncerCommand() {}

  const std::set<ModelSafeGroup>& changed_groups() const {
    return changed_groups_;
  }

 protected:
  virtual std::set<ModelSafeGroup> GetGroupsToChange(
      const sessions::SyncSession& session) const OVERRIDE {
    // This command doesn't actually make changes, so the empty set might be an
    // appropriate response.  That would not be very interesting, so instead we
    // return the set of groups with active types.
    std::set<ModelSafeGroup> enabled_groups;
    const ModelSafeRoutingInfo& routing_info =
        session.context()->routing_info();
    for (ModelSafeRoutingInfo::const_iterator it = routing_info.begin();
         it != routing_info.end(); ++it) {
      enabled_groups.insert(it->second);
    }
    enabled_groups.insert(GROUP_PASSIVE);
    return enabled_groups;
  }

  virtual SyncerError ModelChangingExecuteImpl(
      sessions::SyncSession* session) OVERRIDE {
    changed_groups_.insert(session->status_controller().group_restriction());
    return SYNCER_OK;
  }

 private:
  std::set<ModelSafeGroup> changed_groups_;

  DISALLOW_COPY_AND_ASSIGN(FakeModelChangingSyncerCommand);
};

class ModelChangingSyncerCommandTest : public SyncerCommandTest {
 protected:
  ModelChangingSyncerCommandTest() {}
  virtual ~ModelChangingSyncerCommandTest() {}

  virtual void SetUp() {
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_UI)));
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_PASSWORD)));
    (*mutable_routing_info())[BOOKMARKS] = GROUP_UI;
    (*mutable_routing_info())[PASSWORDS] = GROUP_PASSWORD;
    SyncerCommandTest::SetUp();
  }

  FakeModelChangingSyncerCommand command_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ModelChangingSyncerCommandTest);
};

TEST_F(ModelChangingSyncerCommandTest, Basic) {
  ExpectGroupsToChange(command_, GROUP_UI, GROUP_PASSWORD, GROUP_PASSIVE);
  EXPECT_TRUE(command_.changed_groups().empty());
  command_.ExecuteImpl(session());
  EXPECT_EQ(command_.GetGroupsToChangeForTest(*session()),
            command_.changed_groups());
}

}  // namespace

}  // namespace syncer
