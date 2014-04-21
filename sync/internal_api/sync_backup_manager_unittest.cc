// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_backup_manager.h"

#include "base/files/scoped_temp_dir.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/test/test_internal_components_factory.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/syncable/entry.h"
#include "sync/test/test_directory_backing_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

void OnConfigDone(bool success) {
  EXPECT_TRUE(success);
}

class SyncBackupManagerTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    CHECK(temp_dir_.CreateUniqueTempDir());
  }

  void InitManager(SyncManager* manager) {
    TestInternalComponentsFactory factory(InternalComponentsFactory::Switches(),
                                          STORAGE_ON_DISK);

    manager->Init(temp_dir_.path(),
                  MakeWeakHandle(base::WeakPtr<JsEventHandler>()),
                  "", 0, true, scoped_ptr<HttpPostProviderFactory>().Pass(),
                  std::vector<scoped_refptr<ModelSafeWorker> >(),
                  NULL, NULL, SyncCredentials(), "", "", "", &factory,
                  NULL, scoped_ptr<UnrecoverableErrorHandler>().Pass(),
                  NULL, NULL);
    manager->ConfigureSyncer(
          CONFIGURE_REASON_NEW_CLIENT,
          ModelTypeSet(PREFERENCES),
          ModelTypeSet(), ModelTypeSet(), ModelTypeSet(),
          ModelSafeRoutingInfo(),
          base::Bind(&OnConfigDone, true),
          base::Bind(&OnConfigDone, false));
  }

  void CreateEntry(UserShare* user_share, ModelType type,
                   const std::string& client_tag) {
    WriteTransaction trans(FROM_HERE, user_share);
    ReadNode type_root(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              type_root.InitByTagLookup(ModelTypeToRootTag(type)));

    WriteNode node(&trans);
    EXPECT_EQ(WriteNode::INIT_SUCCESS,
              node.InitUniqueByCreation(type, type_root, client_tag));
  }

  base::ScopedTempDir temp_dir_;
  base::MessageLoop loop_;    // Needed for WeakHandle
};

TEST_F(SyncBackupManagerTest, NormalizeAndPersist) {
  scoped_ptr<SyncBackupManager> manager(new SyncBackupManager);
  InitManager(manager.get());

  CreateEntry(manager->GetUserShare(), PREFERENCES, "test");

  {
    // New entry is local and unsynced at first.
    ReadTransaction trans(FROM_HERE, manager->GetUserShare());
    ReadNode pref(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              pref.InitByClientTagLookup(PREFERENCES, "test"));
    EXPECT_FALSE(pref.GetEntry()->GetId().ServerKnows());
    EXPECT_TRUE(pref.GetEntry()->GetIsUnsynced());
  }

  manager->SaveChanges();

  {
    // New entry has server ID and unsynced bit is cleared after saving.
    ReadTransaction trans(FROM_HERE, manager->GetUserShare());
    ReadNode pref(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              pref.InitByClientTagLookup(PREFERENCES, "test"));
    EXPECT_TRUE(pref.GetEntry()->GetId().ServerKnows());
    EXPECT_FALSE(pref.GetEntry()->GetIsUnsynced());
  }
  manager->ShutdownOnSyncThread();

  // Reopen db to verify entry is persisted.
  manager.reset(new SyncBackupManager);
  InitManager(manager.get());
  {
    ReadTransaction trans(FROM_HERE, manager->GetUserShare());
    ReadNode pref(&trans);
    EXPECT_EQ(BaseNode::INIT_OK,
              pref.InitByClientTagLookup(PREFERENCES, "test"));
    EXPECT_TRUE(pref.GetEntry()->GetId().ServerKnows());
    EXPECT_FALSE(pref.GetEntry()->GetIsUnsynced());
  }
}

}  // anonymous namespace

}  // namespace syncer

