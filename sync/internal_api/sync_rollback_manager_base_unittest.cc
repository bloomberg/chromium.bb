// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_rollback_manager_base.h"

#include "base/bind.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/test/test_internal_components_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

void OnConfigDone(bool success) {
  EXPECT_TRUE(success);
}

class SyncTestRollbackManager : public SyncRollbackManagerBase {
 public:
  virtual void Init(
      const base::FilePath& database_location,
      const WeakHandle<JsEventHandler>& event_handler,
      const std::string& sync_server_and_path,
      int sync_server_port,
      bool use_ssl,
      scoped_ptr<HttpPostProviderFactory> post_factory,
      const std::vector<scoped_refptr<ModelSafeWorker> >& workers,
      ExtensionsActivity* extensions_activity,
      ChangeDelegate* change_delegate,
      const SyncCredentials& credentials,
      const std::string& invalidator_client_id,
      const std::string& restored_key_for_bootstrapping,
      const std::string& restored_keystore_key_for_bootstrapping,
      InternalComponentsFactory* internal_components_factory,
      Encryptor* encryptor,
      scoped_ptr<UnrecoverableErrorHandler> unrecoverable_error_handler,
      ReportUnrecoverableErrorFunction report_unrecoverable_error_function,
      CancelationSignal* cancelation_signal) OVERRIDE {
    SyncRollbackManagerBase::InitInternal(database_location,
                                          internal_components_factory,
                                          unrecoverable_error_handler.Pass(),
                                          report_unrecoverable_error_function);
  }
};

class SyncRollbackManagerBaseTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    TestInternalComponentsFactory factory(InternalComponentsFactory::Switches(),
                                          STORAGE_IN_MEMORY);
    manager_.Init(base::FilePath(base::FilePath::kCurrentDirectory),
                  MakeWeakHandle(base::WeakPtr<JsEventHandler>()),
                  "", 0, true, scoped_ptr<HttpPostProviderFactory>().Pass(),
                  std::vector<scoped_refptr<ModelSafeWorker> >(),
                  NULL, NULL, SyncCredentials(), "", "", "", &factory,
                  NULL, scoped_ptr<UnrecoverableErrorHandler>().Pass(),
                  NULL, NULL);
  }

  SyncTestRollbackManager manager_;
  base::MessageLoop loop_;    // Needed for WeakHandle
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
