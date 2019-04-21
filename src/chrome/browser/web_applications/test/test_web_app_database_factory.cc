// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"

#include "components/sync/model/model_type_store_test_util.h"

namespace web_app {

TestWebAppDatabaseFactory::TestWebAppDatabaseFactory() {
  // InMemoryStore must be created after message_loop_.
  store_ = syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest();
}

TestWebAppDatabaseFactory::~TestWebAppDatabaseFactory() {}

syncer::OnceModelTypeStoreFactory TestWebAppDatabaseFactory::GetStoreFactory() {
  return syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(
      store_.get());
}

}  // namespace web_app
