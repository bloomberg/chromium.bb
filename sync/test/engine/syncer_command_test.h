// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_ENGINE_SYNCER_COMMAND_TEST_H_
#define SYNC_TEST_ENGINE_SYNCER_COMMAND_TEST_H_

#include <algorithm>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "sync/engine/model_changing_syncer_command.h"
#include "sync/engine/traffic_recorder.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/sessions/debug_info_getter.h"
#include "sync/sessions/sync_session.h"
#include "sync/sessions/sync_session_context.h"
#include "sync/syncable/directory.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/mock_connection_manager.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "sync/util/extensions_activity.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;

namespace syncer {

class MockDebugInfoGetter : public sessions::DebugInfoGetter {
 public:
  MockDebugInfoGetter();
  virtual ~MockDebugInfoGetter();
  MOCK_METHOD1(GetAndClearDebugInfo, void(sync_pb::DebugInfo* debug_info));
};

// A test fixture that simplifies writing unit tests for individual
// SyncerCommands, providing convenient access to a test directory
// and a syncer session.
class SyncerCommandTestBase : public testing::Test,
                              public sessions::SyncSession::Delegate {
 public:
  // SyncSession::Delegate implementation.
  virtual void OnThrottled(
      const base::TimeDelta& throttle_duration) OVERRIDE {
    FAIL() << "Should not get silenced.";
  }
  virtual void OnTypesThrottled(
      ModelTypeSet types,
      const base::TimeDelta& throttle_duration) OVERRIDE {
    FAIL() << "Should not get silenced.";
  }
  virtual bool IsCurrentlyThrottled() OVERRIDE {
    return false;
  }
  virtual void OnReceivedLongPollIntervalUpdate(
      const base::TimeDelta& new_interval) OVERRIDE {
    FAIL() << "Should not get poll interval update.";
  }
  virtual void OnReceivedShortPollIntervalUpdate(
      const base::TimeDelta& new_interval) OVERRIDE {
    FAIL() << "Should not get poll interval update.";
  }
  virtual void OnReceivedSessionsCommitDelay(
      const base::TimeDelta& new_delay) OVERRIDE {
    FAIL() << "Should not get sessions commit delay.";
  }
  virtual void OnReceivedClientInvalidationHintBufferSize(int size) OVERRIDE {
    FAIL() << "Should not get hint buffer size.";
  }
  virtual void OnShouldStopSyncingPermanently() OVERRIDE {
    FAIL() << "Shouldn't be called.";
  }
  virtual void OnSyncProtocolError(
      const sessions::SyncSessionSnapshot& session) OVERRIDE {
    return;
  }

  std::vector<ModelSafeWorker*> GetWorkers() {
    std::vector<ModelSafeWorker*> workers;
    std::vector<scoped_refptr<ModelSafeWorker> >::iterator it;
    for (it = workers_.begin(); it != workers_.end(); ++it)
      workers.push_back(it->get());
    return workers;
  }
  void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) {
    ModelSafeRoutingInfo copy(routing_info_);
    out->swap(copy);
  }

 protected:
  SyncerCommandTestBase();

  virtual ~SyncerCommandTestBase();
  virtual void SetUp();
  virtual void TearDown();

  sessions::SyncSessionContext* context() const { return context_.get(); }
  sessions::SyncSession::Delegate* delegate() { return this; }

  // Lazily create a session requesting all datatypes with no state.
  sessions::SyncSession* session() {
    if (!session_.get())
      session_.reset(sessions::SyncSession::Build(context(), delegate()));
    return session_.get();
  }

  void ClearSession() {
    session_.reset();
  }

  void ResetContext() {
    context_.reset(new sessions::SyncSessionContext(
            mock_server_.get(), directory(),
            GetWorkers(), extensions_activity_.get(),
            std::vector<SyncEngineEventListener*>(),
            &mock_debug_info_getter_,
            &traffic_recorder_,
            true,  // enable keystore encryption
            false,  // force enable pre-commit GU avoidance experiment
            "fake_invalidator_client_id"));
    context_->set_routing_info(routing_info_);
    context_->set_account_name(directory()->name());
    ClearSession();
  }

  // Install a MockServerConnection.  Resets the context.  By default,
  // the context does not have a MockServerConnection attached.
  void ConfigureMockServerConnection() {
    mock_server_.reset(new MockConnectionManager(directory()));
    ResetContext();
  }

  virtual syncable::Directory* directory() = 0;

  std::vector<scoped_refptr<ModelSafeWorker> >* workers() {
    return &workers_;
  }

  const ModelSafeRoutingInfo& routing_info() { return routing_info_; }
  ModelSafeRoutingInfo* mutable_routing_info() { return &routing_info_; }

  MockConnectionManager* mock_server() {
    return mock_server_.get();
  }

  MockDebugInfoGetter* mock_debug_info_getter() {
    return &mock_debug_info_getter_;
  }

  // Helper functions to check command.GetGroupsToChange().

  void ExpectNoGroupsToChange(const ModelChangingSyncerCommand& command) {
    EXPECT_TRUE(command.GetGroupsToChangeForTest(*session()).empty());
  }

  void ExpectGroupToChange(
      const ModelChangingSyncerCommand& command, ModelSafeGroup group) {
    std::set<ModelSafeGroup> expected_groups_to_change;
    expected_groups_to_change.insert(group);
    EXPECT_EQ(expected_groups_to_change,
              command.GetGroupsToChangeForTest(*session()));
  }

  void ExpectGroupsToChange(
      const ModelChangingSyncerCommand& command,
      ModelSafeGroup group1, ModelSafeGroup group2) {
    std::set<ModelSafeGroup> expected_groups_to_change;
    expected_groups_to_change.insert(group1);
    expected_groups_to_change.insert(group2);
    EXPECT_EQ(expected_groups_to_change,
              command.GetGroupsToChangeForTest(*session()));
  }

  void ExpectGroupsToChange(
      const ModelChangingSyncerCommand& command,
      ModelSafeGroup group1, ModelSafeGroup group2, ModelSafeGroup group3) {
    std::set<ModelSafeGroup> expected_groups_to_change;
    expected_groups_to_change.insert(group1);
    expected_groups_to_change.insert(group2);
    expected_groups_to_change.insert(group3);
    EXPECT_EQ(expected_groups_to_change,
              command.GetGroupsToChangeForTest(*session()));
  }

 private:
  base::MessageLoop message_loop_;
  scoped_ptr<sessions::SyncSessionContext> context_;
  scoped_ptr<MockConnectionManager> mock_server_;
  scoped_ptr<sessions::SyncSession> session_;
  std::vector<scoped_refptr<ModelSafeWorker> > workers_;
  ModelSafeRoutingInfo routing_info_;
  NiceMock<MockDebugInfoGetter> mock_debug_info_getter_;
  scoped_refptr<ExtensionsActivity> extensions_activity_;
  TrafficRecorder traffic_recorder_;
  DISALLOW_COPY_AND_ASSIGN(SyncerCommandTestBase);
};

class SyncerCommandTest : public SyncerCommandTestBase {
 public:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;
  virtual syncable::Directory* directory() OVERRIDE;

 private:
  TestDirectorySetterUpper dir_maker_;
};

}  // namespace syncer

#endif  // SYNC_TEST_ENGINE_SYNCER_COMMAND_TEST_H_
