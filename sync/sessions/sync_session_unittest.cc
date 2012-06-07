// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/sync_session.h"

#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "sync/engine/conflict_resolver.h"
#include "sync/engine/syncer_types.h"
#include "sync/internal_api/public/syncable/model_type.h"
#include "sync/sessions/session_state.h"
#include "sync/sessions/status_controller.h"
#include "sync/syncable/syncable.h"
#include "sync/syncable/syncable_id.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "sync/test/fake_extensions_activity_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncable::WriteTransaction;

namespace browser_sync {
namespace sessions {
namespace {

class SyncSessionTest : public testing::Test,
                        public SyncSession::Delegate {
 public:
  SyncSessionTest() : controller_invocations_allowed_(false) {}

  SyncSession* MakeSession() {
    std::vector<ModelSafeWorker*> workers;
    GetWorkers(&workers);
    return new SyncSession(context_.get(), this, SyncSourceInfo(),
                           routes_, workers);
  }

  virtual void SetUp() {
    ModelSafeRoutingInfo routing_info;
    std::vector<ModelSafeWorker*> workers;

    GetModelSafeRoutingInfo(&routing_info);
    GetWorkers(&workers);

    context_.reset(
        new SyncSessionContext(
            NULL, NULL, routing_info, workers, &extensions_activity_monitor_,
            std::vector<SyncEngineEventListener*>(), NULL, NULL));
    routes_.clear();
    routes_[syncable::BOOKMARKS] = GROUP_UI;
    routes_[syncable::AUTOFILL] = GROUP_DB;
    scoped_refptr<ModelSafeWorker> passive_worker(
        new FakeModelWorker(GROUP_PASSIVE));
    scoped_refptr<ModelSafeWorker> ui_worker(
        new FakeModelWorker(GROUP_UI));
    scoped_refptr<ModelSafeWorker> db_worker(
        new FakeModelWorker(GROUP_DB));
    workers_.clear();
    workers_.push_back(passive_worker);
    workers_.push_back(ui_worker);
    workers_.push_back(db_worker);
    session_.reset(MakeSession());
  }
  virtual void TearDown() {
    session_.reset();
    context_.reset();
  }

  virtual void OnSilencedUntil(const base::TimeTicks& silenced_until) OVERRIDE {
    FailControllerInvocationIfDisabled("OnSilencedUntil");
  }
  virtual bool IsSyncingCurrentlySilenced() OVERRIDE {
    FailControllerInvocationIfDisabled("IsSyncingCurrentlySilenced");
    return false;
  }
  virtual void OnReceivedLongPollIntervalUpdate(
      const base::TimeDelta& new_interval) OVERRIDE {
    FailControllerInvocationIfDisabled("OnReceivedLongPollIntervalUpdate");
  }
  virtual void OnReceivedShortPollIntervalUpdate(
      const base::TimeDelta& new_interval) OVERRIDE {
    FailControllerInvocationIfDisabled("OnReceivedShortPollIntervalUpdate");
  }
  virtual void OnReceivedSessionsCommitDelay(
      const base::TimeDelta& new_delay) OVERRIDE {
    FailControllerInvocationIfDisabled("OnReceivedSessionsCommitDelay");
  }
  virtual void OnShouldStopSyncingPermanently() OVERRIDE {
    FailControllerInvocationIfDisabled("OnShouldStopSyncingPermanently");
  }
  virtual void OnSyncProtocolError(
      const sessions::SyncSessionSnapshot& snapshot) {
    FailControllerInvocationIfDisabled("SyncProtocolError");
  }

  void GetWorkers(std::vector<ModelSafeWorker*>* out) const {
    out->clear();
    for (std::vector<scoped_refptr<ModelSafeWorker> >::const_iterator it =
             workers_.begin(); it != workers_.end(); ++it) {
      out->push_back(it->get());
    }
  }
  void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) const {
    *out = routes_;
  }

  StatusController* status() { return session_->mutable_status_controller(); }
 protected:
  void FailControllerInvocationIfDisabled(const std::string& msg) {
    if (!controller_invocations_allowed_)
      FAIL() << msg;
  }

  syncable::ModelTypeSet ParamsMeaningAllEnabledTypes() {
    syncable::ModelTypeSet request_params(
        syncable::BOOKMARKS, syncable::AUTOFILL);
    return request_params;
  }

  syncable::ModelTypeSet ParamsMeaningJustOneEnabledType() {
    return syncable::ModelTypeSet(syncable::AUTOFILL);
  }

  MessageLoop message_loop_;
  bool controller_invocations_allowed_;
  scoped_ptr<SyncSession> session_;
  scoped_ptr<SyncSessionContext> context_;
  std::vector<scoped_refptr<ModelSafeWorker> > workers_;
  ModelSafeRoutingInfo routes_;
  FakeExtensionsActivityMonitor extensions_activity_monitor_;
};

TEST_F(SyncSessionTest, EnabledGroupsEmpty) {
  routes_.clear();
  workers_.clear();
  scoped_ptr<SyncSession> session(MakeSession());
  std::set<ModelSafeGroup> expected_enabled_groups;
  expected_enabled_groups.insert(GROUP_PASSIVE);
  EXPECT_EQ(expected_enabled_groups, session->GetEnabledGroups());
}

TEST_F(SyncSessionTest, EnabledGroups) {
  scoped_ptr<SyncSession> session(MakeSession());
  std::set<ModelSafeGroup> expected_enabled_groups;
  expected_enabled_groups.insert(GROUP_PASSIVE);
  expected_enabled_groups.insert(GROUP_UI);
  expected_enabled_groups.insert(GROUP_DB);
  EXPECT_EQ(expected_enabled_groups, session->GetEnabledGroups());
}

TEST_F(SyncSessionTest, EnabledGroupsWithConflictsEmpty) {
  scoped_ptr<SyncSession> session(MakeSession());
  // Auto-create conflict progress.  This shouldn't put that group in
  // conflict.
  session->mutable_status_controller()->
      GetUnrestrictedMutableConflictProgressForTest(GROUP_PASSIVE);
  EXPECT_TRUE(session->GetEnabledGroupsWithConflicts().empty());
}

TEST_F(SyncSessionTest, EnabledGroupsWithConflicts) {
  scoped_ptr<SyncSession> session(MakeSession());
  // Put GROUP_UI in conflict.
  session->mutable_status_controller()->
      GetUnrestrictedMutableConflictProgressForTest(GROUP_UI)->
      AddSimpleConflictingItemById(syncable::Id());
  std::set<ModelSafeGroup> expected_enabled_groups_with_conflicts;
  expected_enabled_groups_with_conflicts.insert(GROUP_UI);
  EXPECT_EQ(expected_enabled_groups_with_conflicts,
            session->GetEnabledGroupsWithConflicts());
}

TEST_F(SyncSessionTest, ScopedContextHelpers) {
  ConflictResolver resolver;
  EXPECT_FALSE(context_->resolver());
  {
    ScopedSessionContextConflictResolver s_resolver(context_.get(), &resolver);
    EXPECT_EQ(&resolver, context_->resolver());
  }
  EXPECT_FALSE(context_->resolver());
}

TEST_F(SyncSessionTest, SetWriteTransaction) {
  TestDirectorySetterUpper dir_maker;
  dir_maker.SetUp();
  syncable::Directory* directory = dir_maker.directory();

  scoped_ptr<SyncSession> session(MakeSession());
  EXPECT_TRUE(NULL == session->write_transaction());
  {
    WriteTransaction trans(FROM_HERE, syncable::UNITTEST, directory);
    sessions::ScopedSetSessionWriteTransaction set_trans(session.get(), &trans);
    EXPECT_TRUE(&trans == session->write_transaction());
  }
}

TEST_F(SyncSessionTest, MoreToDownloadIfDownloadFailed) {
  status()->set_updates_request_types(ParamsMeaningAllEnabledTypes());

  status()->set_last_download_updates_result(NETWORK_IO_ERROR);

  // When DownloadUpdatesCommand fails, these should be false.
  EXPECT_FALSE(status()->ServerSaysNothingMoreToDownload());
  EXPECT_FALSE(status()->download_updates_succeeded());

  // Download updates has its own loop in the syncer; it shouldn't factor
  // into HasMoreToSync.
  EXPECT_FALSE(session_->HasMoreToSync());
}

TEST_F(SyncSessionTest, MoreToDownloadIfGotChangesRemaining) {
  status()->set_updates_request_types(ParamsMeaningAllEnabledTypes());

  // When the server returns changes_remaining, that means there's
  // more to download.
  status()->set_last_download_updates_result(SYNCER_OK);
  status()->mutable_updates_response()->mutable_get_updates()
     ->set_changes_remaining(1000L);
  EXPECT_FALSE(status()->ServerSaysNothingMoreToDownload());
  EXPECT_TRUE(status()->download_updates_succeeded());

  // Download updates has its own loop in the syncer; it shouldn't factor
  // into HasMoreToSync.
  EXPECT_FALSE(session_->HasMoreToSync());
}

TEST_F(SyncSessionTest, MoreToDownloadIfGotNoChangesRemaining) {
  status()->set_updates_request_types(ParamsMeaningAllEnabledTypes());

  status()->set_last_download_updates_result(SYNCER_OK);
  status()->mutable_updates_response()->mutable_get_updates()
      ->set_changes_remaining(0);
  EXPECT_TRUE(status()->ServerSaysNothingMoreToDownload());
  EXPECT_TRUE(status()->download_updates_succeeded());

  // Download updates has its own loop in the syncer; it shouldn't factor
  // into HasMoreToSync.
  EXPECT_FALSE(session_->HasMoreToSync());
}

TEST_F(SyncSessionTest, MoreToSyncIfConflictsResolved) {
  // Conflict resolution happens after get updates and commit,
  // so we need to loop back and get updates / commit again now
  // that we have made forward progress.
  status()->update_conflicts_resolved(true);
  EXPECT_TRUE(session_->HasMoreToSync());
}

TEST_F(SyncSessionTest, ResetTransientState) {
  status()->update_conflicts_resolved(true);
  status()->increment_num_successful_commits();
  EXPECT_TRUE(session_->HasMoreToSync());
  session_->PrepareForAnotherSyncCycle();
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION,
            session_->source().updates_source);
  EXPECT_FALSE(status()->conflicts_resolved());
  EXPECT_FALSE(session_->HasMoreToSync());
  EXPECT_FALSE(status()->TestAndClearIsDirty());
}

TEST_F(SyncSessionTest, Coalesce) {
  std::vector<ModelSafeWorker*> workers_one, workers_two;
  ModelSafeRoutingInfo routes_one, routes_two;
  syncable::ModelTypePayloadMap one_type =
      syncable::ModelTypePayloadMapFromEnumSet(
          ParamsMeaningJustOneEnabledType(),
          std::string());
  syncable::ModelTypePayloadMap all_types =
      syncable::ModelTypePayloadMapFromEnumSet(
          ParamsMeaningAllEnabledTypes(),
          std::string());
  SyncSourceInfo source_one(sync_pb::GetUpdatesCallerInfo::PERIODIC, one_type);
  SyncSourceInfo source_two(sync_pb::GetUpdatesCallerInfo::LOCAL, all_types);

  scoped_refptr<ModelSafeWorker> passive_worker(
      new FakeModelWorker(GROUP_PASSIVE));
  scoped_refptr<ModelSafeWorker> db_worker(new FakeModelWorker(GROUP_DB));
  scoped_refptr<ModelSafeWorker> ui_worker(new FakeModelWorker(GROUP_UI));
  workers_one.push_back(passive_worker);
  workers_one.push_back(db_worker);
  workers_two.push_back(passive_worker);
  workers_two.push_back(db_worker);
  workers_two.push_back(ui_worker);
  routes_one[syncable::AUTOFILL] = GROUP_DB;
  routes_two[syncable::AUTOFILL] = GROUP_DB;
  routes_two[syncable::BOOKMARKS] = GROUP_UI;
  SyncSession one(context_.get(), this, source_one, routes_one, workers_one);
  SyncSession two(context_.get(), this, source_two, routes_two, workers_two);

  std::set<ModelSafeGroup> expected_enabled_groups_one;
  expected_enabled_groups_one.insert(GROUP_PASSIVE);
  expected_enabled_groups_one.insert(GROUP_DB);

  std::set<ModelSafeGroup> expected_enabled_groups_two;
  expected_enabled_groups_two.insert(GROUP_PASSIVE);
  expected_enabled_groups_two.insert(GROUP_DB);
  expected_enabled_groups_two.insert(GROUP_UI);

  EXPECT_EQ(expected_enabled_groups_one, one.GetEnabledGroups());
  EXPECT_EQ(expected_enabled_groups_two, two.GetEnabledGroups());

  one.Coalesce(two);

  EXPECT_EQ(expected_enabled_groups_two, one.GetEnabledGroups());
  EXPECT_EQ(expected_enabled_groups_two, two.GetEnabledGroups());

  EXPECT_EQ(two.source().updates_source, one.source().updates_source);
  EXPECT_EQ(all_types, one.source().types);
  std::vector<ModelSafeWorker*>::const_iterator it_db =
      std::find(one.workers().begin(), one.workers().end(), db_worker);
  std::vector<ModelSafeWorker*>::const_iterator it_ui =
      std::find(one.workers().begin(), one.workers().end(), ui_worker);
  EXPECT_NE(it_db, one.workers().end());
  EXPECT_NE(it_ui, one.workers().end());
  EXPECT_EQ(routes_two, one.routing_info());
}

TEST_F(SyncSessionTest, RebaseRoutingInfoWithLatestRemoveOneType) {
  std::vector<ModelSafeWorker*> workers_one, workers_two;
  ModelSafeRoutingInfo routes_one, routes_two;
  syncable::ModelTypePayloadMap one_type =
      syncable::ModelTypePayloadMapFromEnumSet(
          ParamsMeaningJustOneEnabledType(),
          std::string());
  syncable::ModelTypePayloadMap all_types =
      syncable::ModelTypePayloadMapFromEnumSet(
          ParamsMeaningAllEnabledTypes(),
          std::string());
  SyncSourceInfo source_one(sync_pb::GetUpdatesCallerInfo::PERIODIC, one_type);
  SyncSourceInfo source_two(sync_pb::GetUpdatesCallerInfo::LOCAL, all_types);

  scoped_refptr<ModelSafeWorker> passive_worker(
      new FakeModelWorker(GROUP_PASSIVE));
  scoped_refptr<ModelSafeWorker> db_worker(new FakeModelWorker(GROUP_DB));
  scoped_refptr<ModelSafeWorker> ui_worker(new FakeModelWorker(GROUP_UI));
  workers_one.push_back(passive_worker);
  workers_one.push_back(db_worker);
  workers_two.push_back(passive_worker);
  workers_two.push_back(db_worker);
  workers_two.push_back(ui_worker);
  routes_one[syncable::AUTOFILL] = GROUP_DB;
  routes_two[syncable::AUTOFILL] = GROUP_DB;
  routes_two[syncable::BOOKMARKS] = GROUP_UI;
  SyncSession one(context_.get(), this, source_one, routes_one, workers_one);
  SyncSession two(context_.get(), this, source_two, routes_two, workers_two);

  std::set<ModelSafeGroup> expected_enabled_groups_one;
  expected_enabled_groups_one.insert(GROUP_PASSIVE);
  expected_enabled_groups_one.insert(GROUP_DB);

  std::set<ModelSafeGroup> expected_enabled_groups_two;
  expected_enabled_groups_two.insert(GROUP_PASSIVE);
  expected_enabled_groups_two.insert(GROUP_DB);
  expected_enabled_groups_two.insert(GROUP_UI);

  EXPECT_EQ(expected_enabled_groups_one, one.GetEnabledGroups());
  EXPECT_EQ(expected_enabled_groups_two, two.GetEnabledGroups());

  two.RebaseRoutingInfoWithLatest(one);

  EXPECT_EQ(expected_enabled_groups_one, one.GetEnabledGroups());
  EXPECT_EQ(expected_enabled_groups_one, two.GetEnabledGroups());

  // Make sure the source has not been touched.
  EXPECT_EQ(two.source().updates_source,
      sync_pb::GetUpdatesCallerInfo::LOCAL);

  // Make sure the payload is reduced to one.
  EXPECT_EQ(one_type, two.source().types);

  // Make sure the workers are udpated.
  std::vector<ModelSafeWorker*>::const_iterator it_db =
      std::find(two.workers().begin(), two.workers().end(), db_worker);
  std::vector<ModelSafeWorker*>::const_iterator it_ui =
      std::find(two.workers().begin(), two.workers().end(), ui_worker);
  EXPECT_NE(it_db, two.workers().end());
  EXPECT_EQ(it_ui, two.workers().end());
  EXPECT_EQ(two.workers().size(), 2U);

  // Make sure the model safe routing info is reduced to one type.
  ModelSafeRoutingInfo::const_iterator it =
      two.routing_info().find(syncable::AUTOFILL);
  // Note that attempting to use EXPECT_NE would fail for an Android build due
  // to seeming incompatibility with gtest and stlport.
  EXPECT_TRUE(it != two.routing_info().end());
  EXPECT_EQ(it->second, GROUP_DB);
  EXPECT_EQ(two.routing_info().size(), 1U);
}

TEST_F(SyncSessionTest, RebaseRoutingInfoWithLatestWithSameType) {
  std::vector<ModelSafeWorker*> workers_first, workers_second;
  ModelSafeRoutingInfo routes_first, routes_second;
  syncable::ModelTypePayloadMap all_types =
      syncable::ModelTypePayloadMapFromEnumSet(
          ParamsMeaningAllEnabledTypes(),
          std::string());
  SyncSourceInfo source_first(sync_pb::GetUpdatesCallerInfo::PERIODIC,
      all_types);
  SyncSourceInfo source_second(sync_pb::GetUpdatesCallerInfo::LOCAL,
      all_types);

  scoped_refptr<ModelSafeWorker> passive_worker(
      new FakeModelWorker(GROUP_PASSIVE));
  scoped_refptr<FakeModelWorker> db_worker(new FakeModelWorker(GROUP_DB));
  scoped_refptr<FakeModelWorker> ui_worker(new FakeModelWorker(GROUP_UI));
  workers_first.push_back(passive_worker);
  workers_first.push_back(db_worker);
  workers_first.push_back(ui_worker);
  workers_second.push_back(passive_worker);
  workers_second.push_back(db_worker);
  workers_second.push_back(ui_worker);
  routes_first[syncable::AUTOFILL] = GROUP_DB;
  routes_first[syncable::BOOKMARKS] = GROUP_UI;
  routes_second[syncable::AUTOFILL] = GROUP_DB;
  routes_second[syncable::BOOKMARKS] = GROUP_UI;
  SyncSession first(context_.get(), this, source_first, routes_first,
      workers_first);
  SyncSession second(context_.get(), this, source_second, routes_second,
      workers_second);

  std::set<ModelSafeGroup> expected_enabled_groups;
  expected_enabled_groups.insert(GROUP_PASSIVE);
  expected_enabled_groups.insert(GROUP_DB);
  expected_enabled_groups.insert(GROUP_UI);

  EXPECT_EQ(expected_enabled_groups, first.GetEnabledGroups());
  EXPECT_EQ(expected_enabled_groups, second.GetEnabledGroups());

  second.RebaseRoutingInfoWithLatest(first);

  EXPECT_EQ(expected_enabled_groups, first.GetEnabledGroups());
  EXPECT_EQ(expected_enabled_groups, second.GetEnabledGroups());

  // Make sure the source has not been touched.
  EXPECT_EQ(second.source().updates_source,
      sync_pb::GetUpdatesCallerInfo::LOCAL);

  // Make sure our payload is still the same.
  EXPECT_EQ(all_types, second.source().types);

  // Make sure the workers are still the same.
  std::vector<ModelSafeWorker*>::const_iterator it_passive =
      std::find(second.workers().begin(), second.workers().end(),
                passive_worker);
  std::vector<ModelSafeWorker*>::const_iterator it_db =
      std::find(second.workers().begin(), second.workers().end(), db_worker);
  std::vector<ModelSafeWorker*>::const_iterator it_ui =
      std::find(second.workers().begin(), second.workers().end(), ui_worker);
  EXPECT_NE(it_passive, second.workers().end());
  EXPECT_NE(it_db, second.workers().end());
  EXPECT_NE(it_ui, second.workers().end());
  EXPECT_EQ(second.workers().size(), 3U);

  // Make sure the model safe routing info is reduced to first type.
  ModelSafeRoutingInfo::const_iterator it1 =
      second.routing_info().find(syncable::AUTOFILL);
  ModelSafeRoutingInfo::const_iterator it2 =
      second.routing_info().find(syncable::BOOKMARKS);

  // Note that attempting to use EXPECT_NE would fail for an Android build due
  // to seeming incompatibility with gtest and stlport.
  EXPECT_TRUE(it1 != second.routing_info().end());
  EXPECT_EQ(it1->second, GROUP_DB);

  // Note that attempting to use EXPECT_NE would fail for an Android build due
  // to seeming incompatibility with gtest and stlport.
  EXPECT_TRUE(it2 != second.routing_info().end());
  EXPECT_EQ(it2->second, GROUP_UI);
  EXPECT_EQ(second.routing_info().size(), 2U);
}


TEST_F(SyncSessionTest, MakeTypePayloadMapFromBitSet) {
  syncable::ModelTypeSet types;
  std::string payload = "test";
  syncable::ModelTypePayloadMap types_with_payloads =
      syncable::ModelTypePayloadMapFromEnumSet(types, payload);
  EXPECT_TRUE(types_with_payloads.empty());

  types.Put(syncable::BOOKMARKS);
  types.Put(syncable::PASSWORDS);
  types.Put(syncable::AUTOFILL);
  payload = "test2";
  types_with_payloads =
      syncable::ModelTypePayloadMapFromEnumSet(types, payload);

  ASSERT_EQ(3U, types_with_payloads.size());
  EXPECT_EQ(types_with_payloads[syncable::BOOKMARKS], payload);
  EXPECT_EQ(types_with_payloads[syncable::PASSWORDS], payload);
  EXPECT_EQ(types_with_payloads[syncable::AUTOFILL], payload);
}

TEST_F(SyncSessionTest, MakeTypePayloadMapFromRoutingInfo) {
  std::string payload = "test";
  syncable::ModelTypePayloadMap types_with_payloads
      = syncable::ModelTypePayloadMapFromRoutingInfo(routes_, payload);
  ASSERT_EQ(routes_.size(), types_with_payloads.size());
  for (ModelSafeRoutingInfo::iterator iter = routes_.begin();
       iter != routes_.end();
       ++iter) {
    EXPECT_EQ(payload, types_with_payloads[iter->first]);
  }
}

TEST_F(SyncSessionTest, CoalescePayloads) {
  syncable::ModelTypePayloadMap original;
  std::string empty_payload;
  std::string payload1 = "payload1";
  std::string payload2 = "payload2";
  std::string payload3 = "payload3";
  original[syncable::BOOKMARKS] = empty_payload;
  original[syncable::PASSWORDS] = payload1;
  original[syncable::AUTOFILL] = payload2;
  original[syncable::THEMES] = payload3;

  syncable::ModelTypePayloadMap update;
  update[syncable::BOOKMARKS] = empty_payload;  // Same.
  update[syncable::PASSWORDS] = empty_payload;  // Overwrite with empty.
  update[syncable::AUTOFILL] = payload1;        // Overwrite with non-empty.
  update[syncable::SESSIONS] = payload2;        // New.
  // Themes untouched.

  CoalescePayloads(&original, update);
  ASSERT_EQ(5U, original.size());
  EXPECT_EQ(empty_payload, original[syncable::BOOKMARKS]);
  EXPECT_EQ(payload1, original[syncable::PASSWORDS]);
  EXPECT_EQ(payload1, original[syncable::AUTOFILL]);
  EXPECT_EQ(payload2, original[syncable::SESSIONS]);
  EXPECT_EQ(payload3, original[syncable::THEMES]);
}

}  // namespace
}  // namespace sessions
}  // namespace browser_sync
