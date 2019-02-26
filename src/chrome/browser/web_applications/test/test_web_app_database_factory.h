// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_DATABASE_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_DATABASE_FACTORY_H_

#include "base/macros.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"

namespace syncer {
class ModelTypeStore;
}  // namespace syncer

namespace web_app {

// Requires base::MessageLoop message_loop_ in test fixture. Reason:
// InMemoryStore needs a SequencedTaskRunner.
// MessageLoop ctor calls MessageLoop::SetThreadTaskRunnerHandle().
class TestWebAppDatabaseFactory : public AbstractWebAppDatabaseFactory {
 public:
  TestWebAppDatabaseFactory();
  ~TestWebAppDatabaseFactory() override;

  // AbstractWebAppDatabaseFactory interface implementation.
  syncer::OnceModelTypeStoreFactory GetStoreFactory() override;

  syncer::ModelTypeStore* store() { return store_.get(); }

 private:
  std::unique_ptr<syncer::ModelTypeStore> store_;

  DISALLOW_COPY_AND_ASSIGN(TestWebAppDatabaseFactory);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_DATABASE_FACTORY_H_
