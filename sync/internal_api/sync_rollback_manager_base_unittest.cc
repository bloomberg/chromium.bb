// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_rollback_manager_base.h"

#include "base/bind.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/test/test_internal_components_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace syncer {

namespace {

void OnConfigDone(bool success) {
  EXPECT_TRUE(success);
}

class SyncTestRollbackManager : public SyncRollbackManagerBase {
 public:
  virtual void Init(InitArgs* args) OVERRIDE {
    SyncRollbackManagerBase::InitInternal(
        args->database_location,
        args->internal_components_factory.get(),
        InternalComponentsFactory::STORAGE_IN_MEMORY,
        args->unrecoverable_error_handler.Pass(),
        args->report_unrecoverable_error_function);
  }
};

class SyncRollbackManagerBaseTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    SyncManager::InitArgs args;
    args.database_location = base::FilePath(base::FilePath::kCurrentDirectory);
    args.service_url = GURL("https://example.com/");
    args.internal_components_factory.reset(new TestInternalComponentsFactory(
        InternalComponentsFactory::Switches(),
        InternalComponentsFactory::STORAGE_IN_MEMORY,
        &storage_used_));
    manager_.Init(&args);
    EXPECT_EQ(InternalComponentsFactory::STORAGE_IN_MEMORY, storage_used_);
  }

  SyncTestRollbackManager manager_;
  base::MessageLoop loop_;    // Needed for WeakHandle
  InternalComponentsFactory::StorageOption storage_used_;
};

TEST_F(SyncRollbackManagerBaseTest, InitTypeOnConfiguration) {
  EXPECT_TRUE(manager_.InitialSyncEndedTypes().Empty());

  manager_.ConfigureSyncer(
      CONFIGURE_REASON_NEW_CLIENT,
      ModelTypeSet(PREFERENCES, BOOKMARKS),
      ModelTypeSet(), ModelTypeSet(), ModelTypeSet(), ModelSafeRoutingInfo(),
      base::Bind(&OnConfigDone, true),
      base::Bind(&OnConfigDone, false));

  ReadTransaction trans(FROM_HERE, manager_.GetUserShare());
  ReadNode pref_root(&trans);
  EXPECT_EQ(BaseNode::INIT_OK,
            pref_root.InitTypeRoot(PREFERENCES));

  ReadNode bookmark_root(&trans);
  EXPECT_EQ(BaseNode::INIT_OK,
            bookmark_root.InitTypeRoot(BOOKMARKS));
  ReadNode bookmark_bar(&trans);
  EXPECT_EQ(BaseNode::INIT_OK,
            bookmark_bar.InitByTagLookupForBookmarks("bookmark_bar"));
  ReadNode bookmark_mobile(&trans);
  EXPECT_EQ(BaseNode::INIT_FAILED_ENTRY_NOT_GOOD,
            bookmark_mobile.InitByTagLookupForBookmarks("synced_bookmarks"));
  ReadNode bookmark_other(&trans);
  EXPECT_EQ(BaseNode::INIT_OK,
            bookmark_other.InitByTagLookupForBookmarks("other_bookmarks"));
}

}  // anonymous namespace

}  // namespace syncer
