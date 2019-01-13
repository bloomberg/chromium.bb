// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_APP_CAST_APPLICATION_CONFIG_MANAGER_APPLICATION_CONFIG_MANAGER_H_
#define WEBRUNNER_APP_CAST_APPLICATION_CONFIG_MANAGER_APPLICATION_CONFIG_MANAGER_H_

#include <webrunner/fidl/chromium/cast/cpp/fidl.h>

#include "base/macros.h"

namespace castrunner {

// Stub cast.ApplicationConfigManager implementation which maps a fixed set of
// Cast application Ids to configurations.
// TODO(crbug.com/914610): Have a real implementation for this class.
class ApplicationConfigManager
    : public chromium::cast::ApplicationConfigManager {
 public:
  ApplicationConfigManager();
  ~ApplicationConfigManager() override;

  // chromium::cast::ApplicationConfigManager interface.
  void GetConfig(std::string id, GetConfigCallback config_callback) override;

  DISALLOW_COPY_AND_ASSIGN(ApplicationConfigManager);
};

}  // namespace castrunner

#endif  // WEBRUNNER_APP_CAST_APPLICATION_CONFIG_MANAGER_APPLICATION_CONFIG_MANAGER_H_
