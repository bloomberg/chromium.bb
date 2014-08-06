// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_backup_manager.h"

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"
#include "sync/internal_api/public/test/test_internal_components_factory.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/syncable/entry.h"
#include "sync/test/test_directory_backing_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArgs;

namespace syncer {

namespace {

void OnConfigDone(bool success) {
  EXPECT_TRUE(success);
}

class SyncBackupManagerTest : public syncer::SyncManager::Observer,
                              public testing::Test {
 public:
  MOCK_METHOD1(OnSyncCycleCompleted,
               void(const sessions::SyncSessionSnapshot&));
  MOCK_METHOD1(OnConnectionStatusChange, void(ConnectionStatus));
  MOCK_METHOD1(OnActionableError, void(const SyncProtocolError&));
  MOCK_METHOD1(OnMigrationRequested, void(ModelTypeSet));;
  MOCK_METHOD1(OnProtocolEvent, void(const ProtocolEvent&));
  MOCK_METHOD4(OnInitializationComplete,
               void(const WeakHandle<JsBackend>&,
                    const WeakHandle<DataTypeDebugInfoListener>&,
                    bool, ModelTypeSet));

 protected:
  virtual void SetUp() OVERRIDE {
    CHECK(temp_dir_.CreateUniqueTempDir());
  }

  void InitManager(SyncManager* manager, StorageOption storage_option) {
    manager_ = manager;
    EXPECT_CALL(*this, OnInitializationComplete(_, _, _, _))
        .WillOnce(WithArgs<2>(Invoke(this,
                                     &SyncBackupManagerTest::HandleInit)));

    TestInternalComponentsFactory factory(InternalComponentsFactory::Switches(),
                                          storage_option);
    manager->AddObserver(this);

    base::RunLoop run_loop;
    manager->Init(temp_dir_.path(),
                  MakeWeakHandle(base::WeakPtr<JsEventHandler>()),
                  GURL("https://example.com/"),
                  scoped_ptr<HttpPostProviderFactory>().Pass(),
                  std::vector<scoped_refptr<ModelSafeWorker> >(),
                  NULL,
                  NULL,
                  SyncCredentials(),
                  "",
                  "",
                  "",
                  &factory,
                  NULL,
                  scoped_ptr<UnrecoverableErrorHandler>().Pass(),
                  NULL,
                  NULL);
    loop_.PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  void CreateEntry(UserShare* user_share, ModelType type,
                   const std::string& client_tag) {
    WriteTransaction trans(FROM_HERE, user_share);
    ReadNode type_root(&trans);
    EXPECT_EQ(BaseNode::INIT_OK, type_root.InitTypeRoot(type));

    WriteNode node(&trans);
    EXPECT_EQ(WriteNode::INIT_SUCCESS,
              node.InitUniqueByCreation(type, type_root, client_tag));
  }

 private:
  void ConfigureSyncer() {
    manager_->ConfigureSyncer(CONFIGURE_REASON_NEW_CLIENT,
                              ModelTypeSet(SEARCH_ENGINES),
                              ModelTypeSet(), ModelTypeSet(), ModelTypeSet(),
                              ModelSafeRoutingInfo(),
                              base::Bind(&OnConfigDone, true),
                              base::Bind(&OnConfigDone, false));
  }

  void HandleInit(bool success) {
    if (success) {
      loop_.PostTask(FROM_HERE,
                     base::Bind(&SyncBackupManagerTest::ConfigureSyncer,
                                base::Unretained(this)));
    } else {
      manager_->ShutdownOnSyncThread(STOP_SYNC);
    }
  }

  base::ScopedTempDir temp_dir_;
  base::MessageLoop loop_;    // Needed for WeakHandle
  SyncManager* manager_;
};

TEST_F(SyncBackupManagerTest, NormalizeAndPersist) {
  scoped_ptr<SyncBackupManager> manager(new SyncBackupManager);
  InitManager(manager.get(), STORAGE_ON_DISK);

  CreateEntry(manager->GetUserShare(), SEARCH_ENGINES, "test");

  {
    // New entry is local and unsynced at first.
    ReadTransaction trans(FROM_HERE, manager->GetUserShare());
    ReadNode pref(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              pref.InitByClientTagLookup(SEARCH_ENGINES, "test"));
    EXPECT_FALSE(pref.GetEntry()->GetId().ServerKnows());
    EXPECT_TRUE(pref.GetEntry()->GetIsUnsynced());
  }

  manager->SaveChanges();

  {
    // New entry has server ID and unsynced bit is cleared after saving.
    ReadTransaction trans(FROM_HERE, manager->GetUserShare());
    ReadNode pref(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              pref.InitByClientTagLookup(SEARCH_ENGINES, "test"));
    EXPECT_TRUE(pref.GetEntry()->GetId().ServerKnows());
    EXPECT_FALSE(pref.GetEntry()->GetIsUnsynced());
  }
  manager->ShutdownOnSyncThread(STOP_SYNC);

  // Reopen db to verify entry is persisted.
  manager.reset(new SyncBackupManager);
  InitManager(manager.get(), STORAGE_ON_DISK);
  {
    ReadTransaction trans(FROM_HERE, manager->GetUserShare());
    ReadNode pref(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              pref.InitByClientTagLookup(SEARCH_ENGINES, "test"));
    EXPECT_TRUE(pref.GetEntry()->GetId().ServerKnows());
    EXPECT_FALSE(pref.GetEntry()->GetIsUnsynced());
  }
}

TEST_F(SyncBackupManagerTest, FailToInitialize) {
  // Test graceful shutdown on initialization failure.
  scoped_ptr<SyncBackupManager> manager(new SyncBackupManager);
  InitManager(manager.get(), STORAGE_INVALID);
}

}  // anonymous namespace

}  // namespace syncer
