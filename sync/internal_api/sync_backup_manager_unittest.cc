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

  void InitManager(SyncManager* manager,
                   InternalComponentsFactory::StorageOption storage_option) {
    manager_ = manager;
    EXPECT_CALL(*this, OnInitializationComplete(_, _, _, _))
        .WillOnce(WithArgs<2>(Invoke(this,
                                     &SyncBackupManagerTest::HandleInit)));

    manager->AddObserver(this);

    base::RunLoop run_loop;

    SyncManager::InitArgs args;
    args.database_location = temp_dir_.path();
    args.event_handler = MakeWeakHandle(base::WeakPtr<JsEventHandler>());
    args.service_url = GURL("https://example.com/");
    args.post_factory = scoped_ptr<HttpPostProviderFactory>().Pass();
    args.internal_components_factory.reset(new TestInternalComponentsFactory(
        InternalComponentsFactory::Switches(), storage_option,
        &storage_used_));
    manager->Init(&args);
    EXPECT_EQ(InternalComponentsFactory::STORAGE_ON_DISK_DEFERRED,
              storage_used_);
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
  InternalComponentsFactory::StorageOption storage_used_;
};

TEST_F(SyncBackupManagerTest, NormalizeEntry) {
  scoped_ptr<SyncBackupManager> manager(new SyncBackupManager);
  InitManager(manager.get(), InternalComponentsFactory::STORAGE_IN_MEMORY);

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
}

TEST_F(SyncBackupManagerTest, PersistWithSwitchToSyncShutdown) {
  scoped_ptr<SyncBackupManager> manager(new SyncBackupManager);
  InitManager(manager.get(),
              InternalComponentsFactory::STORAGE_ON_DISK_DEFERRED);

  CreateEntry(manager->GetUserShare(), SEARCH_ENGINES, "test");
  manager->SaveChanges();
  manager->ShutdownOnSyncThread(SWITCH_MODE_SYNC);

  // Reopen db to verify entry is persisted.
  manager.reset(new SyncBackupManager);
  InitManager(manager.get(), InternalComponentsFactory::STORAGE_ON_DISK);
  {
    ReadTransaction trans(FROM_HERE, manager->GetUserShare());
    ReadNode pref(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              pref.InitByClientTagLookup(SEARCH_ENGINES, "test"));
    EXPECT_TRUE(pref.GetEntry()->GetId().ServerKnows());
    EXPECT_FALSE(pref.GetEntry()->GetIsUnsynced());
  }
}

TEST_F(SyncBackupManagerTest, DontPersistWithOtherShutdown) {
  scoped_ptr<SyncBackupManager> manager(new SyncBackupManager);
  InitManager(manager.get(),
              InternalComponentsFactory::STORAGE_ON_DISK_DEFERRED);

  CreateEntry(manager->GetUserShare(), SEARCH_ENGINES, "test");
  manager->SaveChanges();
  manager->ShutdownOnSyncThread(STOP_SYNC);
  EXPECT_FALSE(base::PathExists(
      temp_dir_.path().Append(syncable::Directory::kSyncDatabaseFilename)));
}

TEST_F(SyncBackupManagerTest, FailToInitialize) {
  // Test graceful shutdown on initialization failure.
  scoped_ptr<SyncBackupManager> manager(new SyncBackupManager);
  InitManager(manager.get(), InternalComponentsFactory::STORAGE_INVALID);
}

}  // anonymous namespace

}  // namespace syncer
