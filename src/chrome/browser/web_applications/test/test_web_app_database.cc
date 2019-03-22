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

void TestWebAppDatabase::WriteWebApp(const WebApp& web_app) {
  write_web_app_id_ = web_app.app_id();
}

void TestWebAppDatabase::DeleteWebApps(std::vector<AppId> app_ids) {
  delete_web_app_ids_ = std::move(app_ids);
}

}  // namespace web_app
