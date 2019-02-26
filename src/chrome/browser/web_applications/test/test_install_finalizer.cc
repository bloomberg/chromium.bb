// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/test/test_install_finalizer.h"

#include "base/callback.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/web_application_info.h"

namespace web_app {

TestInstallFinalizer::TestInstallFinalizer() {}

TestInstallFinalizer::~TestInstallFinalizer() = default;

void TestInstallFinalizer::FinalizeInstall(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    InstallFinalizedCallback callback) {
  const AppId app_id = GenerateAppIdFromURL(web_app_info->app_url);

  web_app_info_ = std::move(web_app_info);
  std::move(callback).Run(app_id, InstallResultCode::kSuccess);
}

}  // namespace web_app
