// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/test_web_app_database.h"

#include "chrome/browser/web_applications/web_app.h"

namespace web_app {

TestWebAppDatabase::TestWebAppDatabase() {}

TestWebAppDatabase::~TestWebAppDatabase() {}

void TestWebAppDatabase::OpenDatabase(OnceRegistryOpenedCallback callback) {
  open_database_callback_ = std::move(callback);
}

void TestWebAppDatabase::WriteWebApps(AppsToWrite apps,
                                      CompletionCallback callback) {
  for (auto* app : apps)
    write_web_app_ids_.push_back(app->app_id());

  std::move(callback).Run(next_write_web_apps_result_);
}

void TestWebAppDatabase::DeleteWebApps(std::vector<AppId> app_ids,
                                       CompletionCallback callback) {
  delete_web_app_ids_ = std::move(app_ids);

  std::move(callback).Run(next_delete_web_apps_result_);
}

void TestWebAppDatabase::SetNextWriteWebAppsResult(
    bool next_write_web_apps_result) {
  next_write_web_apps_result_ = next_write_web_apps_result;
}

void TestWebAppDatabase::SetNextDeleteWebAppsResult(
    bool next_delete_web_apps_result) {
  next_delete_web_apps_result_ = next_delete_web_apps_result;
}

}  // namespace web_app
