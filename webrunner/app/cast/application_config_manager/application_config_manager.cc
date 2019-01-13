// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/app/cast/application_config_manager/application_config_manager.h"

#include "base/logging.h"

namespace castrunner {

ApplicationConfigManager::ApplicationConfigManager() = default;
ApplicationConfigManager::~ApplicationConfigManager() = default;

void ApplicationConfigManager::GetConfig(std::string id,
                                         GetConfigCallback callback) {
  constexpr char kTestCastAppId[] = "00000000";
  if (id != kTestCastAppId) {
    LOG(ERROR) << "Unknown Cast app Id: " << id;
    callback(chromium::cast::ApplicationConfigPtr());
    return;
  }

  chromium::cast::ApplicationConfigPtr app_config =
      chromium::cast::ApplicationConfig::New();
  app_config->id = id;
  app_config->display_name = "Dummy test app";
  app_config->web_url = "https://www.google.com";
  callback(std::move(app_config));
}

}  // namespace castrunner
