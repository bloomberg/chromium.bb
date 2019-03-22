// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_DATABASE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_DATABASE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/web_applications/abstract_web_app_database.h"

namespace web_app {

class TestWebAppDatabase : public AbstractWebAppDatabase {
 public:
  TestWebAppDatabase();
  ~TestWebAppDatabase() override;

  // AbstractWebAppDatabase:
  void OpenDatabase(OnceRegistryOpenedCallback callback) override;
  void WriteWebApp(const WebApp& web_app) override;
  void DeleteWebApps(std::vector<AppId> app_ids) override;

  OnceRegistryOpenedCallback TakeOpenDatabaseCallback() {
    return std::move(open_database_callback_);
  }
  const AppId& write_web_app_id() const { return write_web_app_id_; }
  const std::vector<AppId>& delete_web_app_ids() const {
    return delete_web_app_ids_;
  }

 private:
  OnceRegistryOpenedCallback open_database_callback_;
  AppId write_web_app_id_;
  std::vector<AppId> delete_web_app_ids_;

  DISALLOW_COPY_AND_ASSIGN(TestWebAppDatabase);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_DATABASE_H_
