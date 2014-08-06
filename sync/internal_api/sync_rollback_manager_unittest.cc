// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_rollback_manager.h"

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"
#include "sync/internal_api/public/test/test_internal_components_factory.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/internal_api/sync_backup_manager.h"
#include "sync/syncable/entry.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/test_directory_backing_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoDefault;
using ::testing::Invoke;
using ::testing::Truly;
using ::testing::WithArgs;

namespace syncer {

namespace {

class TestChangeDelegate : public SyncManager::ChangeDelegate {
 public:
  TestChangeDelegate() {
    ON_CALL(*this, OnChangesApplied(_, _, _, _))
        .WillByDefault(
            WithArgs<3>(Invoke(this,
                               &TestChangeDelegate::VerifyDeletes)));
  }

  void add_expected_delete(int64 v) {
    expected_deletes_.insert(v);
  }

  MOCK_METHOD4(OnChangesApplied,
               void(ModelType model_type,
                    int64 model_version,
                    const BaseTransaction* trans,
                    const ImmutableChangeRecordList& changes));
  MOCK_METHOD1(OnChangesComplete, void(ModelType model_type));

 private:
  void VerifyDeletes(const ImmutableChangeRecordList& changes) {
    std::set<int64> deleted;
    for (size_t i = 0; i < changes.Get().size(); ++i) {
      const ChangeRecord& change = (changes.Get())[i];
      EXPECT_EQ(ChangeRecord::ACTION_DELETE, change.action);
      EXPECT_TRUE(deleted.find(change.id) == deleted.end());
      deleted.insert(change.id);
    }
    EXPECT_TRUE(expected_deletes_ == deleted);
  }

  std::set<int64> expected_deletes_;
};

class SyncRollbackManagerTest : public testing::Test,
                                public SyncManager::Observer {
 protected:
  virtual void SetUp() OVERRIDE {
    CHECK(temp_dir_.CreateUniqueTempDir());

    worker_ = new FakeModelWorker(GROUP_UI);
  }

  MOCK_METHOD1(OnSyncCycleCompleted,
               void(const sessions::SyncSessionSnapshot&));
  MOCK_METHOD1(OnConnectionStatusChange, void(ConnectionStatus));
  MOCK_METHOD4(OnInitializationComplete,
               void(const WeakHandle<JsBackend>&,
                    const WeakHandle<DataTypeDebugInfoListener>&,
                    bool, ModelTypeSet));
  MOCK_METHOD1(OnActionableError, void(const SyncProtocolError&));
  MOCK_METHOD1(OnMigrationRequested, void(ModelTypeSet));;
  MOCK_METHOD1(OnProtocolEvent, void(const ProtocolEvent&));

  void OnConfigDone(bool success) {
    EXPECT_TRUE(success);
  }

  int64 CreateEntry(UserShare* user_share, ModelType type,
                    const std::string& client_tag) {
    WriteTransaction trans(FROM_HERE, user_share);
    ReadNode type_root(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, type_root.InitTypeRoot(type));

    WriteNode node(&trans);
    EXPECT_EQ(WriteNode::INIT_SUCCESS,
              node.InitUniqueByCreation(type, type_root, client_tag));
    return node.GetEntry()->GetMetahandle();
  }

  void InitManager(SyncManager* manager, ModelTypeSet types,
                   TestChangeDelegate* delegate, StorageOption storage_option) {
    manager_ = manager;
    types_ = types;

    EXPECT_CALL(*this, OnInitializationComplete(_, _, _, _))
        .WillOnce(WithArgs<2>(Invoke(this,
                                     &SyncRollbackManagerTest::HandleInit)));

    manager->AddObserver(this);
    TestInternalComponentsFactory factory(InternalComponentsFactory::Switches(),
                                          storage_option);

    base::RunLoop run_loop;
    manager->Init(temp_dir_.path(),
                  MakeWeakHandle(base::WeakPtr<JsEventHandler>()),
                  "", 0, true, scoped_ptr<HttpPostProviderFactory>().Pass(),
                  std::vector<scoped_refptr<ModelSafeWorker> >(1,
                                                               worker_.get()),
                  NULL, delegate, SyncCredentials(), "", "", "", &factory,
                  NULL, scoped_ptr<UnrecoverableErrorHandler>().Pass(),
                  NULL, NULL);
    loop_.PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Create and persist an entry by unique tag in DB.
  void PrepopulateDb(ModelType type, const std::string& client_tag) {
    SyncBackupManager backup_manager;
    TestChangeDelegate delegate;
    InitManager(&backup_manager, ModelTypeSet(type), &delegate,
                STORAGE_ON_DISK);
    CreateEntry(backup_manager.GetUserShare(), type, client_tag);
    backup_manager.ShutdownOnSyncThread(STOP_SYNC);
  }

  // Verify entry with |client_tag| exists in sync directory.
  bool VerifyEntry(UserShare* user_share, ModelType type,
                   const std::string& client_tag) {
    ReadTransaction trans(FROM_HERE, user_share);
    ReadNode node(&trans);
    return BaseNode::INIT_OK == node.InitByClientTagLookup(type, client_tag);
  }

 private:
  void ConfigureSyncer() {
    manager_->ConfigureSyncer(
          CONFIGURE_REASON_NEW_CLIENT,
          types_,
          ModelTypeSet(), ModelTypeSet(), ModelTypeSet(),
          ModelSafeRoutingInfo(),
          base::Bind(&SyncRollbackManagerTest::OnConfigDone,
                     base::Unretained(this), true),
          base::Bind(&SyncRollbackManagerTest::OnConfigDone,
                     base::Unretained(this), false));
  }

  void HandleInit(bool success) {
    if (success) {
      loop_.PostTask(FROM_HERE,
                     base::Bind(&SyncRollbackManagerTest::ConfigureSyncer,
                                base::Unretained(this)));
    } else {
      manager_->ShutdownOnSyncThread(STOP_SYNC);
    }
  }

  base::ScopedTempDir temp_dir_;
  scoped_refptr<ModelSafeWorker> worker_;
  base::MessageLoop loop_;    // Needed for WeakHandle
  SyncManager* manager_;
  ModelTypeSet types_;
};

bool IsRollbackDoneAction(SyncProtocolError e) {
  return e.action == syncer::ROLLBACK_DONE;
}

TEST_F(SyncRollbackManagerTest, RollbackBasic) {
  PrepopulateDb(PREFERENCES, "pref1");

  TestChangeDelegate delegate;
  SyncRollbackManager rollback_manager;
  InitManager(&rollback_manager, ModelTypeSet(PREFERENCES), &delegate,
              STORAGE_ON_DISK);

  // Simulate a new entry added during type initialization.
  int64 new_pref_id =
      CreateEntry(rollback_manager.GetUserShare(), PREFERENCES, "pref2");

  delegate.add_expected_delete(new_pref_id);
  EXPECT_CALL(delegate, OnChangesApplied(_, _, _, _))
      .Times(1)
      .WillOnce(DoDefault());
  EXPECT_CALL(delegate, OnChangesComplete(_)).Times(1);
  EXPECT_CALL(*this, OnActionableError(Truly(IsRollbackDoneAction))).Times(1);

  ModelSafeRoutingInfo routing_info;
  routing_info[PREFERENCES] = GROUP_UI;
  rollback_manager.StartSyncingNormally(routing_info);
}

TEST_F(SyncRollbackManagerTest, NoRollbackOfTypesNotBackedUp) {
  PrepopulateDb(PREFERENCES, "pref1");

  TestChangeDelegate delegate;
  SyncRollbackManager rollback_manager;
  InitManager(&rollback_manager, ModelTypeSet(PREFERENCES, APPS), &delegate,
              STORAGE_ON_DISK);

  // Simulate new entry added during type initialization.
  int64 new_pref_id =
      CreateEntry(rollback_manager.GetUserShare(), PREFERENCES, "pref2");
  CreateEntry(rollback_manager.GetUserShare(), APPS, "app1");

  delegate.add_expected_delete(new_pref_id);
  EXPECT_CALL(delegate, OnChangesApplied(_, _, _, _))
      .Times(1)
      .WillOnce(DoDefault());
  EXPECT_CALL(delegate, OnChangesComplete(_)).Times(1);

  ModelSafeRoutingInfo routing_info;
  routing_info[PREFERENCES] = GROUP_UI;
  rollback_manager.StartSyncingNormally(routing_info);

  // APP entry is still valid.
  EXPECT_TRUE(VerifyEntry(rollback_manager.GetUserShare(), APPS, "app1"));
}

TEST_F(SyncRollbackManagerTest, BackupDbNotChangedOnAbort) {
  PrepopulateDb(PREFERENCES, "pref1");

  TestChangeDelegate delegate;
  scoped_ptr<SyncRollbackManager> rollback_manager(
      new SyncRollbackManager);
  InitManager(rollback_manager.get(), ModelTypeSet(PREFERENCES), &delegate,
              STORAGE_ON_DISK);

  // Simulate a new entry added during type initialization.
  CreateEntry(rollback_manager->GetUserShare(), PREFERENCES, "pref2");

  // Manager was shut down before sync starts.
  rollback_manager->ShutdownOnSyncThread(STOP_SYNC);

  // Verify new entry was not persisted.
  rollback_manager.reset(new SyncRollbackManager);
  InitManager(rollback_manager.get(), ModelTypeSet(PREFERENCES), &delegate,
              STORAGE_ON_DISK);
  EXPECT_FALSE(VerifyEntry(rollback_manager->GetUserShare(), PREFERENCES,
                           "pref2"));
}

TEST_F(SyncRollbackManagerTest, OnInitializationFailure) {
  // Test graceful shutdown on initialization failure.
  scoped_ptr<SyncRollbackManager> rollback_manager(
      new SyncRollbackManager);
  InitManager(rollback_manager.get(), ModelTypeSet(PREFERENCES), NULL,
              STORAGE_ON_DISK);
}

}  // anonymous namespace

}  // namespace syncer
